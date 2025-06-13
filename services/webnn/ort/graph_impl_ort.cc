// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_impl_ort.h"

#include "base/command_line.h"
#include "base/notimplemented.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "services/webnn/error.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/model_editor.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

namespace {

std::vector<std::pair<std::string,
                      scoped_refptr<QueueableResourceState<BufferContentOrt>>>>
ToNamedBufferStates(
    const base::flat_map<std::string, WebNNTensorImpl*>& named_tensors) {
  std::vector<std::pair<
      std::string, scoped_refptr<QueueableResourceState<BufferContentOrt>>>>
      buffer_states_vec;
  buffer_states_vec.reserve(named_tensors.size());

  for (const auto& [name, tensor] : named_tensors) {
    buffer_states_vec.emplace_back(
        name, static_cast<TensorImplOrt*>(tensor)->GetBufferState());
  }

  return buffer_states_vec;
}

}  // namespace

// Represents the collection of resources associated with a particular graph.
// These resources may outlive their associated `GraphImplOrt` instance while
// executing the graph.
class GraphImplOrt::ComputeResources {
 public:
  ComputeResources(ScopedOrtEnv env,
                   ScopedOrtSession session,
                   std::vector<base::HeapArray<uint8_t>> external_data,
                   base::flat_map<std::string, std::string>
                       operand_input_name_to_onnx_input_name,
                   base::flat_map<std::string, std::string>
                       operand_output_name_to_onnx_output_name)
      : operand_input_name_to_onnx_input_name_(
            std::move(operand_input_name_to_onnx_input_name)),
        operand_output_name_to_onnx_output_name_(
            std::move(operand_output_name_to_onnx_output_name)),
        external_data_(std::move(external_data)),
        env_(std::move(env)),
        session_(std::move(session)) {}

  ~ComputeResources() = default;

  void OrtRunSync(
      std::vector<std::pair<std::string, const OrtValue*>> named_input_tensors,
      std::vector<std::pair<std::string, OrtValue*>> named_output_tensors) {
    ScopedTrace scoped_trace("GraphImplOrt::ComputeResources::OrtRunSync");
    std::vector<const char*> input_names;
    std::vector<const OrtValue*> input_tensors;
    input_names.reserve(named_input_tensors.size());
    input_tensors.reserve(named_input_tensors.size());
    for (const auto& [name, tensor] : named_input_tensors) {
      input_names.push_back(
          operand_input_name_to_onnx_input_name_.at(name).c_str());
      input_tensors.push_back(tensor);
    }

    std::vector<const char*> output_names;
    std::vector<OrtValue*> output_tensors;
    output_names.reserve(named_output_tensors.size());
    output_tensors.reserve(named_output_tensors.size());
    for (const auto& [name, tensor] : named_output_tensors) {
      output_names.push_back(
          operand_output_name_to_onnx_output_name_.at(name).c_str());
      output_tensors.push_back(tensor);
    }

    const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
    CHECK_STATUS(ort_api->Run(session_.get(), nullptr, input_names.data(),
                              input_tensors.data(), input_names.size(),
                              output_names.data(), output_names.size(),
                              output_tensors.data()));
  }

 private:
  base::flat_map<std::string, std::string>
      operand_input_name_to_onnx_input_name_;
  base::flat_map<std::string, std::string>
      operand_output_name_to_onnx_output_name_;
  std::vector<base::HeapArray<uint8_t>> external_data_;

  // `env` should be prior to `session`. That ensures releasing `env` after
  // releasing the session. This avoids unloading the providers DLLs being
  // used during `session` destruction.
  ScopedOrtEnv env_;
  ScopedOrtSession session_;
};

// static
void GraphImplOrt::CreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    ContextImplOrt* context,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  ScopedTrace scoped_trace("GraphImplOrt::CreateAndBuild");

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&GraphImplOrt::CreateAndBuildOnBackgroundThread,
                     std::move(graph_info), context->session_options(),
                     context->properties(), std::move(constant_operands),
                     std::move(scoped_trace)),
      base::BindOnce(&GraphImplOrt::DidCreateAndBuild, std::move(receiver),
                     context->AsWeakPtr(), std::move(compute_resource_info),
                     std::move(callback)));
}

