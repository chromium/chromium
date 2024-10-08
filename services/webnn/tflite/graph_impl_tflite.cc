// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_impl_tflite.h"

#include "base/containers/flat_map.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/queueable_resource_state_base.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/tflite/buffer_content.h"
#include "services/webnn/tflite/context_impl_tflite.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/tflite/op_resolver.h"
#include "services/webnn/tflite/tensor_impl_tflite.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter_builder.h"
#include "third_party/tflite/src/tensorflow/lite/stderr_reporter.h"

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
#include "third_party/tflite/src/tensorflow/lite/profiling/buffered_profiler.h"
#include "third_party/tflite/src/tensorflow/lite/profiling/profile_summarizer.h"
#endif

namespace webnn::tflite {

namespace {

std::string_view TfLiteStatusToString(TfLiteStatus status) {
  switch (status) {
    case kTfLiteOk:
      return "ok";
    case kTfLiteError:
      return "error";
    case kTfLiteDelegateError:
      return "delegate error";
    case kTfLiteApplicationError:
      return "application error";
    case kTfLiteDelegateDataNotFound:
      return "delegate data not found";
    case kTfLiteDelegateDataWriteError:
      return "delegate data write error";
    case kTfLiteDelegateDataReadError:
      return "delegate data read error";
    case kTfLiteUnresolvedOps:
      return "unresolved ops";
    case kTfLiteCancelled:
      return "cancelled";
    case kTfLiteOutputShapeNotKnown:
      return "output shape not known";
  }
}

base::span<uint8_t> SpanFromTensor(TfLiteTensor* tensor) {
  // SAFETY: TFLite guarantees that it has allocated enough memory to
  // store `tensor`.
  return UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(tensor->data.data), tensor->bytes));
}

}  // namespace

// Represents the non-thread-safe collection of resources associated with a
// particular graph and compute context (i.e. a TFLite interpreter).
class GraphImplTflite::ComputeResources {
 public:
  static base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
  Create(WebNNContextImpl* context,
         const mojom::GraphInfo& graph_info,
         const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
             constant_operands) {
    auto self = std::make_unique<ComputeResources>();

    ASSIGN_OR_RETURN(
        self->model_content_,
        GraphBuilderTflite::CreateAndBuild(context->properties(), graph_info,
                                           constant_operands),
        [](std::string error) {
          return mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                                   std::move(error));
        });

