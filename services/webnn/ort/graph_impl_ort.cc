// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_impl_ort.h"

#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
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
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

namespace {

// Builds operand descriptors by querying the session's actual I/O metadata.
// Returns nullopt if the session metadata is invalid or doesn't match the
// expected binding names.
std::optional<base::flat_map<std::string, OperandDescriptor>>
BuildDescriptorsFromSession(
    const OrtSession* session,
    const base::flat_map<std::string, std::string>& operand_name_to_onnx_name,
    const ContextProperties& context_properties,
    bool is_input) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  size_t operand_count = 0;
  if (is_input ? ORT_CALL_FAILED(
                     ort_api->SessionGetInputCount(session, &operand_count))
               : ORT_CALL_FAILED(
                     ort_api->SessionGetOutputCount(session, &operand_count))) {
    return std::nullopt;
  }
  if (operand_count != operand_name_to_onnx_name.size()) {
    return std::nullopt;
  }

  // Build a reverse map: onnx_name -> operand_name.
  std::vector<std::pair<std::string, std::string>> reverse_pairs;
  reverse_pairs.reserve(operand_name_to_onnx_name.size());
  for (const auto& [operand_name, onnx_name] : operand_name_to_onnx_name) {
    reverse_pairs.emplace_back(onnx_name, operand_name);
  }
  base::flat_map<std::string, std::string> onnx_name_to_operand_name(
      std::move(reverse_pairs));
  if (onnx_name_to_operand_name.size() != operand_name_to_onnx_name.size()) {
    // In case there are duplicate ONNX names.
    return std::nullopt;
  }

  OrtAllocator* allocator = nullptr;
  CHECK_STATUS(ort_api->GetAllocatorWithDefaultOptions(&allocator));
  CHECK(allocator);

  std::vector<std::pair<std::string, OperandDescriptor>> descriptor_pairs;
  descriptor_pairs.reserve(operand_count);

  for (size_t i = 0; i < operand_count; ++i) {
    char* onnx_name = nullptr;
    base::ScopedClosureRunner free_onnx_name(base::BindOnce(
        [](OrtAllocator* a, char** p) {
          if (*p) {
            a->Free(a, *p);
          }
        },
        allocator, &onnx_name));
    if (is_input ? ORT_CALL_FAILED(ort_api->SessionGetInputName(
                       session, i, allocator, &onnx_name))
                 : ORT_CALL_FAILED(ort_api->SessionGetOutputName(
                       session, i, allocator, &onnx_name))) {
      return std::nullopt;
    }
    auto name_it = onnx_name_to_operand_name.find(onnx_name);
    if (name_it == onnx_name_to_operand_name.end()) {
      return std::nullopt;
    }
    const auto& operand_name = name_it->second;

    // Get tensor info from the session.
    ScopedOrtTypeInfo type_info;
    if (is_input
            ? ORT_CALL_FAILED(ort_api->SessionGetInputTypeInfo(
                  session, i, ScopedOrtTypeInfo::Receiver(type_info).get()))
            : ORT_CALL_FAILED(ort_api->SessionGetOutputTypeInfo(
                  session, i, ScopedOrtTypeInfo::Receiver(type_info).get()))) {
      return std::nullopt;
    }
    const OrtTensorTypeAndShapeInfo* tensor_info = nullptr;
    if (ORT_CALL_FAILED(
            ort_api->CastTypeInfoToTensorInfo(type_info.get(), &tensor_info))) {
      return std::nullopt;
    }
    if (!tensor_info) {
      return std::nullopt;
    }

    // Get operand data type.
    ONNXTensorElementDataType onnx_type =
        ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
    if (ORT_CALL_FAILED(
            ort_api->GetTensorElementType(tensor_info, &onnx_type))) {
      return std::nullopt;
    }
    std::optional<OperandDataType> data_type = OnnxToWebnnDataType(onnx_type);
    if (!data_type.has_value()) {
      return std::nullopt;
    }

    // Get operand shape.
    size_t dim_count = 0;
    if (ORT_CALL_FAILED(ort_api->GetDimensionsCount(tensor_info, &dim_count))) {
      return std::nullopt;
    }
    std::vector<int64_t> onnx_dims(dim_count);
    if (dim_count > 0 && ORT_CALL_FAILED(ort_api->GetDimensions(
                             tensor_info, onnx_dims.data(), dim_count))) {
      return std::nullopt;
    }
    std::optional<std::vector<uint32_t>> shape = OnnxToWebnnShape(onnx_dims);
    if (!shape.has_value()) {
      return std::nullopt;
    }

    auto descriptor = OperandDescriptor::Create(context_properties, *data_type,
                                                *shape, operand_name);
    if (!descriptor.has_value()) {
      return std::nullopt;
    }
    descriptor_pairs.emplace_back(operand_name, std::move(descriptor.value()));
  }

  base::flat_map<std::string, OperandDescriptor> descriptors(
      std::move(descriptor_pairs));
  if (descriptors.size() != operand_count) {
    // Reject if any session I/O names collided when read as C strings (e.g. "x"
    // and "x\0alias" both truncate to "x"), which would collapse distinct
    // operands.
    return std::nullopt;
  }
  return descriptors;
}

}  // namespace

// Represents the collection of resources associated with a particular graph.
// These resources may outlive their associated `GraphImplOrt` instance while
// executing the graph.
class GraphImplOrt::ComputeResources {
 public:
  // Session created from model. ExternalWeightsManager keeps weights alive
  // since they are referenced by the session.
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

