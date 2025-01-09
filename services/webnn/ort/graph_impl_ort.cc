// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_impl_ort.h"

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected_macros.h"
#include "services/webnn/error.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_switches.h"

namespace webnn::ort {

namespace {

std::vector<const OrtValue*> GetSharedLockedInputTensors(
    const std::vector<scoped_refptr<QueueableResourceState<BufferContentOrt>>>&
        buffers) {
  std::vector<const OrtValue*> input_tensors;
  input_tensors.reserve(buffers.size());
  for (const auto& buffer : buffers) {
    input_tensors.push_back(buffer->GetSharedLockedResource().tensor());
  }
  return input_tensors;
}

std::vector<OrtValue*> GetExclusivelyLockedOutputTensors(
    const std::vector<scoped_refptr<QueueableResourceState<BufferContentOrt>>>&
        buffers) {
  std::vector<OrtValue*> output_tensors;
  output_tensors.reserve(buffers.size());
  for (const auto& buffer : buffers) {
    output_tensors.push_back(buffer->GetExclusivelyLockedResource()->tensor());
  }
  return output_tensors;
}

}  // namespace

// static
void GraphImplOrt::CreateAndBuild(
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ContextImplOrt* context,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  auto wrapped_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&GraphImplOrt::DidCreateAndBuild, context->AsWeakPtr(),
                     std::move(compute_resource_info), std::move(callback)));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&GraphImplOrt::CreateAndBuildOnBackgroundThread,
                     std::move(graph_info), context->options().Clone(),
                     context->properties(), std::move(constant_operands),
                     context->allocator()),
      std::move(wrapped_callback));
}

GraphImplOrt::Session::Session(
    OrtSession* session,
    std::vector<base::HeapArray<uint8_t>> external_data)
    : external_data(std::move(external_data)), session(session) {}

GraphImplOrt::Session::~Session() {
  // TODO(https://github.com/shiyi9801/chromium/issues/59): Ensure the session
  // is not released from Dllmain.
  GetOrtApi()->ReleaseSession(GetSession());
}

// static
base::expected<std::unique_ptr<GraphImplOrt::Session>, mojom::ErrorPtr>
GraphImplOrt::CreateAndBuildOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    mojom::CreateContextOptionsPtr context_options,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    scoped_refptr<AllocatorOrt> allocator) {
  const mojom::CreateContextOptions::Device device_type =
      context_options->device;

  ASSIGN_OR_RETURN(std::unique_ptr<GraphBuilderOrt::Result> result,
                   GraphBuilderOrt::CreateAndBuild(
                       *graph_info, std::move(context_properties),
                       std::move(constant_operands), allocator));

  OrtSessionOptions* session_options;
  const OrtApi* ort_api = GetOrtApi();
  CHECK_STATUS(ort_api->CreateSessionOptions(&session_options));

  // TODO(https://github.com/shiyi9801/chromium/issues/58): Investigate what
  // `GraphOptimizationLevel` should be set.
  CHECK_STATUS(ort_api->SetSessionGraphOptimizationLevel(
      session_options, GraphOptimizationLevel::ORT_ENABLE_BASIC));

  // TODO(https://github.com/shiyi9801/chromium/issues/72): Investigate if there
  // is another way to dump the model for OpenVINO EP.
  // OpenVINO EP doesn't support dumping the optimized model since it contains
  // compiled nodes which cannnot be serialized.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtDumpModel) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtUseOpenvino)) {
    static uint64_t dump_count = 0;
    base::FilePath dump_directory =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kWebNNOrtDumpModel);
    base::FilePath dump_path = dump_directory.AppendASCII(
        base::StringPrintf("model%d.onnx", dump_count++));
    CHECK_STATUS(ort_api->SetOptimizedModelFilePath(session_options,
                                                    dump_path.value().c_str()));
    // TODO(https://github.com/shiyi9801/chromium/issues/54): Support saving
    // tensors created with `CreateTensorWithDataAsOrtValue()` or
    // `CreateTensorWithDataAndDeleterAsOrtValue()` when ORT Model Builder API
    // supports it.
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtUseOpenvino)) {
    std::string openvino_device_type;
    switch (device_type) {
      case mojom::CreateContextOptions::Device::kCpu: {
        openvino_device_type = "CPU";
        break;
      }
      case mojom::CreateContextOptions::Device::kGpu: {
        openvino_device_type = "GPU";
        break;
      }
      case mojom::CreateContextOptions::Device::kNpu: {
        openvino_device_type = "NPU";
        break;
      }
    }

    // It is recommended to disable the graph optimization for OpenVINO
    // backend.
    // https://onnxruntime.ai/docs/execution-providers/OpenVINO-ExecutionProvider.html#other-configuration-settings
    CHECK_STATUS(ort_api->SetSessionGraphOptimizationLevel(
        session_options, GraphOptimizationLevel::ORT_DISABLE_ALL));

    OrtOpenVINOProviderOptions openvino_options;
    openvino_options.device_type = openvino_device_type.c_str();

    // TODO(https://github.com/shiyi9801/chromium/issues/74): Fail early when
    // creating the context if the OpenVINO EP is not supported.
    OrtStatus* append_openvino_status =
        ort_api->SessionOptionsAppendExecutionProvider_OpenVINO(
            session_options, &openvino_options);
    if (append_openvino_status != NULL) {
      std::string_view msg = ort_api->GetErrorMessage(append_openvino_status);
      LOG(ERROR) << "[WebNN] Ort Status: " << msg;
      ort_api->ReleaseStatus(append_openvino_status);
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            "OnnxRuntime OpenVINO EP is not supported."));
    }
  } else {
    // Use CPU EP by default.
    //
    // TODO(https://github.com/shiyi9801/chromium/issues/58): Investigate how
    // to apply layout optimizations (ORT_ENABLE_ALL).
    // https://onnxruntime.ai/docs/performance/model-optimizations/graph-optimizations.html#layout-optimizations
    CHECK_STATUS(ort_api->SetSessionGraphOptimizationLevel(
        session_options, GraphOptimizationLevel::ORT_ENABLE_BASIC));
  }

  OrtSession* session;
  const OrtEnv* env = allocator->env();
  OrtStatus* status = GetOrtModelBuilderApi()->CreateSessionFromModel(
      env, result->model_info->model, session_options, &session);
  ort_api->ReleaseSessionOptions(session_options);

  if (status != NULL) {
    std::string_view msg = ort_api->GetErrorMessage(status);
    LOG(ERROR) << "[WebNN] Ort Status: " << msg;
    ort_api->ReleaseStatus(status);
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create ORT session."));
  }

  LOG(ERROR) << "========= Running on ORT =============";

  return base::WrapUnique(new GraphImplOrt::Session(
      session, std::move(result->model_info->external_data)));
}

