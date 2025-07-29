// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_impl_tflite.h"

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/queueable_resource_state_base.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/tflite/buffer_content_tflite.h"
#include "services/webnn/tflite/context_impl_tflite.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/tflite/op_resolver.h"
#include "services/webnn/tflite/tensor_impl_tflite.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/buildflags.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter_builder.h"
#include "third_party/tflite/src/tensorflow/lite/stderr_reporter.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_NNAPI)
#include "third_party/tflite/src/tensorflow/lite/core/c/c_api_types.h"
#include "third_party/tflite/src/tensorflow/lite/delegates/nnapi/nnapi_delegate.h"
#endif

#if BUILDFLAG(BUILD_TFLITE_WITH_OPENCL)
#include "third_party/tflite/src/tensorflow/lite/delegates/gpu/delegate.h"
#endif

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"
#include "third_party/xnnpack/src/include/xnnpack.h"  // nogncheck
#endif

#if BUILDFLAG(WEBNN_USE_CHROME_ML_API)
#include "services/on_device_model/ml/chrome_ml.h"      // nogncheck
#include "services/on_device_model/ml/chrome_ml_api.h"  // nogncheck
#endif

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
#include "third_party/tflite/src/tensorflow/lite/profiling/buffered_profiler.h"
#include "third_party/tflite/src/tensorflow/lite/profiling/profile_summarizer.h"
#endif

