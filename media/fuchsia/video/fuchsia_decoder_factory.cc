// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/video/fuchsia_decoder_factory.h"

#include "base/task/sequenced_task_runner.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/fuchsia/video/fuchsia_video_decoder.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace media {

FuchsiaDecoderFactory::FuchsiaDecoderFactory(
    mojo::PendingRemote<media::mojom::FuchsiaMediaCodecProvider>
        resource_provider,
    bool allow_overlays)
    : resource_provider_(std::move(resource_provider)),
      allow_overlays_(allow_overlays) {}

FuchsiaDecoderFactory::~FuchsiaDecoderFactory() = default;

void FuchsiaDecoderFactory::CreateAudioDecoders(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) {
  // There are no Fuchsia-specific audio decoders.
}

void FuchsiaDecoderFactory::CreateVideoDecoders(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) {
  if (gpu_factories && gpu_factories->IsGpuVideoDecodeAcceleratorEnabled()) {
    auto* context_provider = gpu_factories->GetMediaContextProvider();

    // GetMediaContextProvider() may return nullptr when the context was lost
    // (e.g. after GPU process crash). To handle this case, RenderThreadImpl
    // creates a new GpuVideoAcceleratorFactories with a new ContextProvider
    // instance, but there is no way to get it here. For now just don't add
    // FuchsiaVideoDecoder in that scenario.
    //
    // TODO(crbug.com/42050657) Handle lost context.
    if (context_provider) {
      video_decoders->push_back(std::make_unique<FuchsiaVideoDecoder>(
          context_provider, resource_provider_, allow_overlays_));
    } else {
      LOG(ERROR) << "Can't create FuchsiaVideoDecoder due to GPU context loss.";
    }
  }
}

}  // namespace media