// static
void GraphImplOrt::DidCreateAndBuild(
    base::WeakPtr<WebNNContextImpl> context,
    ComputeResourceInfo compute_resource_info,
    WebNNContextImpl::CreateGraphImplCallback callback,
    base::expected<std::unique_ptr<GraphImplOrt::Session>, mojom::ErrorPtr>
        result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
    return;
  }

  if (!context) {
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Context was destroyed.")));
    return;
  }

  std::move(callback).Run(base::WrapUnique(new GraphImplOrt(
      std::move(compute_resource_info), std::move(result.value()),
      static_cast<ContextImplOrt*>(context.get()))));
}

// Represents the collection of resources associated with a particular graph.
// These resources may outlive their associated `GraphImplOrt` instance while
// executing the graph.
class GraphImplOrt::ComputeResources {
 public:
  ComputeResources(std::unique_ptr<GraphImplOrt::Session> session,
                   const ComputeResourceInfo& compute_resource_info)
      : session_(std::move(session)) {
    size_t input_count =
        compute_resource_info.input_names_to_descriptors.size();
    size_t output_count =
        compute_resource_info.output_names_to_descriptors.size();

    input_names_storage_.reserve(input_count);
    output_names_storage_.reserve(output_count);

    input_names_.reserve(input_count);
    output_names_.reserve(output_count);

    for (const auto& [name, _] :
         compute_resource_info.input_names_to_descriptors) {
      input_names_storage_.push_back(name);
      input_names_.push_back(input_names_storage_.back().data());
    }
    for (const auto& [name, _] :
         compute_resource_info.output_names_to_descriptors) {
      output_names_storage_.push_back(name);
      output_names_.push_back(output_names_storage_.back().data());
    }
  }

  ~ComputeResources() = default;

  // Run the model asynchronously in `base::ThreadPool`.
  void OrtRunSync(std::vector<const OrtValue*> input_tensors,
                  std::vector<OrtValue*> output_tensors) {
    TRACE_EVENT0("gpu", "ort::GraphImplOrt::ComputeResources::OrtRunSync");

    CHECK_EQ(input_tensors.size(), input_names_.size());
    CHECK_EQ(output_tensors.size(), output_names_.size());
    const OrtApi* ort_api = GetOrtApi();
    CHECK_STATUS(ort_api->Run(session_->GetSession(), nullptr,
                              input_names_.data(), input_tensors.data(),
                              input_names_.size(), output_names_.data(),
                              output_names_.size(), output_tensors.data()));
  }

