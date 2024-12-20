// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_impl_ort.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/types/expected_macros.h"
#include "services/webnn/error.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"

namespace webnn::ort {
base::expected<std::unique_ptr<GraphImplOrt>, mojom::ErrorPtr>
GraphImplOrt::CreateAndBuild(
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ContextImplOrt* context) {
  ASSIGN_OR_RETURN(
      std::unique_ptr<GraphBuilderOrt::Result> result,
      GraphBuilderOrt::CreateAndBuild(*graph_info, context->properties(),
                                      std::move(constant_operands)));

  PlatformFunctions* platform_functions = PlatformFunctions::GetInstance();
  if (!platform_functions) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError, "Platform functions error."));
  }
  auto ort_get_api_base_proc = platform_functions->ort_get_api_base_proc();

  // currently, win11 inside onnxruntime.dll version is 1.10.1 and can support
  // IR_VERSION_2021_7_30.
  const char* version = ort_get_api_base_proc()->GetVersionString();
  LOG(ERROR) << "onnxruntime dll version is " << version;

  OrtSessionOptions* session_options;
  const OrtApi* ort_api = GetOrtApi();
  ORT_ABORT_ON_ERROR(ort_api->CreateSessionOptions(&session_options));

  ORT_ABORT_ON_ERROR(ort_api->SetSessionGraphOptimizationLevel(
      session_options, GraphOptimizationLevel::ORT_ENABLE_ALL));

  if (context->options().device == mojom::CreateContextOptions::Device::kGpu || context->options().device == mojom::CreateContextOptions::Device::kNpu) {
    const OrtDmlApi* ort_dml_api;
    ORT_ABORT_ON_ERROR(ort_api->GetExecutionProviderApi(
        "DML", 10, reinterpret_cast<const void**>(&ort_dml_api)));

    OrtDmlDeviceOptions options;
    if (context->options().device == mojom::CreateContextOptions::Device::kGpu) {
      options = {OrtDmlPerformancePreference::MinimumPower, OrtDmlDeviceFilter::Gpu};
    } else {
      options = {OrtDmlPerformancePreference::MinimumPower, OrtDmlDeviceFilter::Gpu};
      // NPU is available only when ENABLE_NPU_ADAPTER_ENUMERATION
      // options = {OrtDmlPerformancePreference::MinimumPower, OrtDmlDeviceFilter::Npu};
    }

    ort_dml_api->SessionOptionsAppendExecutionProvider_DML2(session_options, &options);
  }

  // Todo: Consider move the session creation to BackgroundThread since load model may be time-consuming.
  OrtSession* session;
  const OrtEnv* env = context->env();
  CHECK(env);
  CHECK_STATUS(GetOrtGraphApi()->CreateSessionFromModel(
      env, result->model.get_ptr(), session_options, &session));

  LOG(ERROR) << "success to create session.";

  // verify_input_output_count;
  size_t count;
  ORT_ABORT_ON_ERROR(ort_api->SessionGetInputCount(session, &count));
  LOG(ERROR) << "input count: " << count;
  CHECK_EQ(count, compute_resource_info.input_names_to_descriptors.size());
  ORT_ABORT_ON_ERROR(ort_api->SessionGetOutputCount(session, &count));
  LOG(ERROR) << "output count: " << count;
  CHECK_EQ(count, compute_resource_info.output_names_to_descriptors.size());

  return base::WrapUnique(new GraphImplOrt(std::move(compute_resource_info),
                                           session, session_options, context));
}

GraphImplOrt::~GraphImplOrt() {
  const OrtApi* ort_api = GetOrtApi();
  ort_api->ReleaseSessionOptions(session_options_);
  ort_api->ReleaseSession(session_);
}

GraphImplOrt::GraphImplOrt(ComputeResourceInfo compute_resource_info,
                           OrtSession* session,
                           OrtSessionOptions* session_options,
                           ContextImplOrt* context)
    : WebNNGraphImpl(context, std::move(compute_resource_info)),
      session_(session),
      session_options_(session_options) {}

// TODO: Support dispatching in parallel.
void GraphImplOrt::DispatchImpl(
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs) {
  std::vector<const char*> input_names;
  std::vector<const OrtValue*> input_tensors;
  for (const auto& [name, input_tensor] : named_inputs) {
    TensorImplOrt* input_tensor_impl =
        static_cast<TensorImplOrt*>(input_tensor);
    input_names.push_back(name.data());
    input_tensors.push_back(input_tensor_impl->tensor());
  }

  std::vector<const char*> output_names;
  std::vector<OrtValue*> output_tensors;
  for (const auto& [name, output_tensor] : named_outputs) {
    TensorImplOrt* output_tensor_impl =
        static_cast<TensorImplOrt*>(output_tensor);
    output_names.push_back(name.data());
    output_tensors.push_back(output_tensor_impl->tensor());
  }

  const OrtApi* ort_api = GetOrtApi();
  // TODO: Use RunAsync to support async execution.
  ORT_ABORT_ON_ERROR(ort_api->Run(session_, nullptr, input_names.data(),
                                  input_tensors.data(), input_names.size(),
                                  output_names.data(), output_names.size(),
                                  output_tensors.data()));
}

}  // namespace webnn::ort