namespace webnn::tflite {

namespace {

void DumpModelToFile(const flatbuffers::DetachedBuffer& model_content) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(
          [](std::vector<uint8_t> data) {
            static uint64_t dump_count = 0;
            base::FilePath dump_directory =
                base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
                    switches::kWebNNTfliteDumpModel);
            base::FilePath dump_path = dump_directory.AppendASCII(
                base::StringPrintf("model%d.tflite", dump_count++));
            base::WriteFile(dump_path, data);
          },
          base::ToVector(model_content)));
}

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
         flatbuffers::DetachedBuffer buffer,
         std::vector<uint8_t> buffer_data,
         const base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
             constant_operands,
         bool graph_requires_fp32_precision) {
    auto self = std::make_unique<ComputeResources>();

    self->model_content_ = std::move(buffer);
    self->model_ = ::tflite::FlatBufferModel::BuildFromBuffer(
        reinterpret_cast<const char*>(self->model_content_.data()),
        self->model_content_.size(), ::tflite::DefaultErrorReporter());
    if (!self->model_) {
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            "Unable to build flatbuffer model"));
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWebNNTfliteDumpModel)) {
      DumpModelToFile(self->model_content_);
    }

    OpResolver op_resolver;

    self->model_weights_ = std::move(buffer_data);
    self->allocation_ = std::make_unique<::tflite::MemoryAllocation>(
        self->model_weights_.data(), self->model_weights_.size(),
        ::tflite::DefaultErrorReporter());

    ::tflite::InterpreterBuilder builder(
        self->model_->GetModel(), op_resolver, ::tflite::DefaultErrorReporter(),
        /*options=*/nullptr, self->allocation_.get());
    // On a lower-end system, use only one thread for 1 or 2 cores, use half
    // of the cores for less than 8 cores. On systems with more cores, the max
    // number threads is 4 to be used for inference.
    int num_of_threads =
        std::min(4, (base::SysInfo::NumberOfProcessors() + 1) / 2);
    builder.SetNumThreads(num_of_threads);
    self->SetUpDelegates(builder, context->options().device,
                         graph_requires_fp32_precision, num_of_threads);

    TfLiteStatus status = builder(&self->interpreter_);

    // If failed to build interpreter with delegates, re-build the interpreter
    // with just the XNNPack delegate, then try again with no delegate.
    if (status == kTfLiteDelegateError) {
      self->delegates_.clear();
      ::tflite::InterpreterBuilder builder_with_xnnpack(
          self->model_->GetModel(), op_resolver,
          ::tflite::DefaultErrorReporter(),
          /*options=*/nullptr, self->allocation_.get());
      builder_with_xnnpack.SetNumThreads(num_of_threads);
      self->SetUpXNNPackDelegate(builder_with_xnnpack, num_of_threads);
      status = builder_with_xnnpack(&self->interpreter_);
    }

    if (status == kTfLiteDelegateError) {
      self->delegates_.clear();
      ::tflite::InterpreterBuilder default_builder(
          self->model_->GetModel(), op_resolver,
          ::tflite::DefaultErrorReporter(),
          /*options=*/nullptr, self->allocation_.get());
      default_builder.SetNumThreads(num_of_threads);
      status = default_builder(&self->interpreter_);
    }

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

    absl::flat_hash_set<mojom::Device> devices;
    for (int i : self->interpreter_->execution_plan()) {
      const auto& [node, registration] =
          *self->interpreter_->node_and_registration(i);
      // Delegate is nullptr if it's the default delegate.
      if (!node.delegate) {
        devices.insert(mojom::Device::kCpu);
      } else {
        auto result = std::ranges::find(
            self->delegates_, node.delegate,
            [](const DelegateInfo& info) { return info.delegate.get(); });
        CHECK(result != self->delegates_.end());
        devices.insert(result->device);
      }
    }
    self->devices.assign(devices.begin(), devices.end());

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

  void DoDispatch(base::flat_map<int, raw_ref<const BufferContent>> tensors,
                  ScopedTrace scoped_trace) {
    scoped_trace.AddStep("Set up intepreter");

    // TODO: Detect when `tensors` hasn't changed since the last invocation and
    // this step can be skipped.
    bool needs_reallocate_tensors = false;
    for (auto& [tensor_idx, buffer] : tensors) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      if (tensor->allocation_type == kTfLitePersistentRo) {
        // The initial `AllocateTensors()` call has marked this output as a
        // constant. It cannot be replaced with a custom allocation.
        continue;
      }

      base::span<uint8_t> data = buffer->AsSpan();
      TfLiteStatus status = interpreter_->SetCustomAllocationForTensor(
          tensor_idx, {data.data(), data.size()});
      if (status != kTfLiteOk) {
        LOG(ERROR) << "Unable set custom tensor allocation: "
                   << TfLiteStatusToString(status);
        return;
      }
      needs_reallocate_tensors = true;
    }

    if (needs_reallocate_tensors) {
      TfLiteStatus status = interpreter_->AllocateTensors();
      if (status != kTfLiteOk) {
        LOG(ERROR) << "Unable to allocate tensors: "
                   << TfLiteStatusToString(status);
        return;
      }
    }

    scoped_trace.AddStep("Run inference");
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    profiler_.StartProfiling();
#endif
    TfLiteStatus status = interpreter_->Invoke();
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    profiler_.StopProfiling();
#endif

    if (status != kTfLiteOk) {
      LOG(ERROR) << "Failed to compute: " << TfLiteStatusToString(status);
      return;
    }

    // Copy the outputs that weren't configured as custom allocations.
    scoped_trace.AddStep("Process outputs");
    for (int tensor_idx : interpreter_->outputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      if (tensor->allocation_type == kTfLitePersistentRo) {
        tensors.at(tensor_idx)->AsSpan().copy_from(SpanFromTensor(tensor));
      }
    }
  }

  base::flat_map<int, raw_ref<const BufferContent>> CollectBuffersForDispatch(
      const base::flat_map<
          int,
          scoped_refptr<QueueableResourceState<BufferContent>>>& inputs,
      const base::flat_map<
          int,
          scoped_refptr<QueueableResourceState<BufferContent>>>& outputs) {
    std::vector<std::pair<int, raw_ref<const BufferContent>>> buffers;
    buffers.reserve(inputs.size() + outputs.size());

    for (const auto& [tensor_idx, buffer] : inputs) {
      buffers.emplace_back(tensor_idx, buffer->GetSharedLockedResource());
    }
    for (const auto& [tensor_idx, buffer] : outputs) {
      buffers.emplace_back(tensor_idx, *buffer->GetExclusivelyLockedResource());
    }

    return buffers;
  }
  std::vector<mojom::Device> devices;

 private:
  using TfLiteDelegatePtr =
      std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)>;

  struct DelegateInfo {
    DelegateInfo(
        std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)> delegate,
        mojom::Device device)
        : delegate(std::move(delegate)), device(device) {}
    ~DelegateInfo() = default;

    DelegateInfo(const DelegateInfo&) = delete;
    DelegateInfo& operator=(const DelegateInfo&) = delete;

    DelegateInfo(DelegateInfo&&) = default;
    DelegateInfo& operator=(DelegateInfo&&) = default;

    TfLiteDelegatePtr delegate;
    mojom::Device device;
  };

  void SetUpDelegates(::tflite::InterpreterBuilder& builder,
                      mojom::Device context_device,
                      bool graph_requires_fp32_precision,
                      int num_of_threads) {
#if BUILDFLAG(BUILD_TFLITE_WITH_NNAPI)
    if (context_device == mojom::Device::kNpu) {
      TfLiteDelegate* delegate = new ::tflite::StatefulNnApiDelegate();
      builder.AddDelegate(delegate);
      delegates_.emplace_back(
          TfLiteDelegatePtr(
              delegate,
              [](TfLiteDelegate* delegate) {
                // Cast `delegate` back to a C++ object type so that the correct
                // destructor is invoked.
                delete static_cast<::tflite::StatefulNnApiDelegate*>(delegate);
              }),
          mojom::Device::kNpu);
    }
#endif

    if (context_device == mojom::Device::kGpu) {
#if BUILDFLAG(WEBNN_USE_CHROME_ML_API)
      // TODO(crbug.com/394119734): Simplify this check once these functions are
      // always available.
      auto* chrome_ml = ml::ChromeML::Get();
      if (chrome_ml && chrome_ml->api().CreateGpuDelegate &&
          chrome_ml->api().DestroyGpuDelegate) {
        GpuDelegatePrecision precision = GpuDelegatePrecision::kFp16;
        if (graph_requires_fp32_precision) {
          precision = GpuDelegatePrecision::kFp32;
        }
        TfLiteDelegate* delegate =
            ml::ChromeML::Get()->api().CreateGpuDelegateWithPrecision(
                precision);
        builder.AddDelegate(delegate);
        delegates_.emplace_back(
            TfLiteDelegatePtr(delegate,
                              [](TfLiteDelegate* delegate) {
                                ml::ChromeML::Get()->api().DestroyGpuDelegate(
                                    delegate);
                              }),
            mojom::Device::kGpu);
      }

#elif BUILDFLAG(BUILD_TFLITE_WITH_OPENCL)
      TfLiteDelegate* delegate = TfLiteGpuDelegateV2Create(nullptr);
      builder.AddDelegate(delegate);
      delegates_.emplace_back(
          TfLiteDelegatePtr(delegate, TfLiteGpuDelegateV2Delete),
          mojom::Device::kGpu);
#endif
    }

    SetUpXNNPackDelegate(builder, num_of_threads);
  }

  void SetUpXNNPackDelegate(::tflite::InterpreterBuilder& builder,
                            int num_of_threads) {
#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
    auto opts = TfLiteXNNPackDelegateOptionsDefault();
    opts.num_threads = num_of_threads;
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    opts.runtime_flags = XNN_FLAG_BASIC_PROFILING;
#endif
    TfLiteDelegate* delegate = TfLiteXNNPackDelegateCreate(&opts);
    builder.AddDelegate(delegate);
    delegates_.emplace_back(
        TfLiteDelegatePtr(delegate, TfLiteXNNPackDelegateDelete),
        mojom::Device::kCpu);
#endif
  }

  flatbuffers::DetachedBuffer model_content_;
  std::vector<uint8_t> model_weights_;
  std::vector<DelegateInfo> delegates_;

  // `model_` depends on `model_content_` outliving it.
  std::unique_ptr<::tflite::FlatBufferModel> model_;

  // `allocation_` depends on `model_weights_` outliving it.
  std::unique_ptr<::tflite::Allocation> allocation_;

  // `interpreter_` depends on `model_`, `allocation_`, and `delegates_`
  // outliving it.
  std::unique_ptr<::tflite::Interpreter> interpreter_;

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
  ::tflite::profiling::BufferedProfiler profiler_{/*max_num_entries=*/1024};
