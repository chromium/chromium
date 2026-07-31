// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/compiler_context_impl_ort.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/graph_builder_ort.h"
#include "services/webnn/ort/model_editor.h"
#include "services/webnn/ort/ort_session_options.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/ep_device_info.h"
#include "services/webnn/webnn_constant_operand.h"

namespace webnn::ort {

namespace {

base::unexpected<mojom::ErrorPtr> BuildGraphError() {
  return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                            "Failed to build graph."));
}

}  // namespace

struct CompilerContextImplOrt::CompilationResult {
  mojo_base::BigBuffer compiled_model_data;
  base::flat_map<std::string, std::string>
      operand_input_name_to_onnx_input_name;
  base::flat_map<std::string, std::string>
      operand_output_name_to_onnx_output_name;
};

CompilerContextImplOrt::CompilerContextImplOrt(
    const EpDeviceInfo& target_device,
    mojom::CreateContextOptionsPtr options,
    ContextProperties properties,
    mojo::PendingRemote<mojom::WebNNModelLoader> model_loader)
    : properties_(std::move(properties)),
      options_(std::move(options)),
      model_loader_(std::move(model_loader)),
      // The environment is guaranteed to be initialized in PreSandboxInit().
      env_(Environment::GetInstance().value()) {
  session_options_ = SessionOptions::Create(target_device, env_);

  model_loader_.set_disconnect_handler(
      base::BindOnce(&CompilerContextImplOrt::OnModelLoaderDisconnected,
                     base::Unretained(this)));
}

CompilerContextImplOrt::~CompilerContextImplOrt() = default;

const ContextProperties& CompilerContextImplOrt::properties() const {
  return properties_;
}

const mojom::CreateContextOptions& CompilerContextImplOrt::options() const {
  return *options_;
}

void CompilerContextImplOrt::CreateGraphBuilder(
    mojo::PendingReceiver<mojom::WebNNGraphBuilder> receiver) {
  CreateGraphBuilderImpl(std::move(receiver));
}

void CompilerContextImplOrt::BuildGraph(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo /*compute_resource_info*/,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    BuildGraphCallback callback) {
  // Wrap the callback so it is automatically called with an error if dropped
  // without being run (e.g. the model loader is disconnected).
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), BuildGraphError());

  if (!model_loader_.is_connected()) {
    return;
  }

  // `base::Unretained()` is safe because the tracker is owned by `this` and
  // will cancel the task and reply upon destruction.
  cancelable_task_tracker_.PostTaskAndReplyWithResult(
      env_->graph_compilation_task_runner().get(), FROM_HERE,
      base::BindOnce(&CompilerContextImplOrt::CompileOnBackgroundThread,
                     std::move(graph_info), session_options_, env_, properties_,
                     std::move(constant_operands)),
      base::BindOnce(&CompilerContextImplOrt::DidCompile,
                     base::Unretained(this), std::move(wrapped_callback)));
}

// static
base::expected<std::unique_ptr<CompilerContextImplOrt::CompilationResult>,
               mojom::ErrorPtr>