    self->model_ = ::tflite::FlatBufferModel::BuildFromBuffer(
        reinterpret_cast<const char*>(self->model_content_.data()),
        self->model_content_.size(), ::tflite::DefaultErrorReporter());
    if (!self->model_) {
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            "Unable to build flatbuffer model"));
    }

    int num_threads =
        context->options().thread_count_hint != 0
            ? static_cast<int>(context->options().thread_count_hint)
            : -1;  // Let the TFLite runtime decide.

    OpResolver op_resolver(context->options());
    ::tflite::InterpreterBuilder builder(*self->model_, op_resolver);
    builder.SetNumThreads(num_threads);
    TfLiteStatus status = builder(&self->interpreter_);
    if (status != kTfLiteOk) {
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            base::StrCat({"Unable to build TFLite intepreter: ",
                                          TfLiteStatusToString(status)})));
    }

    // The profiler (if enabled) must be initialized before tensors are
    // allocated.
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    self->interpreter_->SetProfiler(&self->profiler_);
#endif

    // In addition to allocating tensors this step does performs graph
    // initialization steps such as constant folding.
    status = self->interpreter_->AllocateTensors();
    if (status != kTfLiteOk) {
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            base::StrCat({"Unable to allocate tensors: ",
                                          TfLiteStatusToString(status)})));
    }

    return self;
  }

  ComputeResources() = default;

  ~ComputeResources() {
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    if (interpreter_) {
      ::tflite::profiling::ProfileSummarizer profile_summarizer;
      auto profile_events = profiler_.GetProfileEvents();
      profile_summarizer.ProcessProfiles(profile_events, *interpreter_);
      LOG(INFO) << profile_summarizer.GetOutputString();
      interpreter_->SetProfiler(nullptr);
    }
#endif
  }

  mojom::ComputeResultPtr DoCompute(NamedBuffers named_inputs) {
    InitializeBuffersForCompute();

    for (int tensor_idx : interpreter_->inputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      auto it = named_inputs.find(tensor->name);
      // The caller guarantees that all expected tensors have been provided.
      CHECK(it != named_inputs.end());
      compute_buffers_.at(tensor_idx)->AsSpan().copy_from(it->second);
    }

    std::vector<std::pair<int, raw_ref<const BufferContent>>> indexed_buffers;
    indexed_buffers.reserve(compute_buffers_.size());
    base::ranges::transform(
        compute_buffers_, std::back_inserter(indexed_buffers),
        [](const auto& index_and_buffer) {
          return std::make_pair(index_and_buffer.first,
                                raw_ref(*index_and_buffer.second));
        });

    TfLiteStatus status = InvokeInterpreter(std::move(indexed_buffers));
    if (status != kTfLiteOk) {
      return ToError<mojom::ComputeResult>(
          mojom::Error::Code::kUnknownError,
          base::StrCat({"Failed to compute: ", TfLiteStatusToString(status)}));
    }

    std::vector<std::pair<std::string, mojo_base::BigBuffer>> named_outputs;
    named_outputs.reserve(interpreter_->outputs().size());
    for (int tensor_idx : interpreter_->outputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      // Uses `SpanFromTensor()` because `tensor` may or may not be backed by
      // one of our custom allocations.
      named_outputs.emplace_back(tensor->name,
                                 mojo_base::BigBuffer(SpanFromTensor(tensor)));
    }

    return mojom::ComputeResult::NewNamedOutputs(std::move(named_outputs));
  }

  void DoDispatch(base::flat_map<int, raw_ref<const BufferContent>> tensors) {
    TfLiteStatus status = InvokeInterpreter(tensors);
    if (status != kTfLiteOk) {
      LOG(ERROR) << "Failed to compute: " << TfLiteStatusToString(status);
      return;
    }

    // Copy the outputs that weren't configured as custom allocations.
    for (int tensor_idx : interpreter_->outputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      if (tensor->allocation_type == kTfLitePersistentRo) {
        tensors.at(tensor_idx)->AsSpan().copy_from(SpanFromTensor(tensor));
      }
    }
  }

  TfLiteStatus InvokeInterpreter(
      const base::flat_map<int, raw_ref<const BufferContent>>& tensors) {
    TfLiteStatus status;
    bool needs_reallocate_tensors = false;

    // TODO: Detect when `tensors` hasn't changed since the last invocation and
    // this step can be skipped.
    for (auto& [tensor_idx, buffer] : tensors) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      if (tensor->allocation_type == kTfLitePersistentRo) {
        // The initial `AllocateTensors()` call has marked this output as a
        // constant. It cannot be replaced with a custom allocation.
        continue;
      }

      base::span<uint8_t> data = buffer->AsSpan();
      status = interpreter_->SetCustomAllocationForTensor(
          tensor_idx, {data.data(), data.size()});
      if (status != kTfLiteOk) {
        LOG(ERROR) << "Unable set custom tensor allocation: "
                   << TfLiteStatusToString(status);
        return status;
      }
      needs_reallocate_tensors = true;
    }

    if (needs_reallocate_tensors) {
      status = interpreter_->AllocateTensors();
      if (status != kTfLiteOk) {
        LOG(ERROR) << "Unable to allocate tensors: "
                   << TfLiteStatusToString(status);
        return status;
      }
    }

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    profiler_.StartProfiling();
#endif
    status = interpreter_->Invoke();
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    profiler_.StopProfiling();
#endif

    return status;
  }

  base::flat_map<int, raw_ref<const BufferContent>> CollectBuffersForDispatch(
      const base::flat_map<
          std::string,
          scoped_refptr<QueueableResourceState<BufferContent>>>& named_inputs,
      const base::flat_map<
          std::string,
          scoped_refptr<QueueableResourceState<BufferContent>>>&
          named_outputs) {
    std::vector<std::pair<int, raw_ref<const BufferContent>>> buffers;

    buffers.reserve(named_inputs.size() + named_outputs.size());

    for (int tensor_idx : interpreter_->inputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      auto it = named_inputs.find(tensor->name);
      // The caller guarantees that all expected tensors have been provided.
      CHECK(it != named_inputs.end());
      buffers.emplace_back(tensor_idx, it->second->GetSharedLockedResource());
    }

    for (int tensor_idx : interpreter_->outputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      auto it = named_outputs.find(tensor->name);
      // The caller guarantees that all expected tensors have been provided.
      CHECK(it != named_outputs.end());
      buffers.emplace_back(tensor_idx,
                           *it->second->GetExclusivelyLockedResource());
    }

    return buffers;
  }

 private:
  void InitializeBuffersForCompute() {
    if (compute_buffers_.size() > 0) {
      return;
    }

    std::vector<std::pair<int, std::unique_ptr<BufferContent>>> buffers;
    buffers.reserve(interpreter_->inputs().size() +
                    interpreter_->outputs().size());

    for (int tensor_idx : interpreter_->inputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      buffers.emplace_back(tensor_idx,
                           std::make_unique<BufferContent>(tensor->bytes));
    }

    for (int tensor_idx : interpreter_->outputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      if (tensor->allocation_type == kTfLitePersistentRo) {
        // The initial `AllocateTensors()` call has marked this output as a
        // constant. It cannot be replaced with a custom allocation.
        continue;
      }

      buffers.emplace_back(tensor_idx,
                           std::make_unique<BufferContent>(tensor->bytes));
    }

    compute_buffers_ =
        base::flat_map<int, std::unique_ptr<BufferContent>>(std::move(buffers));
  }

  flatbuffers::DetachedBuffer model_content_;

  // `model_` depends on `model_content_` outliving it.
  std::unique_ptr<::tflite::FlatBufferModel> model_;

  // `interpreter_` depends on `model_` outliving it.
  std::unique_ptr<::tflite::Interpreter> interpreter_;

  // Input and output buffers used for compute().
  base::flat_map<int, std::unique_ptr<BufferContent>> compute_buffers_;

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
  ::tflite::profiling::BufferedProfiler profiler_{/*max_num_entries=*/1024};
