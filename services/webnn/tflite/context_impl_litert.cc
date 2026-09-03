// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_impl_litert.h"

#include "services/webnn/gpu_task_scheduler.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/tflite/graph_impl_litert.h"
#include "services/webnn/tflite/tensor_impl_tflite.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_in_renderer.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::litert {

// static
std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>
ContextImplLiteRt::Create(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    ScopedTrace scoped_trace,
    bool is_incognito) {
  DCHECK(owning_task_runner->RunsTasksInCurrentSequence());
  auto task_runner = owning_task_runner;
  // Read the device before `options` is moved below.
  const mojom::Device device = options->device;
  return std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>(
      new ContextImplLiteRt(
          device, std::move(receiver), std::move(context_provider),
          std::move(options), std::move(write_tensor_consumer),
          std::move(read_tensor_producer), std::move(gpu_task_scheduler),
          std::move(memory_tracker), std::move(owning_task_runner),
          shared_image_manager, std::move(main_task_runner), is_incognito),
      OnTaskRunnerDeleter(std::move(task_runner)));
}

ContextImplLiteRt::ContextImplLiteRt(
    mojom::Device device,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    bool is_incognito)
    : WebNNContextImpl(std::move(receiver),
                       std::move(context_provider),
                       ContextBackendUma::kLiteRT,
                       tflite::GraphBuilderTflite::GetContextProperties(device),
                       std::move(options),
                       std::move(write_tensor_consumer),
                       std::move(read_tensor_producer),
                       std::move(gpu_task_scheduler),
                       std::move(memory_tracker),
                       std::move(owning_task_runner),
                       shared_image_manager,
                       std::move(main_task_runner)),
      is_incognito_(is_incognito) {}

// static
WebNNContextImpl::WebNNContextImplPtr ContextImplLiteRt::CreateForRenderer(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderInRenderer> context_provider,
    mojom::CreateContextOptionsPtr options,
    WebGpuContextProperties webgpu_properties,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  DCHECK(owning_task_runner->RunsTasksInCurrentSequence());
  auto task_runner = owning_task_runner;
  // Read the device before `options` is moved below.
  const mojom::Device device = options->device;
  return WebNNContextImplPtr(
      new ContextImplLiteRt(
          device, std::move(receiver), std::move(context_provider),
          std::move(options), std::move(webgpu_properties),
          std::move(owning_task_runner), std::move(main_task_runner)),
      OnTaskRunnerDeleter(std::move(task_runner)));
}

ContextImplLiteRt::ContextImplLiteRt(
    mojom::Device device,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderInRenderer> context_provider,
    mojom::CreateContextOptionsPtr options,
    WebGpuContextProperties webgpu_properties,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : WebNNContextImpl(std::move(receiver),
                       std::move(context_provider),
                       ContextBackendUma::kLiteRT,
                       tflite::GraphBuilderTflite::GetContextProperties(device),
                       std::move(options),
                       std::move(owning_task_runner),
                       std::move(main_task_runner)),
      webgpu_properties_(std::move(webgpu_properties)) {}

ContextImplLiteRt::~ContextImplLiteRt() = default;

base::WeakPtr<WebNNContextImpl> ContextImplLiteRt::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplLiteRt::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    CreateGraphImplCallback callback) {
  if (is_incognito_.value_or(false)) {
    // In incognito mode, weights are stored in the Flatbuffer model file
    // rather than an external weights file, pass an invalid file handle to
    // the graph impl so it can fallback to the default behavior.
    GraphImplLiteRt::CreateAndBuild(
        std::move(graph_info), std::move(compute_resource_info),
        std::move(constant_operands), *this, webgpu_properties_,
        /*weights_file=*/base::File(base::File::FILE_ERROR_NOT_FOUND),
        /*session=*/mojo::NullRemote(), std::move(callback));
    return;
  }

  if (is_context_provider_in_renderer_) {
    OpenWeightsFile(base::BindOnce(
        &ContextImplLiteRt::DidOpenWeightsFile, weak_factory_.GetWeakPtr(),
        std::move(graph_info), std::move(compute_resource_info),
        std::move(constant_operands), std::move(callback)));
    return;
  }

  CreateWeightsFile(base::BindOnce(
      &ContextImplLiteRt::DidCreateWeightsFile, weak_factory_.GetWeakPtr(),
      std::move(graph_info), std::move(compute_resource_info),
      std::move(constant_operands), std::move(callback)));
}

void ContextImplLiteRt::DidCreateWeightsFile(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    CreateGraphImplCallback callback,
    base::File weights_file) {
  // An invalid `weights_file` here means the browser declined to create a
  // temporary file (for example, because the profile is incognito) or the
  // creation failed. In either case, fall back to keeping the weights
  // embedded in the in-memory Flatbuffer model.
  DidOpenWeightsFile(std::move(graph_info), std::move(compute_resource_info),
                     std::move(constant_operands), std::move(callback),
                     std::move(weights_file),
                     /*session=*/mojo::NullRemote());
}

void ContextImplLiteRt::DidOpenWeightsFile(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    CreateGraphImplCallback callback,
    base::File weights_file,
    mojo::PendingRemote<mojom::WeightsFileSession> session) {
  GraphImplLiteRt::CreateAndBuild(
      std::move(graph_info), std::move(compute_resource_info),
      std::move(constant_operands), *this, webgpu_properties_,
      std::move(weights_file), std::move(session), std::move(callback));
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplLiteRt::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info) {
  return tflite::TensorImplTflite::Create(std::move(receiver), *this,
                                          std::move(tensor_info));
}

base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
ContextImplLiteRt::CreateTensorFromSharedImageImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    WebNNTensorImpl::RepresentationPtr representation) {
  return base::unexpected(
      mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                        "WebGPU Interop is not supported."));
}

std::string_view ContextImplLiteRt::GetBackendName() const {
  return "LiteRT";
}

std::vector<mojom::WebNNExecutionProviderDetailsPtr>
ContextImplLiteRt::GetExecutionProvidersInfo() const {
  // LiteRT does not have the concept of execution providers, so we return an
  // empty list.
  return {};
}

}  // namespace webnn::litert
