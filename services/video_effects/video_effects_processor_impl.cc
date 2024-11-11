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
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/video_effects_processor_webgpu.h"
#include "services/video_effects/video_effects_service_impl.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace {

// Maximum number of context losses that the postprocessor will tolerate before
// entering unrecoverable state:
constexpr int kMaxNumOfContextLosses = 5;

bool ContextLossesExceedThreshold(int num_context_losses) {
  return num_context_losses >= kMaxNumOfContextLosses;
}

}  // namespace

namespace video_effects {

VideoEffectsProcessorImpl::VideoEffectsProcessorImpl(
    wgpu::Device device,
    mojo::PendingRemote<media::mojom::VideoEffectsManager> manager_remote,
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver,
    std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider,
    base::OnceClosure on_unrecoverable_error)
    : device_(device),
      manager_remote_(std::move(manager_remote)),
      processor_receiver_(this, std::move(processor_receiver)),
      gpu_channel_host_provider_(std::move(gpu_channel_host_provider)),
      on_unrecoverable_error_(std::move(on_unrecoverable_error)) {
  processor_receiver_.set_disconnect_handler(
      base::BindOnce(&VideoEffectsProcessorImpl::OnMojoDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
  manager_remote_.set_disconnect_handler(
      base::BindOnce(&VideoEffectsProcessorImpl::OnMojoDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

VideoEffectsProcessorImpl::~VideoEffectsProcessorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (webgpu_context_provider_) {
    webgpu_context_provider_->RemoveObserver(this);
  }

  if (raster_interface_context_provider_) {
    raster_interface_context_provider_->RemoveObserver(this);
  }
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

  background_segmentation_model_blob_.resize(model_blob.size());
  base::span(background_segmentation_model_blob_).copy_from(model_blob);
}

bool VideoEffectsProcessorImpl::InitializeGpuState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!ContextLossesExceedThreshold(num_context_losses_));

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

  webgpu_context_provider_->AddObserver(this);
  raster_interface_context_provider_->AddObserver(this);
  processor_webgpu_ = std::make_unique<VideoEffectsProcessorWebGpu>(
      device_, webgpu_context_provider_, raster_interface_context_provider_,
      shared_image_interface_);
  return processor_webgpu_->Initialize();
}

void VideoEffectsProcessorImpl::OnContextLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  initialized_ = false;

  // Before trying to reinitialize GPU state, we should first tear down the
  // current state.
  // TODO(bialpio): consider extracting the entire GPU state into a helper
  // class that can just be nulled out.

  if (webgpu_context_provider_) {
    webgpu_context_provider_->RemoveObserver(this);
  }

  webgpu_context_provider_.reset();

  if (raster_interface_context_provider_) {
    raster_interface_context_provider_->RemoveObserver(this);
  }

  raster_interface_context_provider_.reset();
  shared_image_interface_.reset();
  processor_webgpu_.reset();

  ++num_context_losses_;
  if (ContextLossesExceedThreshold(num_context_losses_)) {
    MaybeCallOnUnrecoverableError();
    return;
  }

  const bool gpu_initialized = InitializeGpuState();

  if (!gpu_initialized) {
    MaybeCallOnUnrecoverableError();
  }
}

void VideoEffectsProcessorImpl::OnMojoDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // One of the pipes has been disconnected, tear down both and notify the
  // `on_unrecoverable_error_` since the owner of this processor instance may
  // want to tear us down (the processor is no longer usable).
  processor_receiver_.reset();
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

  if (!initialized_) {
    if (ContextLossesExceedThreshold(num_context_losses_)) {
      std::move(callback).Run(mojom::PostProcessResult::NewError(
          mojom::PostProcessError::kUnusable));
    } else {
      std::move(callback).Run(mojom::PostProcessResult::NewError(
          mojom::PostProcessError::kNotReady));
    }
    return;
  }

  processor_webgpu_->PostProcess(
      std::move(input_frame_data), std::move(input_frame_info),
      std::move(result_frame_data), result_pixel_format, std::move(callback));
}

void VideoEffectsProcessorImpl::MaybeCallOnUnrecoverableError() {
  if (on_unrecoverable_error_) {
    std::move(on_unrecoverable_error_).Run();
  }
}

}  // namespace video_effects
