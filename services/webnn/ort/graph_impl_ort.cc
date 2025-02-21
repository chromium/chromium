// Copyright 2025 The Chromium Authors
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
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_session_options_config_keys.h"

namespace webnn::ort {

namespace {

struct Session {
  Session(ScopedOrtEnvPtr env,
          ScopedOrtSessionPtr session,
          std::vector<base::HeapArray<uint8_t>> external_data)
      : external_data(std::move(external_data)),
        env(std::move(env)),
        session(std::move(session)) {}
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  ~Session() = default;

  OrtSession* GetSession() { return session.Get(); }

  std::vector<base::HeapArray<uint8_t>> external_data;

  // `env` should be prior to `session`. That ensures releasing `env` after
  // releasing the session. This avoids unloading the providers DLLs being
  // used during `session` destruction.
  ScopedOrtEnvPtr env;
  ScopedOrtSessionPtr session;
};

base::flat_map<std::string,
               scoped_refptr<QueueableResourceState<BufferContentOrt>>>
ToNamedBufferStateMap(
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_tensors) {
  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
      buffer_states;
  buffer_states.reserve(named_tensors.size());

  for (const auto& [name, tensor] : named_tensors) {
    buffer_states.emplace(
        name, static_cast<TensorImplOrt*>(tensor)->GetBufferState());
  }

  return buffer_states;
}

}  // namespace

// Represents the collection of resources associated with a particular graph.
// These resources may outlive their associated `GraphImplOrt` instance while
// executing the graph.
class GraphImplOrt::ComputeResources {
 public:
  ComputeResources(std::unique_ptr<Session> session,
                   base::flat_map<std::string, std::string>
                       operand_input_name_to_onnx_input_name,
                   base::flat_map<std::string, std::string>
                       operand_output_name_to_onnx_output_name)
      : operand_input_name_to_onnx_input_name_(
            std::move(operand_input_name_to_onnx_input_name)),
        operand_output_name_to_onnx_output_name_(
            std::move(operand_output_name_to_onnx_output_name)),
        session_(std::move(session)) {}

  ~ComputeResources() = default;

  // Run the model asynchronously in `base::ThreadPool`.
  void OrtRunSync(
      std::vector<std::pair<std::string, const OrtValue*>> named_input_tensors,
      std::vector<std::pair<std::string, OrtValue*>> named_output_tensors) {
    TRACE_EVENT0("gpu", "ort::GraphImplOrt::ComputeResources::OrtRunSync");

    std::vector<const char*> input_names;
    std::vector<const OrtValue*> input_tensors;
    input_names.reserve(named_input_tensors.size());
    input_tensors.reserve(named_input_tensors.size());
    for (const auto& [name, tensor] : named_input_tensors) {
      input_names.push_back(
          operand_input_name_to_onnx_input_name_.at(name).data());
      input_tensors.push_back(tensor);
    }

    std::vector<const char*> output_names;
    std::vector<OrtValue*> output_tensors;
    output_names.reserve(named_output_tensors.size());
    output_tensors.reserve(named_output_tensors.size());
    for (const auto& [name, tensor] : named_output_tensors) {
      output_names.push_back(
          operand_output_name_to_onnx_output_name_.at(name).data());
      output_tensors.push_back(tensor);
    }

    const OrtApi* ort_api = GetOrtApi();
    CHECK_STATUS(ort_api->Run(session_->GetSession(), nullptr,
                              input_names.data(), input_tensors.data(),
                              input_names.size(), output_names.data(),
                              output_names.size(), output_tensors.data()));
  }

 private:
  base::flat_map<std::string, std::string>
      operand_input_name_to_onnx_input_name_;
  base::flat_map<std::string, std::string>
      operand_output_name_to_onnx_output_name_;
  std::unique_ptr<Session> session_;
};

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
                     context->properties(), std::move(constant_operands)),
      std::move(wrapped_callback));
}

