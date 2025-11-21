// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/context_impl_coreml.h"

#import <CoreML/CoreML.h>

#include "base/sequence_checker.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "services/webnn/coreml/graph_builder_coreml.h"
#include "services/webnn/coreml/graph_impl_coreml.h"
#include "services/webnn/coreml/tensor_impl_coreml.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/scoped_sequence.h"
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
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  auto task_runner = owning_task_runner;
  std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> context_impl(
      new ContextImplCoreml(std::move(receiver), std::move(context_provider),
                            std::move(options), command_buffer_id,
                            std::move(sequence), std::move(memory_tracker),
                            std::move(owning_task_runner), shared_image_manager,
                            std::move(main_task_runner)),
      OnTaskRunnerDeleter(std::move(task_runner)));
  return context_impl;
}

ContextImplCoreml::ContextImplCoreml(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    mojom::CreateContextOptionsPtr options,
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : WebNNContextImpl(std::move(receiver),
                       std::move(context_provider),
                       GraphBuilderCoreml::GetContextProperties(),
                       std::move(options),
                       mojo::ScopedDataPipeConsumerHandle(),
                       mojo::ScopedDataPipeProducerHandle(),
                       command_buffer_id,
                       std::move(sequence),
                       std::move(memory_tracker),
                       std::move(owning_task_runner),
                       shared_image_manager,
                       std::move(main_task_runner)) {}

ContextImplCoreml::~ContextImplCoreml() = default;

base::WeakPtr<WebNNContextImpl> ContextImplCoreml::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplCoreml::CreateGraphImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    CreateGraphImplCallback callback) {
  GraphImplCoreml::CreateAndBuild(
      std::move(receiver), this, std::move(graph_info),
      std::move(compute_resource_info), std::move(constant_operands),
      std::move(constant_tensor_operands), options().Clone(), properties(),
      std::move(callback));
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplCoreml::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info) {
  // TODO(crbug.com/332350952): implement constant tensors for CoreML.
  if (tensor_info->usage.Has(MLTensorUsageFlags::kGraphConstant)) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "Creation of constant tensors is not supported."));
  }
  // TODO(crbug.com/345352987): implement WebGPU interop tensors for CoreML
  // backend.
  if (tensor_info->usage.Has(MLTensorUsageFlags::kWebGpuInterop)) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "WebGPU Interop is not supported."));
  }
  return TensorImplCoreml::Create(std::move(receiver), AsWeakPtr(),
                                  std::move(tensor_info));
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplCoreml::CreateTensorFromSharedImageImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    WebNNTensorImpl::RepresentationPtr representation) {
  return TensorImplCoreml::Create(std::move(receiver), AsWeakPtr(),
                                  std::move(tensor_info),
                                  std::move(representation));
}

}  // namespace webnn::coreml