#endif
};

// static
base::expected<std::unique_ptr<GraphImplTflite>, mojom::ErrorPtr>
GraphImplTflite::CreateAndBuild(
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ContextImplTflite* context) {
  ASSIGN_OR_RETURN(
      std::unique_ptr<ComputeResources> compute_resources,
      ComputeResources::Create(context, *graph_info, constant_operands));

  auto compute_resources_state =
      base::MakeRefCounted<QueueableResourceState<ComputeResources>>(
          std::move(compute_resources));
  return base::WrapUnique(
      new GraphImplTflite(std::move(compute_resource_info),
                          std::move(compute_resources_state), context));
}

GraphImplTflite::~GraphImplTflite() = default;

GraphImplTflite::GraphImplTflite(
    ComputeResourceInfo compute_resource_info,
    scoped_refptr<QueueableResourceState<ComputeResources>>
        compute_resources_state,
    ContextImplTflite* context)
    : WebNNGraphImpl(context, std::move(compute_resource_info)),
      compute_resources_state_(std::move(compute_resources_state)) {}

void GraphImplTflite::ComputeImpl(NamedBuffers named_inputs,
                                  ComputeCallback callback) {
  // Exclusively reserve this graph's compute resources while the graph is
  // executing.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources = {
      compute_resources_state_};

  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/std::vector<
          scoped_refptr<QueueableResourceStateBase>>(),
      std::move(exclusive_resources),
      base::BindOnce(
          [](NamedBuffers named_inputs,
             scoped_refptr<QueueableResourceState<ComputeResources>>
                 compute_resources_state,
             ComputeCallback callback, base::OnceClosure completion_closure) {
            ComputeResources* raw_compute_resources =
                compute_resources_state->GetExclusivelyLockedResource();

            base::ThreadPool::PostTaskAndReplyWithResult(
                FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
                base::BindOnce(
                    &ComputeResources::DoCompute,
                    // Unretained is safe here because a reference to
                    // a `QueueableResourceState` corresponding to
                    // `raw_compute_resources` is held by the
                    // `ResourceTask` until `completion_closure` is run below.
                    base::Unretained(raw_compute_resources),
                    std::move(named_inputs)),
                std::move(callback).Then(std::move(completion_closure)));
          },
          std::move(named_inputs), compute_resources_state_,
          std::move(callback)));
  task->Enqueue();
}

void GraphImplTflite::DispatchImpl(
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs) {
  std::vector<std::pair<std::string,
                        scoped_refptr<QueueableResourceState<BufferContent>>>>
      named_input_buffer_states, named_output_buffer_states;
  named_input_buffer_states.reserve(named_inputs.size());
  named_output_buffer_states.reserve(named_outputs.size());

  for (const auto& [name, tensor] : named_inputs) {
    named_input_buffer_states.emplace_back(
        name, static_cast<TensorImplTflite*>(tensor)->GetBufferState());
  }
  for (const auto& [name, tensor] : named_outputs) {
    named_output_buffer_states.emplace_back(
        name, static_cast<TensorImplTflite*>(tensor)->GetBufferState());
  }

  // Input tensors will be read from while the graph is executing, so lock them
  // them as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources;
  shared_resources.reserve(named_inputs.size());
  for (const auto& [name, buffer_state] : named_input_buffer_states) {
    shared_resources.push_back(buffer_state);
  }

  // Exclusively reserve all output tensors - which will be written to - and
  // this graph's compute resources while the graph is executing.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources;
  // Extra +1 is for the compute resources.
  exclusive_resources.reserve(1 + named_outputs.size());
  exclusive_resources.push_back(compute_resources_state_);
  for (const auto& [name, buffer_state] : named_output_buffer_states) {
    exclusive_resources.push_back(buffer_state);
  }

  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources), std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<ComputeResources>>
                 compute_resources_state,
             base::flat_map<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContent>>>
                 input_buffer_states,
             base::flat_map<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContent>>>
                 output_buffer_states,
             base::OnceClosure completion_closure) {
            ComputeResources* raw_compute_resources =
                compute_resources_state->GetExclusivelyLockedResource();

            base::flat_map<int, raw_ref<const BufferContent>> buffers =
                raw_compute_resources->CollectBuffersForDispatch(
                    input_buffer_states, output_buffer_states);

            // Compute tasks can take a significant amount of time, use the
            // thread pool to avoid blocking the main thread.
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE,
                base::BindOnce(
                    &ComputeResources::DoDispatch,
                    // Unretained is safe here because a reference to
                    // a `QueueableResourceState` corresponding to
                    // `raw_compute_resources` is held by the
                    // `ResourceTask` until `completion_closure` is run below.
                    base::Unretained(raw_compute_resources),
                    std::move(buffers)),
                std::move(completion_closure));
          },
          compute_resources_state_, std::move(named_input_buffer_states),
          std::move(named_output_buffer_states)));
  task->Enqueue();
}

}  // namespace webnn::tflite
