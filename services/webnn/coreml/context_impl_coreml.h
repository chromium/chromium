// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_CONTEXT_IMPL_COREML_H_
#define SERVICES_WEBNN_COREML_CONTEXT_IMPL_COREML_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/webnn/public/mojom/webnn_model_loader.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

class WebNNConstantOperand;

namespace coreml {

// `ContextImplCoreml` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplCoreml` for the CoreML backend on macOS.
class API_AVAILABLE(macos(14.4)) ContextImplCoreml final
    : public WebNNContextImpl,
      public mojom::WebNNModelLoader {
 public:
  // Constructs a new `ContextImplCoreml`.
  static std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> Create(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<WebNNContextProviderImpl> context_provider,
      mojom::CreateContextOptionsPtr options,
      std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      mojo::PendingReceiver<mojom::WebNNModelLoader> model_loader_receiver);

  ContextImplCoreml(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<WebNNContextProviderImpl> context_provider,
      mojom::CreateContextOptionsPtr options,
      std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      mojo::PendingReceiver<mojom::WebNNModelLoader> model_loader_receiver);

  ContextImplCoreml(const WebNNContextImpl&) = delete;
  ContextImplCoreml& operator=(const ContextImplCoreml&) = delete;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

  void RequestCompilerContext(mojo::PendingReceiver<mojom::WebNNCompilerContext>
                                  compiler_context_receiver) override;

 private:
  ~ContextImplCoreml() override;

  void CreateGraphImpl(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      CreateGraphImplCallback callback) override;

  base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorImpl(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                   mojom::TensorInfoPtr tensor_info) override;

  base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorFromSharedImageImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      WebNNTensorImpl::RepresentationPtr representation) override;

  std::string_view GetBackendName() const override;

  std::vector<mojom::WebNNExecutionProviderDetailsPtr>
  GetExecutionProvidersInfo() const override;

  // mojom::WebNNModelLoader:
  void LoadCompiledGraph(mojom::CompiledGraphPtr compiled_graph,
                         LoadCompiledGraphCallback callback) override;

  void OnCompiledModelCopied(
      mojom::CompiledGraphPtr compiled_graph,
      LoadCompiledGraphCallback callback,
      const std::optional<base::FilePath>& gpu_model_path);

 private:
  // Receiver end of the Compiler->GPU channel.
  mojo::Receiver<mojom::WebNNModelLoader> model_loader_receiver_{this};

  base::WeakPtrFactory<ContextImplCoreml> weak_factory_{this};
};

}  // namespace coreml
}  // namespace webnn

#endif  // SERVICES_WEBNN_COREML_CONTEXT_IMPL_COREML_H_
