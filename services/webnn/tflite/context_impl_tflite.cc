// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_impl_tflite.h"

#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"
#include "services/webnn/scoped_sequence.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/tflite/graph_impl_tflite.h"
#include "services/webnn/tflite/tensor_impl_tflite.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::tflite {

// static
std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>
ContextImplTflite::Create(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    ScopedTrace scoped_trace) {
  DCHECK(owning_task_runner->RunsTasksInCurrentSequence());
  auto task_runner = owning_task_runner;
  return std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>(
      new ContextImplTflite(
          std::move(receiver), std::move(context_provider), std::move(options),
          std::move(write_tensor_consumer), std::move(read_tensor_producer),
          command_buffer_id, std::move(sequence), std::move(memory_tracker),
          std::move(owning_task_runner), shared_image_manager,
          std::move(main_task_runner)),
      OnTaskRunnerDeleter(std::move(task_runner)));
}

ContextImplTflite::ContextImplTflite(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : WebNNContextImpl(std::move(receiver),
                       std::move(context_provider),
                       GraphBuilderTflite::GetContextProperties(),
                       std::move(options),
                       std::move(write_tensor_consumer),
                       std::move(read_tensor_producer),
                       command_buffer_id,
                       std::move(sequence),
                       std::move(memory_tracker),
                       std::move(owning_task_runner),
                       shared_image_manager,
                       std::move(main_task_runner)) {}

ContextImplTflite::~ContextImplTflite() = default;

base::WeakPtr<WebNNContextImpl> ContextImplTflite::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplTflite::CreateGraphImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    CreateGraphImplCallback callback) {
  std::move(callback).Run(GraphImplTflite::CreateAndBuild(
      std::move(receiver), std::move(graph_info),
      std::move(compute_resource_info), std::move(constant_operands),
      std::move(constant_tensor_operands), this));
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplTflite::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info) {
  // TODO(crbug.com/332350952): implement constant tensors for TFLite.
  if (tensor_info->usage.Has(MLTensorUsageFlags::kGraphConstant)) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "Creation of constant tensors is not supported."));
  }
  // TODO(crbug.com/345352987): implement WebGPU interop tensors for TFLite
  // backend.
  if (tensor_info->usage.Has(MLTensorUsageFlags::kWebGpuInterop)) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "WebGPU Interop is not supported."));
  }
  return TensorImplTflite::Create(std::move(receiver), AsWeakPtr(),
                                  std::move(tensor_info));
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplTflite::CreateTensorFromSharedImageImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    WebNNTensorImpl::RepresentationPtr representation) {
  return base::unexpected(
      mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                        "WebGPU Interop is not supported."));
}

}  // namespace webnn::tflite
