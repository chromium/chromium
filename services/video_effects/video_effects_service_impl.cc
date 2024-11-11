// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_service_impl.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "services/video_effects/video_effects_processor_impl.h"
#include "services/video_effects/viz_gpu_channel_host_provider.h"

namespace video_effects {

VideoEffectsServiceImpl::VideoEffectsServiceImpl(
    mojo::PendingReceiver<mojom::VideoEffectsService> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {}

VideoEffectsServiceImpl::~VideoEffectsServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoEffectsServiceImpl::CreateEffectsProcessor(
    const std::string& device_id,
    mojo::PendingRemote<viz::mojom::Gpu> gpu_remote,
    mojo::PendingRemote<media::mojom::VideoEffectsManager> manager_remote,
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (processors_.contains(device_id) ||
      pending_processors_.contains(device_id)) {
    return;
  }

  auto gpu = viz::Gpu::Create(std::move(gpu_remote), io_task_runner_);
  auto gpu_channel_host_provider =
      std::make_unique<VizGpuChannelHostProvider>(std::move(gpu));

  if (device_) {
    // We already have a wgpu::Device.  Go ahead and create the processor.
    FinishCreatingEffectsProcessor(device_id, std::move(manager_remote),
                                   std::move(processor_receiver),
                                   std::move(gpu_channel_host_provider));
    return;
  }

  VizGpuChannelHostProvider* provider_ptr = nullptr;
  if (!webgpu_device_) {
    provider_ptr = gpu_channel_host_provider.get();
  }

  // Store the pending request.
  PendingEffectsProcessor pending;
  pending.manager_remote = std::move(manager_remote);
  pending.processor_receiver = std::move(processor_receiver);
  pending.gpu_channel_host_provider = std::move(gpu_channel_host_provider);
  auto [_, inserted] =
      pending_processors_.insert(std::make_pair(device_id, std::move(pending)));
  CHECK(inserted);

  if (!webgpu_device_) {
    CreateWebGpuDeviceAndEffectsProcessors(
        provider_ptr->GetWebGpuContextProvider());
    return;
  }
  // A wgpu::Device is already being created.  We don't need to do anything as
  // pending processors will be created when it is ready.
}

void VideoEffectsServiceImpl::CreateWebGpuDeviceAndEffectsProcessors(
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider) {
  CHECK(!webgpu_device_);
  CHECK(context_provider);

  WebGpuDevice::DeviceLostCallback device_lost_cb = base::BindOnce(
      &VideoEffectsServiceImpl::OnDeviceLost, weak_ptr_factory_.GetWeakPtr());
  webgpu_device_ = std::make_unique<WebGpuDevice>(std::move(context_provider),
                                                  std::move(device_lost_cb));

  WebGpuDevice::DeviceCallback device_cb =
      base::BindOnce(&VideoEffectsServiceImpl::OnDeviceCreated,
                     weak_ptr_factory_.GetWeakPtr());
  WebGpuDevice::ErrorCallback error_cb = base::BindOnce(
      &VideoEffectsServiceImpl::OnDeviceError, weak_ptr_factory_.GetWeakPtr());
  webgpu_device_->Initialize(std::move(device_cb), std::move(error_cb));
}

void VideoEffectsServiceImpl::OnDeviceCreated(wgpu::Device device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_ = std::move(device);
  FinishCreatingEffectsProcessors();
}

void VideoEffectsServiceImpl::OnDeviceError(WebGpuDevice::Error error,
                                            std::string_view msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!device_);
  LOG(ERROR) << "Unable to create wgpu::Device; error = "
             << base::to_underlying(error) << ": " << msg;
  // Abandon ship!
  pending_processors_.clear();
  webgpu_device_.reset();
  // NOTE: Call CreateWgpuDeviceAndEffectsProcessors() again if we believe this
  // was a transient failure?
}

void VideoEffectsServiceImpl::OnDeviceLost(wgpu::DeviceLostReason reason,
                                           std::string_view msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "wgpu::Device was lost; reason = "
             << base::to_underlying(reason) << ": " << msg;

  // Abandon all hope, ye who enter here.
  pending_processors_.clear();
  processors_.clear();
  device_ = nullptr;
  webgpu_device_.reset();
  // NOTE: Possibly attempt to revive current processors with a new
  // wgpu::Device, either by having them support reinitialization with a new
  // device, or replacing it with a new processor and re-binding its mojo pipes.
}

void VideoEffectsServiceImpl::FinishCreatingEffectsProcessors() {
  // Called in-sequence by OnDeviceCreated().
  for (auto& it : pending_processors_) {
    FinishCreatingEffectsProcessor(
        it.first, std::move(it.second.manager_remote),
        std::move(it.second.processor_receiver),
        std::move(it.second.gpu_channel_host_provider));
  }
  pending_processors_.clear();
}

void VideoEffectsServiceImpl::FinishCreatingEffectsProcessor(
    const std::string& device_id,
    mojo::PendingRemote<media::mojom::VideoEffectsManager> manager_remote,
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver,
    std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider) {
  // Called in-sequence.
  if (!device_) {
    // Lost the wgpu::Device before the processor could be constructed. We could
    // insert a new pending request and attempt to re-create the device.
    return;
  }

  auto on_unrecoverable_processor_error =
      base::BindOnce(&VideoEffectsServiceImpl::RemoveProcessor,
                     weak_ptr_factory_.GetWeakPtr(), device_id);

  auto effects_processor = std::make_unique<VideoEffectsProcessorImpl>(
      device_, std::move(manager_remote), std::move(processor_receiver),
      std::move(gpu_channel_host_provider),
      std::move(on_unrecoverable_processor_error));

  if (!effects_processor->Initialize()) {
    return;
  }

  auto [_, inserted] = processors_.insert(
      std::make_pair(device_id, std::move(effects_processor)));
  CHECK(inserted);
}

void VideoEffectsServiceImpl::SetBackgroundSegmentationModel(
    base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(bialpio): make a copy of the model blob to serve it every time new
  // processor gets created.
  base::MemoryMappedFile memory_mapped_model_file;
  if (!memory_mapped_model_file.Initialize(std::move(model_file))) {
    return;
  }

  // Propagate the model to all already existing processors:
  for (auto& device_id_and_processor : processors_) {
    device_id_and_processor.second->SetBackgroundSegmentationModel(
        memory_mapped_model_file.bytes());
  }
}

void VideoEffectsServiceImpl::RemoveProcessor(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processors_.erase(id);
}

VideoEffectsServiceImpl::PendingEffectsProcessor::PendingEffectsProcessor() =
    default;
VideoEffectsServiceImpl::PendingEffectsProcessor::PendingEffectsProcessor(
    PendingEffectsProcessor&&) = default;
VideoEffectsServiceImpl::PendingEffectsProcessor&
VideoEffectsServiceImpl::PendingEffectsProcessor::operator=(
    PendingEffectsProcessor&&) = default;
VideoEffectsServiceImpl::PendingEffectsProcessor::~PendingEffectsProcessor() =
    default;

}  // namespace video_effects