#endif
};

// static
base::expected<scoped_refptr<GraphImplTflite>, mojom::ErrorPtr>
GraphImplTflite::CreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    ContextImplTflite* context) {
  ASSIGN_OR_RETURN(GraphBuilderTflite::Result result,
                   GraphBuilderTflite::CreateAndBuild(
                       context->properties(), *graph_info, constant_operands,
                       compute_resource_info.operand_to_dependent_operations,
                       compute_resource_info.operand_to_producing_operation),
                   [](std::string error) {
                     return mojom::Error::New(
                         mojom::Error::Code::kNotSupportedError,
                         std::move(error));
                   });

  ASSIGN_OR_RETURN(
      std::unique_ptr<ComputeResources> compute_resources,
      ComputeResources::Create(context, std::move(result.buffer),
                               std::move(result.buffer_data), constant_operands,
                               result.graph_requires_fp32_precision));
  auto devices = std::move(compute_resources->devices);
  auto compute_resources_state =
      base::MakeRefCounted<QueueableResourceState<ComputeResources>>(
          std::move(compute_resources));
  return base::MakeRefCounted<GraphImplTflite>(
      std::move(receiver), std::move(compute_resource_info),
      std::move(result.input_name_to_index),
      std::move(result.output_name_to_index),
      std::move(compute_resources_state), context->AsWeakPtr(),
      std::move(devices));
}

