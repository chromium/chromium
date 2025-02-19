// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_processor_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/types/cxx23_to_underlying.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_effects/calculators/video_effects_graph_config.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/video_effects_processor_webgpu.h"
#include "services/video_effects/video_effects_service_impl.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace video_effects {

VideoEffectsProcessorImpl::VideoEffectsProcessorImpl(
    wgpu::Device device,
    mojo::PendingRemote<media::mojom::ReadonlyVideoEffectsManager>
        manager_remote,
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver,
    scoped_refptr<GpuChannelHostProvider> gpu_channel_host_provider,
    base::OnceClosure on_unrecoverable_error)
    : device_(device),
      manager_remote_(std::move(manager_remote)),
      processor_receiver_(this, std::move(processor_receiver)),
      configuration_observer_receiver_(this),
      gpu_channel_host_provider_(gpu_channel_host_provider),
      on_unrecoverable_error_(std::move(on_unrecoverable_error)) {
  CHECK(gpu_channel_host_provider_);
  gpu_channel_host_provider_->AddObserver(*this);
  processor_receiver_.set_disconnect_handler(
      base::BindOnce(&VideoEffectsProcessorImpl::OnMojoDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
  manager_remote_.set_disconnect_handler(
      base::BindOnce(&VideoEffectsProcessorImpl::OnMojoDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
  manager_remote_->AddObserver(
      configuration_observer_.BindNewPipeAndPassRemote());
}

VideoEffectsProcessorImpl::~VideoEffectsProcessorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gpu_channel_host_provider_->RemoveObserver(*this);
}

bool VideoEffectsProcessorImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!initialized_);

  initialized_ = InitializeGpuState();
  return initialized_;
}

void VideoEffectsProcessorImpl::SetBackgroundSegmentationModel(
    base::span<const uint8_t> model_blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  processor_webgpu_->SetBackgroundSegmentationModel(model_blob);
}

bool VideoEffectsProcessorImpl::InitializeGpuState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // In order to create a Video Effects Processor, we will need to have 2
  // distinct context providers - one for WebGPUInterface, and one for
  // RasterInterface. We will also need a SharedImageInterface.
  webgpu_context_provider_ =
      gpu_channel_host_provider_->GetWebGpuContextProvider();
  if (!webgpu_context_provider_) {
    return false;
  }

  raster_interface_context_provider_ =
      gpu_channel_host_provider_->GetRasterInterfaceContextProvider();
  if (!raster_interface_context_provider_) {
    return false;
  }

  shared_image_interface_ =
      gpu_channel_host_provider_->GetSharedImageInterface();
  if (!shared_image_interface_) {
    return false;
  }

  processor_webgpu_ = std::make_unique<VideoEffectsProcessorWebGpu>(
      device_, webgpu_context_provider_, raster_interface_context_provider_,
      shared_image_interface_);
  return processor_webgpu_->Initialize();
}

void VideoEffectsProcessorImpl::OnContextLost(
    scoped_refptr<GpuChannelHostProvider>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  initialized_ = false;

  // Drop references to context objects.
  webgpu_context_provider_ = nullptr;
  raster_interface_context_provider_ = nullptr;
  shared_image_interface_ = nullptr;
  processor_webgpu_.reset();

  const bool gpu_initialized = InitializeGpuState();
  if (!gpu_initialized) {
    MaybeCallOnUnrecoverableError();
  }
}

void VideoEffectsProcessorImpl::OnPermanentError(
    scoped_refptr<GpuChannelHostProvider>) {
  MaybeCallOnUnrecoverableError();
}

void VideoEffectsProcessorImpl::OnConfigurationChanged(
    media::mojom::VideoEffectsConfigurationPtr configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We've seen a config, so let's make sure `runtime_config_` isn't nullopt.
  // Existence or absence of `configuration->blur` controls whether blur effect
  // is enabled or not:
  runtime_config_.emplace(RuntimeConfig{
      .blur_state =
          !!configuration->blur ? BlurState::kEnabled : BlurState::kDisabled});
}

void VideoEffectsProcessorImpl::OnMojoDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // One of the pipes has been disconnected, tear down both and notify the
  // `on_unrecoverable_error_` since the owner of this processor instance may
  // want to tear us down (the processor is no longer usable).
  processor_receiver_.reset();
  configuration_observer_.reset();
  manager_remote_.reset();

  MaybeCallOnUnrecoverableError();
}

void VideoEffectsProcessorImpl::PostProcess(
    media::mojom::VideoBufferHandlePtr input_frame_data,
    media::mojom::VideoFrameInfoPtr input_frame_info,
    media::mojom::VideoBufferHandlePtr result_frame_data,
    media::VideoPixelFormat result_pixel_format,
    PostProcessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (permanent_error_) {
    std::move(callback).Run(
        mojom::PostProcessResult::NewError(mojom::PostProcessError::kUnusable));
    return;
  }

  if (!initialized_ || !runtime_config_.has_value()) {
    std::move(callback).Run(
        mojom::PostProcessResult::NewError(mojom::PostProcessError::kNotReady));
    return;
  }

  processor_webgpu_->PostProcess(*runtime_config_, std::move(input_frame_data),
                                 std::move(input_frame_info),
                                 std::move(result_frame_data),
                                 result_pixel_format, std::move(callback));
}

void VideoEffectsProcessorImpl::MaybeCallOnUnrecoverableError() {
  permanent_error_ = true;
  if (on_unrecoverable_error_) {
    std::move(on_unrecoverable_error_).Run();
  }
}

}  // namespace video_effects
