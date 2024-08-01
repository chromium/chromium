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

scoped_refptr<viz::ContextProviderCommandBuffer> CreateAndBindContextProvider(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::ContextType context_type) {
  CHECK(gpu_channel_host);
  CHECK(!gpu_channel_host->IsLost());
  CHECK(context_type == gpu::CONTEXT_TYPE_WEBGPU ||
        context_type == gpu::CONTEXT_TYPE_OPENGLES2);

  auto context_creation_attribs = gpu::ContextCreationAttribs();
  context_creation_attribs.context_type = context_type;
  context_creation_attribs.enable_gles2_interface = false;
  context_creation_attribs.enable_raster_interface =
      context_type == gpu::CONTEXT_TYPE_OPENGLES2;
  context_creation_attribs.bind_generates_resource =
      context_type == gpu::CONTEXT_TYPE_WEBGPU;

  // TODO(bialpio): replace `gpu::SharedMemoryLimits::ForOOPRasterContext()`
  // with something better suited or explain why it's appropriate the way it is
  // now.
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider =
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), content::kGpuStreamIdDefault,
          gpu::SchedulingPriority::kNormal, gpu::kNullSurfaceHandle,
          GURL("chrome://gpu/VideoEffects"), true /* automatic flushes */,
          false /* support locking */,
          context_type == gpu::CONTEXT_TYPE_WEBGPU
              ? gpu::SharedMemoryLimits::ForWebGPUContext()
              : gpu::SharedMemoryLimits::ForOOPRasterContext(),
          context_creation_attribs,
          viz::command_buffer_metrics::ContextType::VIDEO_CAPTURE);

  const gpu::ContextResult context_result =
      context_provider->BindToCurrentSequence();
  if (context_result != gpu::ContextResult::kSuccess) {
    LOG(ERROR) << "Bind context provider failed. context_result: "
               << base::to_underlying(context_result);
    return nullptr;
  }

  return context_provider;
}

}  // namespace

namespace video_effects {

VideoEffectsProcessorImpl::VideoEffectsProcessorImpl(
    mojo::PendingRemote<media::mojom::VideoEffectsManager> manager_remote,
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver,
    std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider,
    base::OnceClosure on_unrecoverable_error)
    : manager_remote_(std::move(manager_remote)),
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
  auto gpu_channel_host = gpu_channel_host_provider_->GetGpuChannelHost();
  CHECK(gpu_channel_host);

  scoped_refptr<viz::ContextProviderCommandBuffer> webgpu_context_provider =
      CreateAndBindContextProvider(gpu_channel_host, gpu::CONTEXT_TYPE_WEBGPU);
  if (!webgpu_context_provider) {
    return false;
  }

  scoped_refptr<viz::ContextProviderCommandBuffer>
      raster_interface_context_provider = CreateAndBindContextProvider(
          gpu_channel_host, gpu::CONTEXT_TYPE_OPENGLES2);
  if (!raster_interface_context_provider) {
    return false;
  }

  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface =
      gpu_channel_host->CreateClientSharedImageInterface();
  if (!shared_image_interface) {
    return false;
  }

  webgpu_context_provider_ = std::move(webgpu_context_provider);
  webgpu_context_provider_->AddObserver(this);
  raster_interface_context_provider_ =
      std::move(raster_interface_context_provider);
  raster_interface_context_provider_->AddObserver(this);
  shared_image_interface_ = std::move(shared_image_interface);

  processor_webgpu_ = std::make_unique<VideoEffectsProcessorWebGpu>(
      webgpu_context_provider_, raster_interface_context_provider_,
      shared_image_interface_,
      base::BindOnce(
          base::BindOnce(&VideoEffectsProcessorImpl::OnWebGpuProcessorError,
                         weak_ptr_factory_.GetWeakPtr())));
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

void VideoEffectsProcessorImpl::OnWebGpuProcessorError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  processor_webgpu_.reset();

  processor_webgpu_ = std::make_unique<VideoEffectsProcessorWebGpu>(
      webgpu_context_provider_, raster_interface_context_provider_,
      shared_image_interface_,
      base::BindOnce(&VideoEffectsProcessorImpl::OnWebGpuProcessorError,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!processor_webgpu_->Initialize()) {
    MaybeCallOnUnrecoverableError();
  }
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
