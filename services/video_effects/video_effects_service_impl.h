// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SERVICE_IMPL_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"

namespace viz {
class Gpu;
}

namespace video_effects {

class VideoEffectsProcessorImpl;

class VideoEffectsServiceImpl : public mojom::VideoEffectsService {
 public:
  // Similarly to `VideoCaptureServiceImpl`, `VideoEfffectsServiceImpl` needs
  // to receive `viz::Gpu` instance in order to be able to communicate with the
  // GPU service. This is passed in via `viz_gpu`.
  // `receiver` is the receiving end of the mojo pipe used to communicate with
  // this instance.
  explicit VideoEffectsServiceImpl(
      mojo::PendingReceiver<mojom::VideoEffectsService> receiver,
      std::unique_ptr<viz::Gpu> viz_gpu);

  ~VideoEffectsServiceImpl() override;

  // mojom::VideoEffectsService implementation:
  void CreateEffectsProcessor(
      mojo::PendingRemote<media::mojom::VideoEffectsManager> manager,
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor) override;

 private:
  std::vector<std::unique_ptr<VideoEffectsProcessorImpl>> processors_;

  mojo::Receiver<mojom::VideoEffectsService> receiver_;
  std::unique_ptr<viz::Gpu> viz_gpu_;
};

}  // namespace video_effects

#endif
