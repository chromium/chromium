// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_impl_litert.h"

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
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
#include "services/webnn/tflite/context_impl_litert.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/tflite/tensor_impl_tflite.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/litert/src/litert/c/litert_common.h"
#include "third_party/litert/src/litert/cc/litert_compiled_model.h"
#include "third_party/litert/src/litert/cc/litert_element_type.h"
#include "third_party/litert/src/litert/cc/litert_environment.h"
#include "third_party/litert/src/litert/cc/litert_expected.h"
#include "third_party/litert/src/litert/cc/litert_layout.h"
#include "third_party/litert/src/litert/cc/litert_model.h"
#include "third_party/litert/src/litert/cc/litert_options.h"
#include "third_party/litert/src/litert/cc/litert_ranked_tensor_type.h"
#include "third_party/litert/src/litert/cc/litert_tensor_buffer.h"
#include "third_party/litert/src/litert/cc/options/litert_gpu_options.h"
// TODO(crbug.com/454732289): Create new build flags for litert instead of
// reusing tflite build flags.
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/litert/src/tflite/delegates/xnnpack/xnnpack_delegate.h"
#include "third_party/xnnpack/src/include/xnnpack.h"  // nogncheck
#endif

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
#include "third_party/litert/src/litert/cc/litert_profiler.h"
#endif

namespace webnn::litert {

namespace {

using ::webnn::tflite::BufferContent;
using ::webnn::tflite::TensorDescriptor;

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

::litert::ElementType GetLiteRtElementType(OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return ::litert::ElementType::Float32;
    case OperandDataType::kFloat16:
      return ::litert::ElementType::Float16;
    case OperandDataType::kInt32:
      return ::litert::ElementType::Int32;
    case OperandDataType::kUint32:
      return ::litert::ElementType::UInt32;
    case OperandDataType::kInt64:
      return ::litert::ElementType::Int64;
    case OperandDataType::kUint64:
      return ::litert::ElementType::UInt64;
    case OperandDataType::kInt8:
      return ::litert::ElementType::Int8;
    case OperandDataType::kUint8:
      return ::litert::ElementType::UInt8;
    case OperandDataType::kInt4:
      return ::litert::ElementType::Int4;
    default:
      return ::litert::ElementType::None;
  }
}

template <typename T>
base::expected<T, mojom::ErrorPtr> AsBaseExpected(
    ::litert::Expected<T> result) {
  if (result.HasValue()) {
    return std::move(result.Value());
  }
  return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                            result.Error().Message()));
}

}  // namespace

// Represents the non-thread-safe collection of resources associated with a
// particular graph and compute context.
class GraphImplLiteRt::ComputeResources {
 public:
  static base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
  Create(mojom::Device context_device,
         tflite::GraphBuilderTflite::Result build_graph_result) {
    auto self = std::make_unique<ComputeResources>(
        std::move(build_graph_result.input_name_to_descriptor),
        std::move(build_graph_result.output_name_to_descriptor));

    self->model_content_ = std::move(build_graph_result.buffer);
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWebNNTfliteDumpModel)) {
      DumpModelToFile(self->model_content_);
    }

    ASSIGN_OR_RETURN(
        ::litert::Options compilation_options,
        self->GetCompilationOptions(
            context_device, build_graph_result.graph_requires_fp32_precision));

    // TODO(crbug.com/454732289): Update to use ScopedFile and
    // ScopedWeightSectionMap once external weight loader is fully supported in
    // LiteRT.
    // TODO(Update):lyjiang
    // self->weights_file_ = std::make_unique<::litert::ScopedFile>(
    //     build_graph_result.weights_file.TakePlatformFile());
    // compilation_options.SetExternalWeightScopedFile(
    //     *self->weights_file_,
    //     std::move(build_graph_result.weights_section_map));

    ASSIGN_OR_RETURN(self->env_,
                     AsBaseExpected(::litert::Environment::Create({})));

    ASSIGN_OR_RETURN(
        self->model_,
        AsBaseExpected(::litert::CompiledModel::Create(
            *self->env_,
            ::litert::BufferRef<uint8_t>(absl::MakeSpan(self->model_content_)),
            compilation_options)));

    // The profiler (if enabled) must be initialized before tensors are
    // allocated.
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    ASSIGN_OR_RETURN(self->profiler_,
                     AsBaseExpected(self->model_->GetProfiler()));