  // Session created from compiled model bytes, weights are embedded so
  // ExternalWeightsManager is not needed.
  ComputeResources(scoped_refptr<Environment> env,
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
        session_(std::move(session)) {}

  ~ComputeResources() = default;

  ScopedOrtStatus OrtRunSync(
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
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
    return CALL_ORT_FUNC(ort_api->Run(
        session_.get(), nullptr, input_names.data(), input_tensors.data(),
        input_names.size(), output_names.data(), output_names.size(),
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
  // destruction. Only used when the session is created from a model (not from
  // compiled bytes).
  std::unique_ptr<ExternalWeightsManager> external_weights_manager_;
  ScopedOrtSession session_;
};

// static
void GraphImplOrt::CreateAndBuild(
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ContextImplOrt& context,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  ScopedTrace scoped_trace("GraphImplOrt::CreateAndBuild");

  // Safe to use std::ref because the posted task and its reply will be canceled
  // if the context is destroyed.
  context.cancelable_task_tracker().PostTaskAndReplyWithResult(
      context.env()->graph_compilation_task_runner().get(), FROM_HERE,
      base::BindOnce(&GraphImplOrt::CreateAndBuildOnBackgroundThread,
                     std::move(graph_info), context.session_options(),
                     context.env(), context.properties(),
                     std::move(constant_operands), std::move(scoped_trace)),
      base::BindOnce(&GraphImplOrt::DidCreateAndBuild, std::ref(context),
                     std::move(compute_resource_info), std::move(callback)));
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
    WebNNContextImpl& context,
    ComputeResourceInfo compute_resource_info,
    WebNNContextImpl::CreateGraphImplCallback callback,
    base::expected<std::unique_ptr<GraphImplOrt::ComputeResources>,
                   mojom::ErrorPtr> result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
    return;
  }

  // TODO(crbug.com/418031018): Get devices that will be used for dispatch.
  std::move(callback).Run(base::MakeRefCounted<GraphImplOrt>(
      std::move(compute_resource_info), std::move(result.value()), context,
      /*devices=*/std::vector<mojom::Device>()));
}

// static
base::expected<scoped_refptr<WebNNGraphImpl>, mojom::ErrorPtr>
GraphImplOrt::CreateSessionFromCompiledGraph(
    WebNNContextImpl& context,
    scoped_refptr<SessionOptions> session_options,
    scoped_refptr<Environment> env,
    mojo_base::BigBuffer compiled_model_data,
    base::flat_map<std::string, std::string>
        operand_input_name_to_onnx_input_name,
    base::flat_map<std::string, std::string>
        operand_output_name_to_onnx_output_name) {
  ScopedOrtSession session;
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  if (ORT_CALL_FAILED(ort_api->CreateSessionFromArray(
          env->get(), compiled_model_data.data(), compiled_model_data.size(),
          session_options->get(), ScopedOrtSession::Receiver(session).get()))) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Failed to create session from compiled model."));
  }

  // Build operand descriptors from the session's I/O metadata.
  auto input_descriptors = BuildDescriptorsFromSession(
      session.get(), operand_input_name_to_onnx_input_name,
      context.properties(), /*is_input=*/true);
  auto output_descriptors = BuildDescriptorsFromSession(
      session.get(), operand_output_name_to_onnx_output_name,
      context.properties(), /*is_input=*/false);
  if (!input_descriptors.has_value() || !output_descriptors.has_value()) {
    LOG(ERROR) << "Failed to build operand descriptors from session metadata.";
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to build graph."));
  }

  ComputeResourceInfo compute_resource_info(std::move(*input_descriptors),
                                            std::move(*output_descriptors),
                                            base::PassKey<GraphImplOrt>());

  auto compute_resources = base::WrapUnique(new GraphImplOrt::ComputeResources(
      std::move(env), std::move(session),
      std::move(operand_input_name_to_onnx_input_name),
      std::move(operand_output_name_to_onnx_output_name)));

  return base::MakeRefCounted<GraphImplOrt>(
      std::move(compute_resource_info), std::move(compute_resources), context,
      // TODO(crbug.com/418031018): Get devices that will be used for dispatch.
      /*devices=*/std::vector<mojom::Device>());
}

GraphImplOrt::~GraphImplOrt() = default;

GraphImplOrt::GraphImplOrt(
    ComputeResourceInfo compute_resource_info,
    std::unique_ptr<GraphImplOrt::ComputeResources> compute_resources,
    WebNNContextImpl& context,
    std::vector<mojom::Device> devices)
    : WebNNGraphImpl(context,
                     std::move(compute_resource_info),
                     std::move(devices)),
      compute_resources_(std::move(compute_resources)) {}

void GraphImplOrt::DispatchImpl(
    base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        named_input_tensors,
    base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        named_output_tensors) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  // Ort runs the graph on its own thread, so this call blocks until execution
  // completes.
  ScopedOrtStatus status = compute_resources_->OrtRunSync(
      std::move(named_input_tensors), std::move(named_output_tensors));
  if (status.is_valid()) {
    static_cast<ContextImplOrt&>(context_.get())
        .HandleContextLostOrCrash("Failed to run session.",
                                  ort_api->GetErrorCode(status.get()));
  }
}

}  // namespace webnn::ort
