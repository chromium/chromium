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
// #include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"

namespace webnn::ort {

namespace {

const base::FilePath::CharType kOnnxModelFileName[] =
    FILE_PATH_LITERAL("model.onnx");
}  // namespace

base::expected<std::unique_ptr<GraphImplOrt>, mojom::ErrorPtr>
GraphImplOrt::CreateAndBuild(
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ContextImplOrt* context) {
  base::ScopedTempDir model_file_dir;
  if (!model_file_dir.CreateUniqueTempDir()) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError, "Model allocation error."));
  }
  // C:\WINDOWS\SystemTemp\scoped_dir5020_2103244731\model.onnx

  ASSIGN_OR_RETURN(std::unique_ptr<GraphBuilderOrt::Result> result,
                   GraphBuilderOrt::CreateAndBuild(
                       *graph_info, context->properties(),
                       std::move(constant_operands), model_file_dir.GetPath()));

  const OrtApi* g_ort = ContextImplOrt::GetGlobalOrt();
  OrtEnv* env = ContextImplOrt::GetEnv(g_ort);

  OrtSessionOptions* session_options;
  ORT_ABORT_ON_ERROR(g_ort, g_ort->CreateSessionOptions(&session_options));

  ORT_ABORT_ON_ERROR(g_ort, g_ort->SetSessionGraphOptimizationLevel(session_options, GraphOptimizationLevel::ORT_ENABLE_ALL));

  // ORT_ABORT_ON_ERROR(g_ort->OrtSessionOptionsAppendExecutionProvider_DNNL(session_options,
  // 0));

  // Todo: Consider move the session creation to BackgroundThread since load model may be time-consuming.
  OrtSession* session;
  ORT_ABORT_ON_ERROR(g_ort, g_ort->CreateSession(
      env, model_file_dir.GetPath().Append(kOnnxModelFileName).value().c_str(),
      session_options, &session));

  LOG(ERROR) << "success to create session.";

  // verify_input_output_count;
  size_t count;
  ORT_ABORT_ON_ERROR(g_ort, g_ort->SessionGetInputCount(session, &count));
  LOG(ERROR) << "input count: " << count;
  CHECK_EQ(count, compute_resource_info.input_names_to_descriptors.size());
  ORT_ABORT_ON_ERROR(g_ort, g_ort->SessionGetOutputCount(session, &count));
  LOG(ERROR) << "output count: " << count;
  CHECK_EQ(count, compute_resource_info.output_names_to_descriptors.size());

  return base::WrapUnique(new GraphImplOrt(std::move(compute_resource_info),
                                           g_ort, env, session, session_options,
                                           context));
}

GraphImplOrt::~GraphImplOrt() {
  g_ort_->ReleaseSessionOptions(session_options_);
  g_ort_->ReleaseSession(session_);
  g_ort_->ReleaseEnv(env_);
}

GraphImplOrt::GraphImplOrt(
    ComputeResourceInfo compute_resource_info,
    // std::map<uint64_t,  GraphBuilderOrt::OperandInfo> operand_infos,
    const OrtApi* g_ort,
    OrtEnv* env,
    OrtSession* session,
    OrtSessionOptions* session_options,
    ContextImplOrt* context)
    : WebNNGraphImpl(context, std::move(compute_resource_info)),
      g_ort_(g_ort),
      env_(env),
      session_(session),
      session_options_(session_options) {}

void GraphImplOrt::DispatchImpl(
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs) {
  NOTIMPLEMENTED() << "dispatch is not implemented in OnnxRuntime backend.";
}

}  // namespace webnn::ort