#endif
    // TODO(crbug.com/454732289): LiteRT currently provides no API to query
    // runtime accelerators. As a temporary workaround we infer the target
    // devices from the compilation options when the model is fully
    // accelerated. Replace this inference with the official LiteRT API once
    // it becomes available.
    if (self->model_->IsFullyAccelerated()) {
      ASSIGN_OR_RETURN(
          auto hardware_accelerators,
          AsBaseExpected(compilation_options.GetHardwareAccelerators()));
      if (hardware_accelerators & kLiteRtHwAcceleratorGpu) {
        self->devices.push_back(mojom::Device::kGpu);
      }
      if (hardware_accelerators & kLiteRtHwAcceleratorNpu) {
        self->devices.push_back(mojom::Device::kNpu);
      }
      if (hardware_accelerators & kLiteRtHwAcceleratorCpu) {
        self->devices.push_back(mojom::Device::kCpu);
      }
    } else {
      self->devices.push_back(mojom::Device::kCpu);
    }

    return self;
  }

  ComputeResources(std::vector<std::pair<std::string, TensorDescriptor>>
                       input_name_to_descriptor,
                   std::vector<std::pair<std::string, TensorDescriptor>>
                       output_name_to_descriptor)
      : input_name_to_descriptor(std::move(input_name_to_descriptor)),
        output_name_to_descriptor(std::move(output_name_to_descriptor)) {}

  ~ComputeResources() {
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    auto profile_summary = profiler_.GetProfileSummary(model_->Get());
    if (profile_summary) {
      VLOG(1) << "LiteRt Profiler Summary:\n" << *profile_summary;
    } else {
      VLOG(1) << "Failed to get LiteRt profiler summary: "
              << profile_summary.Error().Message();
    }
#endif
  }

  void DoDispatch(
      const std::vector<std::pair<std::string, TensorDescriptor>>& inputs,
      const std::vector<std::pair<std::string, TensorDescriptor>>& outputs,
      base::flat_map<int, raw_ref<const BufferContent>> buffers,
      ScopedTrace scoped_trace) {
    scoped_trace.AddStep("Set up input and output buffers");

    std::vector<::litert::TensorBuffer> input_buffers;
    input_buffers.reserve(inputs.size());
    for (const auto& [name, input] : inputs) {
      auto tensor_type = ::litert::RankedTensorType(
          GetLiteRtElementType(input.descriptor.data_type()),
          ::litert::Layout(
              ::litert::Dimensions(input.descriptor.shape().begin(),
                                   input.descriptor.shape().end())));
      base::span<uint8_t> data = buffers.at(input.tensor_index)->AsSpan();
      auto litert_buffer_or = ::litert::TensorBuffer::CreateFromHostMemory(
          *env_, tensor_type, data.data(), data.size());
      if (!litert_buffer_or) {
        LOG(ERROR) << "Failed to create input litert buffer: "
                   << litert_buffer_or.Error().Message();
        return;
      }
      input_buffers.push_back(std::move(*litert_buffer_or));
    }

    std::vector<::litert::TensorBuffer> output_buffers;
    output_buffers.reserve(outputs.size());
    for (const auto& [name, output] : outputs) {
      auto tensor_type = ::litert::RankedTensorType(
          GetLiteRtElementType(output.descriptor.data_type()),
          ::litert::Layout(
              ::litert::Dimensions(output.descriptor.shape().begin(),
                                   output.descriptor.shape().end())));
      base::span<uint8_t> data = buffers.at(output.tensor_index)->AsSpan();
      auto litert_buffer_or = ::litert::TensorBuffer::CreateFromHostMemory(
          *env_, tensor_type, data.data(), data.size());
      if (!litert_buffer_or) {
        LOG(ERROR) << "Failed to create output litert buffer: "
                   << litert_buffer_or.Error().Message();
        return;
      }
      output_buffers.push_back(std::move(*litert_buffer_or));
    }

    scoped_trace.AddStep("Run inference");
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    profiler_.StartProfiling();
#endif
    auto status = model_->Run(input_buffers, output_buffers);
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    profiler_.StopProfiling();
#endif

    if (!status) {
      LOG(ERROR) << "Failed to compute: " << status.Error().Message();
      return;
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

  // Used for getting queueable input/output resources.
  std::vector<std::pair<std::string, TensorDescriptor>>
      input_name_to_descriptor;
  std::vector<std::pair<std::string, TensorDescriptor>>
      output_name_to_descriptor;

 private:
  base::expected<::litert::Options, mojom::ErrorPtr> GetCompilationOptions(
      mojom::Device context_device,
      bool graph_requires_fp32_precision) {
    auto options = ::litert::Options::Create();
    if (!options) {
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            base::StringPrintf("Unable to create Options: %s",
                                               options.Error().Message())));
    }
    ::litert::HwAcceleratorSet accelerators(::litert::HwAccelerators::kNone);

    // TODO(crbug.com/454732289): Support NPU accelerator.
    if (context_device == mojom::Device::kNpu) {
      accelerators |= ::litert::HwAccelerators::kNpu;
    }

    if (context_device == mojom::Device::kGpu) {
      accelerators |= ::litert::HwAccelerators::kGpu;
      auto gpu_options = options->GetGpuOptions();
      if (!gpu_options) {
        return base::unexpected(mojom::Error::New(
            mojom::Error::Code::kUnknownError,
            base::StringPrintf("Unable to create GPU Options: %s",
                               gpu_options.Error().Message())));
      }
      gpu_options->SetPrecision(graph_requires_fp32_precision
                                    ? ::litert::GpuOptions::Precision::kFp32
                                    : ::litert::GpuOptions::Precision::kFp16);
    }
#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
    accelerators |= ::litert::HwAccelerators::kCpu;
    auto cpu_options = options->GetCpuOptions();
    if (!cpu_options) {
      return base::unexpected(mojom::Error::New(
          mojom::Error::Code::kUnknownError,
          base::StringPrintf("Unable to create CPU Options: %s",
                             cpu_options.Error().Message())));
    }
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    cpu_options->SetXNNPackFlags(XNN_FLAG_BASIC_PROFILING);
    auto runtime_options = options->GetRuntimeOptions();
    if (!runtime_options) {
      return base::unexpected(mojom::Error::New(
          mojom::Error::Code::kUnknownError,
          base::StringPrintf("Unable to create Runtime Options: %s",
                             runtime_options.Error().Message())));
    }
    runtime_options->SetEnableProfiling(true);
#endif
    // On a lower-end system, use only one thread for 1 or 2 cores, use half
    // of the cores for less than 8 cores. On systems with more cores, the max
    // number threads is 4 to be used for inference.
    int num_of_threads =
        std::min(4, (base::SysInfo::NumberOfProcessors() + 1) / 2);
    cpu_options->SetNumThreads(num_of_threads);
#endif
    auto set_accelerators_status =
        options->SetHardwareAccelerators(accelerators);
    if (!set_accelerators_status) {
      return base::unexpected(mojom::Error::New(
          mojom::Error::Code::kUnknownError,
          base::StringPrintf("Unable to set HW Accelerators: %s",
                             set_accelerators_status.Error().Message())));
    }
    return std::move(*options);
  }

  // TODO(crbug.com/454732289): Re-enable the once external weight loader is
  // fully supported in LiteRT. std::unique_ptr<::litert::ScopedFile>
  // weights_file_;
  flatbuffers::DetachedBuffer model_content_;
  std::optional<::litert::Environment> env_;
  std::optional<::litert::CompiledModel> model_;

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
  ::litert::Profiler profiler_;
