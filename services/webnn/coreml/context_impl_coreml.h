// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_CONTEXT_IMPL_COREML_H_
#define SERVICES_WEBNN_COREML_CONTEXT_IMPL_COREML_H_

#include "base/memory/weak_ptr.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

class WebNNConstantOperand;

namespace coreml {

// `ContextImplCoreml` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplCoreml` for the CoreML backend on macOS.
class API_AVAILABLE(macos(14.4)) ContextImplCoreml final
    : public WebNNContextImpl {
 public:
  // Constructs a new `ContextImplCoreml`.
  static std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> Create(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<WebNNContextProviderImpl> context_provider,
      mojom::CreateContextOptionsPtr options,
      gpu::CommandBufferId command_buffer_id,
      std::unique_ptr<ScopedSequence> sequence,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  ContextImplCoreml(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<WebNNContextProviderImpl> context_provider,
      mojom::CreateContextOptionsPtr options,
      gpu::CommandBufferId command_buffer_id,
      std::unique_ptr<ScopedSequence> sequence,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  ContextImplCoreml(const WebNNContextImpl&) = delete;
  ContextImplCoreml& operator=(const ContextImplCoreml&) = delete;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

 private:
  ~ContextImplCoreml() override;

  void CreateGraphImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
      CreateGraphImplCallback callback) override;

  base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorImpl(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                   mojom::TensorInfoPtr tensor_info) override;

  base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorFromSharedImageImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      WebNNTensorImpl::RepresentationPtr representation) override;

  base::WeakPtrFactory<ContextImplCoreml> weak_factory_{this};
};

}  // namespace coreml
}  // namespace webnn

#endif  // SERVICES_WEBNN_COREML_CONTEXT_IMPL_COREML_H_
