// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_IMPL_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_IMPL_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/video_effects_processor_webgpu.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace video_effects {

// Abstract interface that is used by `VideoEffectsServiceImpl` to obtain
// instances of `gpu::GpuChannelHost`. Those are then going to be used to
// create context providers over which the communication to GPU service will
// happen.
class GpuChannelHostProvider {
 public:
  virtual ~GpuChannelHostProvider() = default;

  // Return a connected `gpu::GpuChannelHost`. Implementations should expect
  // this method to be called somewhat frequently when a new Video Effects
  // Processor is created.
  virtual scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost() = 0;
};

class VideoEffectsProcessorImpl : public mojom::VideoEffectsProcessor,
                                  public viz::ContextLostObserver {
 public:
  // `gpu_channel_host_provider` must outlive this processor.
  // `on_unrecoverable_error` will be called after an unrecoverable condition
  // has happened, making this processor defunct. This can happen e.g. when any
  // of the mojo pipes owned by this processor have been disconnected, or when
  // the processor was unable to reinitialize GPU resources after context loss.
  explicit VideoEffectsProcessorImpl(
      mojo::PendingRemote<media::mojom::VideoEffectsManager> manager_remote,
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver,
      std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider,
      base::OnceClosure on_unrecoverable_error);

  ~VideoEffectsProcessorImpl() override;

  // Initializes the post-processor. Initialization errors are not recoverable.
  // Despite that fact, it is guaranteed that initialization errors won't cause
  // the `on_unrecoverable_error_` to be invoked - this is done to avoid
  // possible re-entrancy in the caller (e.g. the caller constructs a processor
  // with an error callback bound to a member method & attempts to initialize
  // the processor - if an initialization error were to cause the callback to be
  // invoked, we would re-enter code inside the caller).
  bool Initialize();

  void SetBackgroundSegmentationModel(base::span<const uint8_t> model_blob);

  void PostProcess(media::mojom::VideoBufferHandlePtr input_frame_data,
                   media::mojom::VideoFrameInfoPtr input_frame_info,
                   media::mojom::VideoBufferHandlePtr result_frame_data,
                   media::VideoPixelFormat result_pixel_format,
                   PostProcessCallback callback) override;

 private:
  // viz::ContextLostObserver:
  void OnContextLost() override;

  // Registered as a disconnect handler for `manager_remote_` and
  // `processor_receiver_`. Calling it will cause this processor to become
  // defunct as we cannot work without functional mojo connections.
  void OnMojoDisconnected();

  // Passed into `processor_webgpu_` as its unrecoverable error callback.
  // It will attempt to recreate the `processor_webgpu_` instance and
  // reinitialize it.
  void OnWebGpuProcessorError();

  // Initializes GPU state (context providers and shared image interface).
  bool InitializeGpuState();

  // Calls `on_unrecoverable_error_` if it's set.
  void MaybeCallOnUnrecoverableError();

  bool initialized_ = false;

  mojo::Remote<media::mojom::VideoEffectsManager> manager_remote_;
  mojo::Receiver<mojom::VideoEffectsProcessor> processor_receiver_;

  std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider_;

  // Called when this processor enters a defunct state.
  base::OnceClosure on_unrecoverable_error_;

  // GPU state. Will be created in `Initialize()`, and should be recreated
  // on context loss.
  scoped_refptr<viz::ContextProviderCommandBuffer> webgpu_context_provider_;
  scoped_refptr<viz::RasterContextProvider> raster_interface_context_provider_;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;

  std::unique_ptr<VideoEffectsProcessorWebGpu> processor_webgpu_;
  std::vector<uint8_t> background_segmentation_model_blob_;

  // We'll keep track of how many context losses we've experienced. If this
  // number is too high, we'll make this processor defunct, assuming that
  // it is causing some instability with GPU service.
  int num_context_losses_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<VideoEffectsProcessorImpl> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_IMPL_H_
