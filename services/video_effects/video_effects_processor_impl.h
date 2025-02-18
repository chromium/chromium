// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_IMPL_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_IMPL_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/calculators/video_effects_graph_config.h"
#include "services/video_effects/gpu_channel_host_provider.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/video_effects_processor_webgpu.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace video_effects {

class VideoEffectsProcessorImpl
    : public mojom::VideoEffectsProcessor,
      public GpuChannelHostProvider::Observer,
      public media::mojom::VideoEffectsConfigurationObserver {
 public:
  // `gpu_channel_host_provider` must outlive this processor.
  // `on_unrecoverable_error` will be called after an unrecoverable condition
  // has happened, making this processor defunct. This can happen e.g. when any
  // of the mojo pipes owned by this processor have been disconnected, or when
  // the processor was unable to reinitialize GPU resources after context loss.
  explicit VideoEffectsProcessorImpl(
      wgpu::Device device,
      mojo::PendingRemote<media::mojom::ReadonlyVideoEffectsManager>
          manager_remote,
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver,
      scoped_refptr<GpuChannelHostProvider> gpu_channel_host_provider,
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
  // GpuChannelHostProvider::Observer:
  void OnContextLost(scoped_refptr<GpuChannelHostProvider>) override;
  void OnPermanentError(scoped_refptr<GpuChannelHostProvider>) override;

  // media::mojom::VideoEffectsConfigurationObserver:
  void OnConfigurationChanged(
      media::mojom::VideoEffectsConfigurationPtr configuration) override;

  // Registered as a disconnect handler for `manager_remote_` and
  // `processor_receiver_`. Calling it will cause this processor to become
  // defunct as we cannot work without functional mojo connections.
  void OnMojoDisconnected();

  // Initializes GPU state (context providers and shared image interface).
  bool InitializeGpuState();

  // Calls `on_unrecoverable_error_` if it's set.
  void MaybeCallOnUnrecoverableError();

  bool initialized_ = false;
  bool permanent_error_ = false;

  wgpu::Device device_;

  mojo::Remote<media::mojom::ReadonlyVideoEffectsManager> manager_remote_;
  mojo::Receiver<media::mojom::VideoEffectsConfigurationObserver>
      configuration_observer_{this};
  mojo::Receiver<mojom::VideoEffectsProcessor> processor_receiver_;
  mojo::Receiver<media::mojom::VideoEffectsConfigurationObserver>
      configuration_observer_receiver_;

  scoped_refptr<GpuChannelHostProvider> gpu_channel_host_provider_;

  // Called when this processor enters a defunct state.
  base::OnceClosure on_unrecoverable_error_;

  // GPU state. Will be created in `Initialize()`, and should be recreated
  // on context loss.
  scoped_refptr<viz::ContextProviderCommandBuffer> webgpu_context_provider_;
  scoped_refptr<viz::RasterContextProvider> raster_interface_context_provider_;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;

  std::unique_ptr<VideoEffectsProcessorWebGpu> processor_webgpu_;

  // Most recently seen runtime config.
  std::optional<RuntimeConfig> runtime_config_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<VideoEffectsProcessorImpl> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_IMPL_H_