CompilerContextImplOrt::CompileOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    scoped_refptr<SessionOptions> session_options,
    scoped_refptr<Environment> env,
    ContextProperties context_properties,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands) {
  // Step 1: Build the ORT model from GraphInfo.
  ASSIGN_OR_RETURN(std::unique_ptr<ModelEditor::ModelInfo> model_info,
                   GraphBuilderOrt::CreateAndBuild(
                       *graph_info, std::move(context_properties),
                       std::move(constant_operands),
                       session_options->batched_matmul_k_dimension_limit()));

  // Step 2: Compile the model using ORT Compile API.
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  const OrtCompileApi* ort_compile_api =
      PlatformFunctions::GetInstance()->ort_compile_api();

  ScopedOrtModelCompilationOptions compile_options;
  if (ORT_CALL_FAILED(
          ort_compile_api->CreateModelCompilationOptionsFromSessionOptions(
              env->get(), session_options->get(),
              ScopedOrtModelCompilationOptions::Receiver(compile_options)
                  .get()))) {
    return BuildGraphError();
  }

  // Run all graph optimizations (L1-L4) in this sandboxed Compiler process so
  // the fully optimized graph is captured in the compiled output buffer. The
  // GPU process consumes that buffer with ORT_DISABLE_ALL (see
  // ort_session_options.cc), performing zero graph transformation on untrusted
  // input. The level must be set on the compile options here, not on the
  // session options: CreateModelCompilationOptionsFromSessionOptions forces the
  // level back to Default (see model_compilation_options.cc), so any level on
  // the session options is ignored on the compile path.
  if (ORT_CALL_FAILED(
          ort_compile_api->ModelCompilationOptions_SetGraphOptimizationLevel(
              compile_options.get(), ORT_ENABLE_ALL))) {
    return BuildGraphError();
  }

  if (ORT_CALL_FAILED(ort_compile_api->ModelCompilationOptions_SetInputModel(
          compile_options.get(), model_info->model.get()))) {
    return BuildGraphError();
  }

  // Embed EP context binary data into the output model buffer.
  if (ORT_CALL_FAILED(
          ort_compile_api->ModelCompilationOptions_SetEpContextEmbedMode(
              compile_options.get(), /*embed_ep_context_in_model=*/true))) {
    return BuildGraphError();
  }

  OrtAllocator* default_allocator = nullptr;
  if (ORT_CALL_FAILED(
          ort_api->GetAllocatorWithDefaultOptions(&default_allocator))) {
    return BuildGraphError();
  }

  void* output_model_buffer = nullptr;
  size_t output_model_buffer_size = 0;
  if (ORT_CALL_FAILED(
          ort_compile_api->ModelCompilationOptions_SetOutputModelBuffer(
              compile_options.get(), default_allocator, &output_model_buffer,
              &output_model_buffer_size))) {
    return BuildGraphError();
  }

  // Ensure the buffer is freed when it goes out of scope, even if the
  // compilation fails.
  base::ScopedClosureRunner free_output_model_buffer(base::BindOnce(
      [](OrtAllocator* allocator, void** buffer) {
        if (*buffer) {
          allocator->Free(allocator, *buffer);
        }
      },
      default_allocator, &output_model_buffer));

  if (ORT_CALL_FAILED(
          ort_compile_api->CompileModel(env->get(), compile_options.get()))) {
    return BuildGraphError();
  }
  CHECK(output_model_buffer);
  CHECK_GT(output_model_buffer_size, 0u);

  // Step 3: Prepare the compilation result to be sent back to GPU process.
  auto result = std::make_unique<CompilationResult>();
  // SAFETY: As configured by `ModelCompilationOptions_SetOutputModelBuffer`,
  // `OrtCompileApi::CompileModel()` will set `output_model_buffer` to a
  // buffer containing exactly `output_model_buffer_size` bytes.
  auto output_model_buffer_span = UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(output_model_buffer),
                 output_model_buffer_size));
  result->compiled_model_data = mojo_base::BigBuffer(output_model_buffer_span);

  // Transfer name mappings.
  result->operand_input_name_to_onnx_input_name =
      std::move(model_info->operand_input_name_to_onnx_input_name);
  result->operand_output_name_to_onnx_output_name =
      std::move(model_info->operand_output_name_to_onnx_output_name);

  return result;
}

void CompilerContextImplOrt::DidCompile(
    BuildGraphCallback callback,
    base::expected<std::unique_ptr<CompilationResult>, mojom::ErrorPtr>
        result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
    return;
  }
  if (!model_loader_.is_connected()) {
    return;
  }

  auto& compilation = result.value();

  auto compiled_graph = mojom::CompiledGraph::New(
      std::move(compilation->compiled_model_data),
      std::move(compilation->operand_input_name_to_onnx_input_name),
      std::move(compilation->operand_output_name_to_onnx_output_name));

  // Send compiled graph to GPU process.
  model_loader_->LoadCompiledGraph(
      std::move(compiled_graph),
      base::BindOnce(
          [](BuildGraphCallback callback,
             base::expected<mojom::LoadedGraphInfoPtr, mojom::ErrorPtr>
                 result) {
            if (!result.has_value()) {
              std::move(callback).Run(
                  base::unexpected(std::move(result.error())));
              return;
            }
            std::move(callback).Run(
                GraphCreationResult(result.value()->graph_token,
                                    std::move(result.value()->devices)));
          },
          std::move(callback)));
}

void CompilerContextImplOrt::OnModelLoaderDisconnected() {
  // Cancel any ongoing compilation tasks since the GPU process is no longer
  // available to load the compiled graphs.
  cancelable_task_tracker_.TryCancelAll();
}

}  // namespace webnn::ort