  // Run the model asynchronously in a thread owned by ort intra op thread pool.
  void OrtRunAsync(std::vector<const OrtValue*> input_tensors,
                   std::vector<OrtValue*> output_tensors,
                   base::OnceClosure completion_closure) {
    CHECK_EQ(input_tensors.size(), input_names_.size());
    CHECK_EQ(output_tensors.size(), output_names_.size());
    const OrtApi* ort_api = GetOrtApi();

    CHECK_STATUS(ort_api->RunAsync(
        session_->GetSession(), nullptr, input_names_.data(),
        input_tensors.data(), input_names_.size(), output_names_.data(),
        output_names_.size(), output_tensors.data(),
        &ComputeResources::OnOrtRunAsyncCompleted,
        new base::OnceClosure(std::move(completion_closure))));
  }

  // This method is not run on the main thread, it's called by the ort.
  static void OnOrtRunAsyncCompleted(void* user_data,
                                     OrtValue** outputs,
                                     size_t num_outputs,
                                     OrtStatus* status) {
    auto* completion_closure = static_cast<base::OnceClosure*>(user_data);
    CHECK(!status);
    CHECK(outputs);
    std::move(*completion_closure).Run();
    delete completion_closure;
    completion_closure = nullptr;
  }

 private:
  std::unique_ptr<GraphImplOrt::Session> session_;
  std::vector<std::string> input_names_storage_;
  std::vector<std::string> output_names_storage_;
  // Pointers to the strings in `input_names_storage_`
  std::vector<const char*> input_names_;
  // Pointers to the strings in `output_names_storage_`
  std::vector<const char*> output_names_;
};

GraphImplOrt::~GraphImplOrt() = default;

GraphImplOrt::GraphImplOrt(ComputeResourceInfo compute_resource_info,
                           std::unique_ptr<GraphImplOrt::Session> session,
                           ContextImplOrt* context)
    : WebNNGraphImpl(context, std::move(compute_resource_info)) {
  std::unique_ptr<ComputeResources> compute_resources = base::WrapUnique(
      new ComputeResources(std::move(session), this->compute_resource_info()));
  compute_resources_state_ =
      base::MakeRefCounted<QueueableResourceState<ComputeResources>>(
          std::move(compute_resources));
}

void GraphImplOrt::DispatchImpl(
    const base::flat_map<std::string_view, WebNNTensorImpl*>&
        named_input_tensors,
    const base::flat_map<std::string_view, WebNNTensorImpl*>&
        named_output_tensors) {
  TRACE_EVENT0("gpu", "ort::GraphImplOrt::DispatchImpl");

  // Since the flat_map is ordered, the order of the tensors is guaranteed to be
  // the same as the order of the input/output names.
  std::vector<scoped_refptr<QueueableResourceState<BufferContentOrt>>>
      input_buffer_states, output_buffer_states;
  input_buffer_states.reserve(named_input_tensors.size());
  output_buffer_states.reserve(named_output_tensors.size());

  for (const auto& [_, tensor] : named_input_tensors) {
    input_buffer_states.emplace_back(
        static_cast<TensorImplOrt*>(tensor)->GetBufferState());
  }
  for (const auto& [_, tensor] : named_output_tensors) {
    output_buffer_states.emplace_back(
        static_cast<TensorImplOrt*>(tensor)->GetBufferState());
  }

  // Input tensors will be read from while the graph is executing, so lock them
  // them as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources;
  shared_resources.reserve(named_input_tensors.size());
  for (const auto& buffer_state : input_buffer_states) {
    shared_resources.push_back(buffer_state);
  }

  // Exclusively reserve all output tensors, which will be written to.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources;
  // Extra +1 is for the compute resources.
  exclusive_resources.reserve(1 + named_output_tensors.size());
  exclusive_resources.push_back(compute_resources_state_);
  for (const auto& buffer_state : output_buffer_states) {
    exclusive_resources.push_back(buffer_state);
  }

  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources), std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<ComputeResources>>
                 compute_resources_state,
             std::vector<scoped_refptr<
                 QueueableResourceState<BufferContentOrt>>> input_buffer_states,
             std::vector<
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
                 output_buffer_states,
             base::OnceClosure completion_closure) {
            ComputeResources* raw_compute_resources =
                compute_resources_state->GetExclusivelyLockedResource();
            std::vector<const OrtValue*> input_tensors =
                GetSharedLockedInputTensors(input_buffer_states);
            std::vector<OrtValue*> output_tensors =
                GetExclusivelyLockedOutputTensors(output_buffer_states);

            // TODO(https://github.com/shiyi9801/chromium/issues/57): Decide
            // whether to use `OrtApi::RunAsync` or `OrtApi::Run` here.

            // Compute tasks can take a significant amount of time, use the
            // thread pool to avoid blocking the main thread.
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE,
                base::BindOnce(&ComputeResources::OrtRunSync,
                               base::Unretained(raw_compute_resources),
                               std::move(input_tensors),
                               std::move(output_tensors)),
                std::move(completion_closure));
          },
          compute_resources_state_, std::move(input_buffer_states),
          std::move(output_buffer_states)));

  task->Enqueue();
}

}  // namespace webnn::ort