GraphImplTflite::~GraphImplTflite() = default;

GraphImplTflite::GraphImplTflite(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<std::string, int> input_name_to_index,
    base::flat_map<std::string, int> output_name_to_index,
    scoped_refptr<QueueableResourceState<ComputeResources>>
        compute_resources_state,
    base::WeakPtr<WebNNContextImpl> context,
    std::vector<mojom::Device> devices)
    : WebNNGraphImpl(std::move(receiver),
                     std::move(context),
                     std::move(compute_resource_info),
                     std::move(devices)),
      compute_resources_state_(std::move(compute_resources_state)),
      input_name_to_index_(std::move(input_name_to_index)),
      output_name_to_index_(std::move(output_name_to_index)) {}

void GraphImplTflite::DispatchImpl(
    const base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        named_inputs,
    const base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        named_outputs) {
  ScopedTrace scoped_trace("GraphImplTflite::DispatchImpl");

  std::vector<
      std::pair<int, scoped_refptr<QueueableResourceState<BufferContent>>>>
      input_buffer_states, output_buffer_states;
  input_buffer_states.reserve(named_inputs.size());
  output_buffer_states.reserve(named_outputs.size());

  // The caller guarantees that all expected tensors have been provided.
  for (const auto& [name, tensor] : named_inputs) {
    auto* tflite_tensor = static_cast<TensorImplTflite*>(tensor.get());
    input_buffer_states.emplace_back(input_name_to_index_.at(name),
                                     tflite_tensor->GetBufferState());
  }
  for (const auto& [name, tensor] : named_outputs) {
    auto* tflite_tensor = static_cast<TensorImplTflite*>(tensor.get());
    output_buffer_states.emplace_back(output_name_to_index_.at(name),
                                      tflite_tensor->GetBufferState());
  }

  // Input tensors will be read from while the graph is executing, so lock them
  // them as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources;
  shared_resources.reserve(named_inputs.size());
  for (const auto& [name, buffer_state] : input_buffer_states) {
    shared_resources.push_back(buffer_state);
  }

  // Exclusively reserve all output tensors - which will be written to - and
  // this graph's compute resources while the graph is executing.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources;
  // Extra +1 is for the compute resources.
  exclusive_resources.reserve(1 + named_outputs.size());
  exclusive_resources.push_back(compute_resources_state_);
  for (const auto& [name, buffer_state] : output_buffer_states) {
    exclusive_resources.push_back(buffer_state);
  }

  scoped_trace.AddStep("Acquire resources");
  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources), std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<ComputeResources>>
                 compute_resources_state,
             base::flat_map<
                 int, scoped_refptr<QueueableResourceState<BufferContent>>>
                 input_buffer_states,
             base::flat_map<
                 int, scoped_refptr<QueueableResourceState<BufferContent>>>
                 output_buffer_states,
             ScopedTrace scoped_trace, base::OnceClosure completion_closure) {
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
                    base::Unretained(raw_compute_resources), std::move(buffers),
                    std::move(scoped_trace)),
                std::move(completion_closure));
          },
          compute_resources_state_, std::move(input_buffer_states),
          std::move(output_buffer_states), std::move(scoped_trace)));
  task->Enqueue();
}

}  // namespace webnn::tflite
