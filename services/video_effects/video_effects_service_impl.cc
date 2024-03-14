// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_service_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "services/video_effects/video_effects_processor_impl.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace video_effects {

VideoEffectsServiceImpl::VideoEffectsServiceImpl(
    mojo::PendingReceiver<mojom::VideoEffectsService> receiver,
    std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider)
    : receiver_(this, std::move(receiver)),
      gpu_channel_host_provider_(std::move(gpu_channel_host_provider)) {
  CHECK(gpu_channel_host_provider_);
}

VideoEffectsServiceImpl::~VideoEffectsServiceImpl() = default;

void VideoEffectsServiceImpl::CreateEffectsProcessor(
    mojo::PendingRemote<media::mojom::VideoEffectsManager> manager,
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor) {
  std::unique_ptr<VideoEffectsProcessorImpl> effects_processor =
      std::make_unique<VideoEffectsProcessorImpl>(std::move(manager),
                                                  std::move(processor));

  processors_.push_back(std::move(effects_processor));
}

}  // namespace video_effects