#endif
};

// static
void GraphImplLiteRt::CreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    ContextImplLiteRt* context,
    base::File weights_file,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  base::flat_map<OperandId, base::flat_set<OperationId>>
      operand_to_dependent_operations =
          std::move(compute_resource_info.operand_to_dependent_operations);
  base::flat_map<OperandId, OperationId> operand_to_producing_operation =
      std::move(compute_resource_info.operand_to_producing_operation);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(
          &GraphImplLiteRt::CreateAndBuildOnBackgroundThread,
          context->properties(), context->options().device,
          std::move(graph_info), std::move(constant_operands),
          std::move(operand_to_dependent_operations),
          std::move(operand_to_producing_operation),
          // TODO(crbug.com/454732289): Explicitly pass an invalid file before
          // LiteRT external weight loader support is ready. This will force the
          // builder to store the weights in the flatbuffer.
          base::File(base::File::FILE_ERROR_NOT_FOUND)),
      base::BindOnce(&GraphImplLiteRt::DidCreateAndBuild, std::move(receiver),
                     context->AsWeakPtr(), std::move(compute_resource_info),
                     std::move(callback)));
}

// static
base::expected<std::unique_ptr<GraphImplLiteRt::ComputeResources>,
               mojom::ErrorPtr>
GraphImplLiteRt::CreateAndBuildOnBackgroundThread(
    ContextProperties context_properties,
    mojom::Device context_device,
    mojom::GraphInfoPtr graph_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, base::flat_set<OperationId>>
        operand_to_dependent_operations,
    base::flat_map<OperandId, OperationId> operand_to_producing_operation,
    base::File weights_file) {
  ASSIGN_OR_RETURN(
      tflite::GraphBuilderTflite::Result result,
      tflite::GraphBuilderTflite::CreateAndBuild(
          context_properties, *graph_info, std::move(constant_operands),
          std::move(operand_to_dependent_operations),
          std::move(operand_to_producing_operation), std::move(weights_file)),
      [](std::string error) {
        return mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                                 std::move(error));
      });

  ASSIGN_OR_RETURN(std::unique_ptr<ComputeResources> compute_resources,
                   ComputeResources::Create(context_device, std::move(result)));
  return compute_resources;
}

