// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SERVICE_IMPL_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"

namespace video_effects {

class VideoEffectsProcessorImpl;

// Abstract interface that is used by `VideoEffectsServiceImpl` to obtain
// instances of `gpu::GpuChannelHost`. Those are then going to be used to
// create context providers over which the communication to GPU service will
// happen.
class GpuChannelHostProvider {
 public:
  virtual ~GpuChannelHostProvider() = default;

  virtual scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost() = 0;
};

class VideoEffectsServiceImpl : public mojom::VideoEffectsService {
 public:
  // Similarly to `VideoCaptureServiceImpl`, `VideoEfffectsServiceImpl` needs
  // to receive something that returns `gpu::GpuChannelHost` instances in order
  // to be able to communicate with the GPU service - this is passed in via the
  // `gpu_channel_host_provider`.
  // `receiver` is the receiving end of the mojo pipe used to communicate with
  // this instance.
  explicit VideoEffectsServiceImpl(
      mojo::PendingReceiver<mojom::VideoEffectsService> receiver,
      std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider);

  ~VideoEffectsServiceImpl() override;

  // mojom::VideoEffectsService implementation:
  void CreateEffectsProcessor(
      mojo::PendingRemote<media::mojom::VideoEffectsManager> manager,
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor) override;

 private:
  std::vector<std::unique_ptr<VideoEffectsProcessorImpl>> processors_;

  mojo::Receiver<mojom::VideoEffectsService> receiver_;
  std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider_;
};

}  // namespace video_effects

#endif