// static
base::expected<std::unique_ptr<GraphImplOrt::ComputeResources>, mojom::ErrorPtr>
GraphImplOrt::CreateAndBuildOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    scoped_refptr<SessionOptions> session_options,
    ContextProperties context_properties,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ScopedTrace scoped_trace) {
  scoped_trace.AddStep("Create model info");

  ASSIGN_OR_RETURN(std::unique_ptr<ModelEditor::ModelInfo> model_info,
                   GraphBuilderOrt::CreateAndBuild(
                       *graph_info, std::move(context_properties),
                       std::move(constant_operands)));

  scoped_trace.AddStep("Initializing ORT");
  // `CreateEnv()` will increase the reference count and return the reference of
  // the existing `OrtEnv` instance that is created by context provider. `env`
  // will be owned by `GraphImplOrt::Session` that ensures releasing `OrtEnv`
  // reference after releasing `OrtSession`.
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtEnv env;
  if (ORT_CALL_FAILED(ort_api->CreateEnv(ORT_LOGGING_LEVEL_ERROR, "WebNN",
                                         ScopedOrtEnv::Receiver(env).get()))) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Failed to create the ONNX Runtime environment."));
  }

  scoped_trace.AddStep("Create session from model");
  ScopedOrtSession session;
  const OrtModelEditorApi* ort_model_editor_api =
      PlatformFunctions::GetInstance()->ort_model_editor_api();
  if (ORT_CALL_FAILED(ort_model_editor_api->CreateSessionFromModel(
          env.get(), model_info->model.get(), session_options->get(),
          ScopedOrtSession::Receiver(session).get()))) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create session."));
  }

  scoped_trace.AddStep("Create compute resources");
  return base::WrapUnique(new GraphImplOrt::ComputeResources(
      std::move(env), std::move(session), std::move(model_info->external_data),
      std::move(model_info->operand_input_name_to_onnx_input_name),
      std::move(model_info->operand_output_name_to_onnx_output_name)));
}

// static
void GraphImplOrt::DidCreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    ComputeResourceInfo compute_resource_info,
    WebNNContextImpl::CreateGraphImplCallback callback,
    base::expected<std::unique_ptr<GraphImplOrt::ComputeResources>,
                   mojom::ErrorPtr> result) {
  if (!context) {
    return;
  }

  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
    return;
  }

  // TODO(crbug.com/418031018): Get devices that will be used for dispatch.
  std::move(callback).Run(base::WrapUnique(new GraphImplOrt(
      std::move(receiver), std::move(compute_resource_info),
      std::move(result.value()), static_cast<ContextImplOrt*>(context.get()),
      /*devices=*/{})));
}

GraphImplOrt::~GraphImplOrt() = default;

GraphImplOrt::GraphImplOrt(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    ComputeResourceInfo compute_resource_info,
    std::unique_ptr<GraphImplOrt::ComputeResources> compute_resources,
    ContextImplOrt* context,
    std::vector<mojom::Device> devices)
    : WebNNGraphImpl(std::move(receiver),
                     context,
                     std::move(compute_resource_info),
                     std::move(devices)) {
  compute_resources_state_ =
      base::MakeRefCounted<QueueableResourceState<ComputeResources>>(
          std::move(compute_resources));
}

void GraphImplOrt::DispatchImpl(
    base::flat_map<std::string, WebNNTensorImpl*> named_input_tensors,
    base::flat_map<std::string, WebNNTensorImpl*> named_output_tensors) {
  ScopedTrace scoped_trace("GraphImplOrt::DispatchImpl");
  std::vector<std::pair<
      std::string, scoped_refptr<QueueableResourceState<BufferContentOrt>>>>
      named_input_buffer_states = ToNamedBufferStates(named_input_tensors);
  std::vector<std::pair<
      std::string, scoped_refptr<QueueableResourceState<BufferContentOrt>>>>
      named_output_buffer_states = ToNamedBufferStates(named_output_tensors);

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
             std::vector<std::pair<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>>
                 named_input_buffer_states,
             std::vector<std::pair<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>>
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