void GraphImplLiteRt::DidCreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    ComputeResourceInfo compute_resource_info,
    WebNNContextImpl::CreateGraphImplCallback callback,
    base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
        compute_resources) {
  if (!context) {
    return;
  }

  if (!compute_resources.has_value()) {
    std::move(callback).Run(
        base::unexpected(std::move(compute_resources.error())));
    return;
  }

  auto devices = std::move((*compute_resources)->devices);
  auto input_name_to_index =
      std::move((*compute_resources)->input_name_to_descriptor);
  auto output_name_to_index =
      std::move((*compute_resources)->output_name_to_descriptor);
  auto compute_resources_state =
      base::MakeRefCounted<QueueableResourceState<ComputeResources>>(
          std::move(*compute_resources));
  std::move(callback).Run(base::MakeRefCounted<GraphImplLiteRt>(
      std::move(receiver), std::move(compute_resource_info),
      std::move(input_name_to_index), std::move(output_name_to_index),
      std::move(compute_resources_state), std::move(context),
      std::move(devices)));
}

GraphImplLiteRt::~GraphImplLiteRt() = default;

GraphImplLiteRt::GraphImplLiteRt(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    ComputeResourceInfo compute_resource_info,
    std::vector<std::pair<std::string, tflite::TensorDescriptor>>
        input_name_to_descriptor,
    std::vector<std::pair<std::string, tflite::TensorDescriptor>>
        output_name_to_descriptor,
    scoped_refptr<QueueableResourceState<ComputeResources>>
        compute_resources_state,
    base::WeakPtr<WebNNContextImpl> context,
    std::vector<mojom::Device> devices)
    : WebNNGraphImpl(std::move(receiver),
                     std::move(context),
                     std::move(compute_resource_info),
                     std::move(devices)),
      compute_resources_state_(std::move(compute_resources_state)),
      input_name_to_descriptor_(std::move(input_name_to_descriptor)),
      output_name_to_descriptor_(std::move(output_name_to_descriptor)) {}

void GraphImplLiteRt::DispatchImpl(
    const base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        named_inputs,
    const base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        named_outputs) {
  ScopedTrace scoped_trace("GraphImplLiteRt::DispatchImpl");

  std::vector<
      std::pair<int, scoped_refptr<QueueableResourceState<BufferContent>>>>
      input_buffer_states, output_buffer_states;
  input_buffer_states.reserve(input_name_to_descriptor_.size());
  output_buffer_states.reserve(output_name_to_descriptor_.size());

  // The caller guarantees that all expected tensors have been provided.
  for (const auto& [name, descriptor] : input_name_to_descriptor_) {
    auto* tflite_tensor =
        static_cast<tflite::TensorImplTflite*>(named_inputs.at(name).get());
    input_buffer_states.emplace_back(descriptor.tensor_index,
                                     tflite_tensor->GetBufferState());
  }

  for (const auto& [name, descriptor] : output_name_to_descriptor_) {
    auto* tflite_tensor =
        static_cast<tflite::TensorImplTflite*>(named_outputs.at(name).get());
    output_buffer_states.emplace_back(descriptor.tensor_index,
                                      tflite_tensor->GetBufferState());
  }

  // Input tensors will be read from while the graph is executing, so lock them
  // them as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources;
  shared_resources.reserve(input_name_to_descriptor_.size());
  for (const auto& [name, buffer_state] : input_buffer_states) {
    shared_resources.push_back(buffer_state);
  }

  // Exclusively reserve all output tensors - which will be written to - and
  // this graph's compute resources while the graph is executing.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources;
  // Extra +1 is for the compute resources.
  exclusive_resources.reserve(1 + output_name_to_descriptor_.size());
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
             const std::vector<
                 std::pair<std::string, tflite::TensorDescriptor>>&
                 input_name_to_descriptor,
             const std::vector<
                 std::pair<std::string, tflite::TensorDescriptor>>&
                 output_name_to_descriptor,
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
                    base::Unretained(raw_compute_resources),
                    input_name_to_descriptor, output_name_to_descriptor,
                    std::move(buffers), std::move(scoped_trace)),
                std::move(completion_closure));
          },
          compute_resources_state_, std::move(input_buffer_states),
          std::move(output_buffer_states), input_name_to_descriptor_,
          output_name_to_descriptor_, std::move(scoped_trace)));
  task->Enqueue();
}

}  // namespace webnn::litert
