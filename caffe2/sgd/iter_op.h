#ifndef CAFFE2_SGD_ITER_OP_H_
#define CAFFE2_SGD_ITER_OP_H_

#include <limits>
#include <mutex>

#include "caffe2/core/blob_serialization.h"
#include "caffe2/core/context.h"
#include "caffe2/core/operator.h"

namespace caffe2 {

inline void IncrementIter(TensorCPU* output) {
  CAFFE_ENFORCE_EQ(
      output->size(),
      1,
      "The output of IterOp exists, but not of the right size.");
  int64_t* iter = output->template mutable_data<int64_t>();
  CAFFE_ENFORCE(*iter >= 0, "Previous iteration number is negative.");
  CAFFE_ENFORCE(
      *iter < std::numeric_limits<int64_t>::max(), "Overflow will happen!");
  (*iter)++;
}

// IterOp runs an iteration counter. I cannot think of a case where we would
// need to access the iter variable on device, so this will always produce a
// tensor on the CPU side. If the blob already exists and is a tensor<int64_t>
// object, we will simply increment it (this emulates the case when we want to
// resume training). Otherwise we will have the iter starting with 0.
template <class Context>
class IterOp final : public Operator<Context> {
 public:
  USE_OPERATOR_CONTEXT_FUNCTIONS;

  IterOp(const OperatorDef& operator_def, Workspace* ws)
      : Operator<Context>(operator_def, ws) {}

  bool RunOnDevice() override {
    if (InputSize() == 0) {
      if (!OperatorBase::OutputIsType<TensorCPU>(0)) {
        // This is the first run; set the iter to start with 0.
        LOG(ERROR) << "You are using an old definition of IterOp that will "
                      "be deprecated soon. More specifically, IterOp now "
                      "requires an explicit in-place input and output.";

        auto* output = OperatorBase::Output<TensorCPU>(0);
        VLOG(1) << "Initializing iter counter.";
        output->Resize(1);
        output->template mutable_data<int64_t>()[0] = 0;
      }
    }
    IncrementIter(OperatorBase::Output<TensorCPU>(0));
    return true;
  }
};

template <class Context>
class AtomicIterOp final : public Operator<Context> {
 public:
  USE_OPERATOR_CONTEXT_FUNCTIONS;

  AtomicIterOp(const OperatorDef& operator_def, Workspace* ws)
      : Operator<Context>(operator_def, ws) {}

  bool RunOnDevice() override {
    auto& mutex = OperatorBase::Input<std::unique_ptr<std::mutex>>(0);
    std::lock_guard<std::mutex> lg(*mutex);
    IncrementIter(OperatorBase::Output<TensorCPU>(0));
    return true;
  }
};

class MutexSerializer : public BlobSerializerBase {
 public:
  /**
   * Serializes a std::unique_ptr<std::mutex>. Note that this blob has to
   * contain std::unique_ptr<std::mutex>, otherwise this function produces a
   * fatal error.
   */
  void Serialize(
      const Blob& blob,
      const string& name,
      BlobSerializerBase::SerializationAcceptor acceptor) override;
};

class MutexDeserializer : public BlobDeserializerBase {
 public:
  void Deserialize(const BlobProto& proto, Blob* blob) override;
};

} // namespace caffe2

#endif // CAFFE2_SGD_ITER_OP_H_
