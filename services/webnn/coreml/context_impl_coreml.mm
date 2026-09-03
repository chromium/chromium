// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/context_impl_coreml.h"

#import <CoreML/CoreML.h>

#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "services/webnn/coreml/graph_builder_coreml.h"
#include "services/webnn/coreml/graph_impl_coreml.h"
#include "services/webnn/coreml/tensor_impl_coreml.h"
#include "services/webnn/coreml/utils_coreml.h"
#include "services/webnn/error.h"
#include "services/webnn/gpu_task_scheduler.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"

namespace webnn::coreml {

// static
std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>
ContextImplCoreml::Create(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    mojom::CreateContextOptionsPtr options,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    mojo::PendingReceiver<mojom::WebNNModelLoader> model_loader_receiver) {
  auto task_runner = owning_task_runner;
  std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> context_impl(
      new ContextImplCoreml(
          std::move(receiver), std::move(context_provider), std::move(options),
          std::move(gpu_task_scheduler), std::move(memory_tracker),
          std::move(owning_task_runner), shared_image_manager,
          std::move(main_task_runner), std::move(model_loader_receiver)),
      OnTaskRunnerDeleter(std::move(task_runner)));
  return context_impl;
}

ContextImplCoreml::ContextImplCoreml(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    mojom::CreateContextOptionsPtr options,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    mojo::PendingReceiver<mojom::WebNNModelLoader> model_loader_receiver)
    : WebNNContextImpl(std::move(receiver),
                       std::move(context_provider),
                       ContextBackendUma::kCoreML,
                       GraphBuilderCoreml::GetContextProperties(),
                       std::move(options),
                       mojo::ScopedDataPipeConsumerHandle(),
                       mojo::ScopedDataPipeProducerHandle(),
                       std::move(gpu_task_scheduler),
                       std::move(memory_tracker),
                       std::move(owning_task_runner),
                       shared_image_manager,
                       std::move(main_task_runner)) {
  if (model_loader_receiver.is_valid()) {
    model_loader_receiver_.Bind(std::move(model_loader_receiver));
  }
}

ContextImplCoreml::~ContextImplCoreml() = default;

base::WeakPtr<WebNNContextImpl> ContextImplCoreml::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplCoreml::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    CreateGraphImplCallback callback) {
  if (base::FeatureList::IsEnabled(mojom::features::kWebNNCompilerProcess)) {
    ReportBadMessageAndDisconnect(kBadMessageGraphBuilderBypassesCompiler);
    return;
  }

  GraphImplCoreml::CreateAndBuild(
      *this, std::move(graph_info), std::move(compute_resource_info),
      std::move(constant_operands), options().Clone(), properties(),
      std::move(callback));
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplCoreml::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info) {
  return TensorImplCoreml::Create(std::move(receiver), *this,
                                  std::move(tensor_info));
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplCoreml::CreateTensorFromSharedImageImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    WebNNTensorImpl::RepresentationPtr representation) {
  return TensorImplCoreml::Create(std::move(receiver), *this,
                                  std::move(tensor_info),
                                  std::move(representation));
}

std::string_view ContextImplCoreml::GetBackendName() const {
  return "CoreML";
}

std::vector<mojom::WebNNExecutionProviderDetailsPtr>
ContextImplCoreml::GetExecutionProvidersInfo() const {
  // CoreML does not have the concept of execution providers, so we return an
  // empty list.
  return {};
}

void ContextImplCoreml::LoadCompiledGraph(
    mojom::CompiledGraphPtr compiled_graph,
    LoadCompiledGraphCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!context_provider_) {
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Context provider is destroyed.")));
    return;
  }

  // Ask the Browser process to safely copy the compiled model from the Compiler
  // process's temp folder to the GPU process's secure temp folder.
  context_provider_->webnn_browser_host()->CopyCompiledModel(
      compiled_graph->compiled_model_path,
      base::BindOnce(&ContextImplCoreml::OnCompiledModelCopied,
                     weak_factory_.GetWeakPtr(), std::move(compiled_graph),
                     std::move(callback)));
}

void ContextImplCoreml::OnCompiledModelCopied(
    mojom::CompiledGraphPtr compiled_graph,
    LoadCompiledGraphCallback callback,
    const std::optional<base::FilePath>& gpu_model_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!gpu_model_path) {
    LOG(ERROR) << "[WebNN GPU] Brokered copy failed for path: "
               << compiled_graph->compiled_model_path.value();
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Failed to copy compiled model.")));
    return;
  }

  // Reconstruct coreml_name_to_operand_name mapping.
  std::vector<std::pair<std::string, std::string>> coreml_name_to_operand_name;
  coreml_name_to_operand_name.reserve(
      compiled_graph->input_binding_names.size() +
      compiled_graph->output_binding_names.size());
  for (const auto& [name, coreml_name] : compiled_graph->input_binding_names) {
    coreml_name_to_operand_name.emplace_back(coreml_name, name);
  }
  for (const auto& [name, coreml_name] : compiled_graph->output_binding_names) {
    coreml_name_to_operand_name.emplace_back(coreml_name, name);
  }

  // Wrap the copied model path's parent directory in ScopedTempDir to manage
  // its lifetime. It will be deleted once the graph is destroyed.
  base::ScopedTempDir compiled_model_dir;
  std::ignore = compiled_model_dir.Set(gpu_model_path->DirName());

  // Load the copied model.
  GraphImplCoreml::CreateAndLoadCompiledModel(
      *this, std::move(compiled_model_dir),
      std::move(coreml_name_to_operand_name),
      base::BindOnce(
          [](base::WeakPtr<ContextImplCoreml> context,
             LoadCompiledGraphCallback callback,
             base::expected<scoped_refptr<WebNNGraphImpl>, mojom::ErrorPtr>
                 result) {
            if (!context) {
              std::move(callback).Run(base::unexpected(mojom::Error::New(
                  mojom::Error::Code::kUnknownError, "Context lost.")));
              return;
            }
            if (!result.has_value()) {
              std::move(callback).Run(
                  base::unexpected(std::move(result.error())));
              return;
            }
            auto loaded_graph_info = mojom::LoadedGraphInfo::New(
                result.value()->handle(), std::move(result.value()->devices()));
            context->AddGraphImpl(std::move(result.value()));
            std::move(callback).Run(std::move(loaded_graph_info));
          },
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextImplCoreml::RequestCompilerContext(
    mojo::PendingReceiver<mojom::WebNNCompilerContext>
        compiler_context_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create the ModelLoader pipe pair on this thread (the owning thread where
  // model_loader_receiver_ lives), avoiding cross-thread posting.
  model_loader_receiver_.reset();
  auto model_loader_remote = model_loader_receiver_.BindNewPipeAndPassRemote();

  // Post to the main thread to reconnect via the context provider.
  main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<WebNNContextProviderImpl> context_provider,
             mojom::CreateContextOptionsPtr options,
             ContextProperties properties,
             mojo::PendingReceiver<mojom::WebNNCompilerContext>
                 compiler_context_receiver,
             mojo::PendingRemote<mojom::WebNNModelLoader> model_loader_remote) {
            if (!context_provider) {
              LOG(ERROR) << "[WebNN] RequestCompilerContext() failed: "
                            "WebNNContextProviderImpl is no longer available.";
              return;
            }
            context_provider->ReconnectCompilerContext(
                std::move(options), properties,
                std::move(compiler_context_receiver),
                std::move(model_loader_remote));
          },
          context_provider_, options_->Clone(), properties_,
          std::move(compiler_context_receiver),
          std::move(model_loader_remote)));
}

}  // namespace webnn::coreml
