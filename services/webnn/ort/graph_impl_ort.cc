// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_impl_ort.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "services/webnn/error.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/external_weights_manager.h"
#include "services/webnn/ort/model_editor.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

// Represents the collection of resources associated with a particular graph.
// These resources may outlive their associated `GraphImplOrt` instance while
// executing the graph.
class GraphImplOrt::ComputeResources {
 public:
  ComputeResources(
      scoped_refptr<Environment> env,
      std::unique_ptr<ExternalWeightsManager> external_weights_manager,
      ScopedOrtSession session,
      base::flat_map<std::string, std::string>
          operand_input_name_to_onnx_input_name,
      base::flat_map<std::string, std::string>
          operand_output_name_to_onnx_output_name)
      : operand_input_name_to_onnx_input_name_(
            std::move(operand_input_name_to_onnx_input_name)),
        operand_output_name_to_onnx_output_name_(
            std::move(operand_output_name_to_onnx_output_name)),
        env_(std::move(env)),
        external_weights_manager_(std::move(external_weights_manager)),
        session_(std::move(session)) {}

  ~ComputeResources() = default;

  void OrtRunSync(base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
                      named_input_tensors,
                  base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
                      named_output_tensors) {
    SCOPED_UMA_HISTOGRAM_TIMER("WebNN.ORT.TimingMs.Inference");

    ScopedTrace scoped_trace("GraphImplOrt::ComputeResources::OrtRunSync");
    std::vector<const char*> input_names;
    std::vector<const OrtValue*> input_tensors;
    input_names.reserve(named_input_tensors.size());
    input_tensors.reserve(named_input_tensors.size());
    for (const auto& [name, tensor] : named_input_tensors) {
      input_names.push_back(
          operand_input_name_to_onnx_input_name_.at(name).c_str());
      input_tensors.push_back(
          static_cast<TensorImplOrt*>(tensor.get())->tensor());
    }

    std::vector<const char*> output_names;
    std::vector<OrtValue*> output_tensors;
    output_names.reserve(named_output_tensors.size());
    output_tensors.reserve(named_output_tensors.size());
    for (const auto& [name, tensor] : named_output_tensors) {
      output_names.push_back(
          operand_output_name_to_onnx_output_name_.at(name).c_str());
      output_tensors.push_back(
          static_cast<TensorImplOrt*>(tensor.get())->tensor());
    }

    const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
    // TODO(crbug.com/433543131): Handle the inference error of MLGraph.
    CALL_ORT_FUNC(ort_api->Run(session_.get(), nullptr, input_names.data(),
                               input_tensors.data(), input_names.size(),
                               output_names.data(), output_names.size(),
                               output_tensors.data()));
  }

 private:
  base::flat_map<std::string, std::string>
      operand_input_name_to_onnx_input_name_;
  base::flat_map<std::string, std::string>
      operand_output_name_to_onnx_output_name_;

  // `env_` should be prior to `session_`. That ensures releasing `env_` after
  // releasing the session. This avoids unloading the providers DLLs being
  // used during `session` destruction.
  scoped_refptr<Environment> env_;
  // `external_weights_manager_` should be prior to `session_` since it will be
  // called by ORT to release the external weights during `session_`
  // destruction.
  std::unique_ptr<ExternalWeightsManager> external_weights_manager_;
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
                     context->env(), context->properties(),
                     std::move(constant_operands), std::move(scoped_trace)),
      base::BindOnce(&GraphImplOrt::DidCreateAndBuild, std::move(receiver),
                     context->AsWeakPtr(), std::move(compute_resource_info),
                     std::move(callback)));
}

// static
base::expected<std::unique_ptr<GraphImplOrt::ComputeResources>, mojom::ErrorPtr>
GraphImplOrt::CreateAndBuildOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    scoped_refptr<SessionOptions> session_options,
    scoped_refptr<Environment> env,
    ContextProperties context_properties,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ScopedTrace scoped_trace) {
  SCOPED_UMA_HISTOGRAM_TIMER("WebNN.ORT.TimingMs.Compilation");

  scoped_trace.AddStep("Create model info");
  std::unique_ptr<ModelEditor::ModelInfo> model_info =
      GraphBuilderOrt::CreateAndBuild(*graph_info,
                                      std::move(context_properties),
                                      std::move(constant_operands));

  scoped_trace.AddStep("Create session from model");
  ScopedOrtSession session;
  const OrtModelEditorApi* ort_model_editor_api =
      PlatformFunctions::GetInstance()->ort_model_editor_api();
  if (ORT_CALL_FAILED(ort_model_editor_api->CreateSessionFromModel(
          env->get(), model_info->model.get(), session_options->get(),
          ScopedOrtSession::Receiver(session).get()))) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create session."));
  }

  scoped_trace.AddStep("Create compute resources");
  return base::WrapUnique(new GraphImplOrt::ComputeResources(
      std::move(env), std::move(model_info->external_weights_manager),
      std::move(session),
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
  std::move(callback).Run(base::MakeRefCounted<GraphImplOrt>(
      std::move(receiver), std::move(compute_resource_info),
      std::move(result.value()), std::move(context),
      /*devices=*/std::vector<mojom::Device>()));
}

GraphImplOrt::~GraphImplOrt() = default;

GraphImplOrt::GraphImplOrt(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    ComputeResourceInfo compute_resource_info,
    std::unique_ptr<GraphImplOrt::ComputeResources> compute_resources,
    base::WeakPtr<WebNNContextImpl> context,
    std::vector<mojom::Device> devices)
    : WebNNGraphImpl(std::move(receiver),
                     std::move(context),
                     std::move(compute_resource_info),
                     std::move(devices)),
      compute_resources_(std::move(compute_resources)) {}

void GraphImplOrt::DispatchImpl(
    base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        named_input_tensors,
    base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        named_output_tensors) {
  // Ort runs the graph on its own thread, so this call blocks until execution
  // completes.
  compute_resources_->OrtRunSync(std::move(named_input_tensors),
                                 std::move(named_output_tensors));
}

}  // namespace webnn::ort
