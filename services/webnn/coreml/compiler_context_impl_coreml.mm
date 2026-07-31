// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/compiler_context_impl_coreml.h"

#import <CoreML/CoreML.h>

#include "base/apple/foundation_util.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "services/webnn/coreml/graph_builder_coreml.h"
#include "services/webnn/coreml/utils_coreml.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_model_loader.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_builder_impl.h"

namespace webnn::coreml {

// static
std::unique_ptr<CompilerContextImplCoreml> CompilerContextImplCoreml::Create(
    mojom::CreateContextOptionsPtr options,
    ContextProperties properties,
    mojo::PendingRemote<mojom::WebNNModelLoader> model_loader) {
  return std::make_unique<CompilerContextImplCoreml>(
      std::move(options), std::move(properties), std::move(model_loader),
      base::PassKey<CompilerContextImplCoreml>());
}

CompilerContextImplCoreml::CompilerContextImplCoreml(
    mojom::CreateContextOptionsPtr options,
    ContextProperties properties,
    mojo::PendingRemote<mojom::WebNNModelLoader> model_loader,
    base::PassKey<CompilerContextImplCoreml> pass_key)
    : properties_(std::move(properties)),
      options_(std::move(options)),
      model_loader_(std::move(model_loader)) {}

CompilerContextImplCoreml::~CompilerContextImplCoreml() = default;

void CompilerContextImplCoreml::CreateGraphBuilder(
    mojo::PendingReceiver<mojom::WebNNGraphBuilder> receiver) {
  CreateGraphBuilderImpl(std::move(receiver));
}

const ContextProperties& CompilerContextImplCoreml::properties() const {
  return properties_;
}

const mojom::CreateContextOptions& CompilerContextImplCoreml::options() const {
  return *options_;
}

void CompilerContextImplCoreml::BuildGraph(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo /*compute_resource_info*/,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    BuildGraphCallback callback) {
  auto did_compile_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&CompilerContextImplCoreml::DidCompile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&CompilerContextImplCoreml::CompileOnBackgroundThread,
                     std::move(graph_info), properties_, options_->device,
                     std::move(constant_operands),
                     std::move(did_compile_callback)));
}

CompilerContextImplCoreml::CompilationResult::CompilationResult(
    base::ScopedTempDir compiled_model_dir,
    base::flat_map<std::string, std::string> input_name_to_coreml_name,
    base::flat_map<std::string, std::string> output_name_to_coreml_name)
    : compiled_model_dir(std::move(compiled_model_dir)),
      input_name_to_coreml_name(std::move(input_name_to_coreml_name)),
      output_name_to_coreml_name(std::move(output_name_to_coreml_name)) {}

CompilerContextImplCoreml::CompilationResult::~CompilationResult() = default;

// static
void CompilerContextImplCoreml::CompileOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    ContextProperties context_properties,
    mojom::Device device,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    CompileCallback callback) {
  base::ScopedTempDir model_file_dir;
  if (!model_file_dir.CreateUniqueTempDir()) {
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Failed to create temp dir.")));
    return;
  }

  // Pre-calculate inputs and outputs name mappings
  base::flat_map<std::string, std::string> input_name_to_coreml_name;
  for (auto const& input_id : graph_info->input_operands) {
    auto& name = graph_info->operands.at(input_id.value())->name;
    CHECK(name.has_value());
    input_name_to_coreml_name.emplace(
        name.value(), GetCoreMLNameFromInput(name.value(), input_id));
  }

  base::flat_map<std::string, std::string> output_name_to_coreml_name;
  for (auto const& output_id : graph_info->output_operands) {
    auto& name = graph_info->operands.at(output_id.value())->name;
    CHECK(name.has_value());
    output_name_to_coreml_name.emplace(
        name.value(), GetCoreMLNameFromOutput(name.value(), output_id));
  }

  base::ElapsedTimer ml_model_write_timer;
  // Translate graph to .mlpackage
  auto build_graph_result = GraphBuilderCoreml::CreateAndBuild(
      *graph_info.get(), std::move(context_properties), device,
      std::move(constant_operands), model_file_dir.GetPath());

  if (!build_graph_result.has_value()) {
    std::move(callback).Run(
        base::unexpected(std::move(build_graph_result.error())));
    return;
  }
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
      "WebNN.CoreML.TimingMs.MLModelTranslate", ml_model_write_timer.Elapsed());

  base::FilePath model_path = build_graph_result.value()->GetModelFilePath();
  NSURL* model_url = base::apple::FilePathToNSURL(model_path);

  [MLModel
      compileModelAtURL:model_url
      completionHandler:
          base::CallbackToBlock(base::BindOnce(
              [](base::ElapsedTimer compilation_timer,
                 base::ScopedTempDir model_file_dir,
                 base::flat_map<std::string, std::string> inputs_map,
                 base::flat_map<std::string, std::string> outputs_map,
                 CompileCallback compile_callback, NSURL* compiled_model_url,
                 NSError* error) {
                DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
                    "WebNN.CoreML.TimingMs.MLModelCompile",
                    compilation_timer.Elapsed());
                if (error) {
                  LOG(ERROR) << "CoreML model compilation failed: " << error;
                  std::move(compile_callback)
                      .Run(base::unexpected(mojom::Error::New(
                          mojom::Error::Code::kUnknownError,
                          "CoreML model compilation failed.")));
                  return;
                }

                base::ScopedTempDir compiled_model_dir;
                if (!compiled_model_url ||
                    !compiled_model_dir.Set(
                        base::apple::NSURLToFilePath(compiled_model_url))) {
                  std::move(compile_callback)
                      .Run(base::unexpected(mojom::Error::New(
                          mojom::Error::Code::kUnknownError,
                          "Failed to get compiled model path.")));
                } else {
                  std::move(compile_callback)
                      .Run(std::make_unique<CompilationResult>(
                          std::move(compiled_model_dir), std::move(inputs_map),
                          std::move(outputs_map)));
                }
              },
              base::ElapsedTimer(), std::move(model_file_dir),
              std::move(input_name_to_coreml_name),
              std::move(output_name_to_coreml_name), std::move(callback)))];
}

void CompilerContextImplCoreml::DidCompile(
    BuildGraphCallback callback,
    base::expected<std::unique_ptr<CompilationResult>, mojom::ErrorPtr>
        result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
    return;
  }

  std::unique_ptr<CompilationResult> compilation = std::move(result.value());

  auto compiled_graph = mojom::CompiledGraph::New(
      compilation->compiled_model_dir.GetPath(),
      std::move(compilation->input_name_to_coreml_name),
      std::move(compilation->output_name_to_coreml_name));

  model_loader_->LoadCompiledGraph(
      std::move(compiled_graph),
      base::BindOnce(
          [](std::unique_ptr<CompilationResult> compilation,
             BuildGraphCallback callback,
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
          std::move(compilation), std::move(callback)));
}

}  // namespace webnn::coreml