// static
base::expected<std::unique_ptr<GraphImplOrt::ComputeResources>, mojom::ErrorPtr>
GraphImplOrt::CreateAndBuildOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    mojom::CreateContextOptionsPtr context_options,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands) {
  const mojom::CreateContextOptions::Device device_type =
      context_options->device;

  ASSIGN_OR_RETURN(std::unique_ptr<OrtModelBuilder::ModelInfo> model_info,
                   GraphBuilderOrt::CreateAndBuild(
                       *graph_info, std::move(context_properties),
                       std::move(constant_operands)));

  const OrtApi* ort_api = GetOrtApi();
  ScopedOrtSessionOptionsPtr session_options;
  if (ORT_CALL_FAILED(
          ort_api->CreateSessionOptions(session_options.GetAddressOf()))) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create graph."));
  }

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
    CALL_ORT_FUNC(ort_api->SetOptimizedModelFilePath(
        session_options, dump_path.value().c_str()));

    // TODO(https://github.com/shiyi9801/chromium/issues/54): Support saving
    // tensors created with `CreateTensorWithDataAsOrtValue()` or
    // `CreateTensorWithDataAndDeleterAsOrtValue()` when ORT Model Builder API
    // supports it.
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtDisableCpuFallback)) {
    CALL_ORT_FUNC(ort_api->AddSessionConfigEntry(
        session_options, /*config_key=*/kOrtSessionOptionsDisableCPUEPFallback,
        /*config_value=*/"1"));
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
    CALL_ORT_FUNC(ort_api->SetSessionGraphOptimizationLevel(
        session_options, GraphOptimizationLevel::ORT_DISABLE_ALL));

    OrtOpenVINOProviderOptions openvino_options;
    openvino_options.device_type = openvino_device_type.c_str();

    // TODO(https://github.com/shiyi9801/chromium/issues/74): Fail early when
    // creating the context if the OpenVINO EP is not supported.
    if (ORT_CALL_FAILED(ort_api->SessionOptionsAppendExecutionProvider_OpenVINO(
            session_options, &openvino_options))) {
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
    CALL_ORT_FUNC(ort_api->SetSessionGraphOptimizationLevel(
        session_options, GraphOptimizationLevel::ORT_ENABLE_BASIC));
  }

  // `CreateEnv()` will increase the reference count and return the reference of
  // the existing `OrtEnv` instance that is created by context provider. `env`
  // will be owned by `GraphImplOrt::Session` that ensures releasing `OrtEnv`
  // reference after releasing `OrtSession`.
  ScopedOrtEnvPtr env;
  if (ORT_CALL_FAILED(ort_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "WebNN",
                                         env.GetAddressOf()))) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create graph."));
  }

  ScopedOrtSessionPtr session;
  if (ORT_CALL_FAILED(GetOrtModelBuilderApi()->CreateSessionFromModel(
          env, model_info->model, session_options, session.GetAddressOf()))) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to build graph."));
  }

  auto compute_session =
      base::WrapUnique(new Session(std::move(env), std::move(session),
                                   std::move(model_info->external_data)));

  base::flat_map<std::string, std::string>
      operand_input_name_to_onnx_input_name;
  for (uint64_t id : graph_info->input_operands) {
    std::string_view operand_name =
        graph_info->id_to_operand_map.at(id)->name.value();
    operand_input_name_to_onnx_input_name.emplace(
        operand_name, GetOperandName(operand_name, id));
  }
  base::flat_map<std::string, std::string>
      operand_output_name_to_onnx_output_name;
  for (uint64_t id : graph_info->output_operands) {
    std::string_view operand_name =
        graph_info->id_to_operand_map.at(id)->name.value();
    operand_output_name_to_onnx_output_name.emplace(
        operand_name, GetOperandName(operand_name, id));
  }

  // TODO: remove this log which is for temporary debugging purpose.
  LOG(ERROR) << "========= Running on ORT =============";

  return base::WrapUnique(new GraphImplOrt::ComputeResources(
      std::move(compute_session),
      std::move(operand_input_name_to_onnx_input_name),
      std::move(operand_output_name_to_onnx_output_name)));
}

// static
void GraphImplOrt::DidCreateAndBuild(
    base::WeakPtr<WebNNContextImpl> context,
    ComputeResourceInfo compute_resource_info,
    WebNNContextImpl::CreateGraphImplCallback callback,
    base::expected<std::unique_ptr<GraphImplOrt::ComputeResources>,
                   mojom::ErrorPtr> result) {
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

GraphImplOrt::~GraphImplOrt() = default;

GraphImplOrt::GraphImplOrt(
    ComputeResourceInfo compute_resource_info,
    std::unique_ptr<GraphImplOrt::ComputeResources> compute_resources,
    ContextImplOrt* context)
    : WebNNGraphImpl(context, std::move(compute_resource_info)) {
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

  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
      named_input_buffer_states = ToNamedBufferStateMap(named_input_tensors);
  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
      named_output_buffer_states = ToNamedBufferStateMap(named_output_tensors);

  // Input tensors will be read from while the graph is executing, so lock them
  // them as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources;
  shared_resources.reserve(named_input_tensors.size());
  for (const auto& [_, buffer_state] : named_input_buffer_states) {
    shared_resources.push_back(buffer_state);
  }

  // Exclusively reserve all output tensors, which will be written to.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources;
  // Extra +1 is for the compute resources.
  exclusive_resources.reserve(1 + named_output_tensors.size());
  exclusive_resources.push_back(compute_resources_state_);
  for (const auto& [_, buffer_state] : named_output_buffer_states) {
    exclusive_resources.push_back(buffer_state);
  }

  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources), std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<ComputeResources>>
                 compute_resources_state,
             base::flat_map<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
                 named_input_buffer_states,
             base::flat_map<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
                 named_output_buffer_states,
             base::OnceClosure completion_closure) {
            ComputeResources* raw_compute_resources =
                compute_resources_state->GetExclusivelyLockedResource();

            std::vector<std::pair<std::string, const OrtValue*>>
                named_input_tensors;
            named_input_tensors.reserve(named_input_buffer_states.size());
            std::vector<std::pair<std::string, OrtValue*>> named_output_tensors;
            named_output_tensors.reserve(named_output_buffer_states.size());

            for (const auto& [name, buffer] : named_input_buffer_states) {
              named_input_tensors.emplace_back(
                  name, buffer->GetSharedLockedResource().tensor());
            }
            for (const auto& [name, buffer] : named_output_buffer_states) {
              named_output_tensors.emplace_back(
                  name, buffer->GetExclusivelyLockedResource()->tensor());
            }

            // TODO(https://github.com/shiyi9801/chromium/issues/57): Decide
            // whether to use `OrtApi::RunAsync` or `OrtApi::Run` here.

            // Compute tasks can take a significant amount of time, use the
            // thread pool to avoid blocking the main thread.
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE,
                base::BindOnce(&ComputeResources::OrtRunSync,
                               base::Unretained(raw_compute_resources),
                               std::move(named_input_tensors),
                               std::move(named_output_tensors)),
                std::move(completion_closure));
          },
          compute_resources_state_, std::move(named_input_buffer_states),
          std::move(named_output_buffer_states)));

  task->Enqueue();
}

}  // namespace webnn::ort
