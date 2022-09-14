// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/video/fuchsia_decoder_factory.h"

#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/fuchsia/video/fuchsia_video_decoder.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace media {

FuchsiaDecoderFactory::FuchsiaDecoderFactory(
    blink::BrowserInterfaceBrokerProxy* interface_broker) {
  interface_broker->GetInterface(
      media_resource_provider_handle_.InitWithNewPipeAndPassReceiver());
}

FuchsiaDecoderFactory::~FuchsiaDecoderFactory() = default;

void FuchsiaDecoderFactory::CreateAudioDecoders(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) {
  // There are no Fuchsia-specific audio decoders.
}

SupportedVideoDecoderConfigs
FuchsiaDecoderFactory::GetSupportedVideoDecoderConfigsForWebRTC() {
  // TODO(crbug.com/1207991) Enable HW decoder support for WebRTC.
  return {};
}

void FuchsiaDecoderFactory::CreateVideoDecoders(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) {
  // Bind `media_resource_provider_` the first time this function is called.
  if (media_resource_provider_handle_)
    media_resource_provider_.Bind(std::move(media_resource_provider_handle_));

  if (gpu_factories && gpu_factories->IsGpuVideoDecodeAcceleratorEnabled()) {
    auto* context_provider = gpu_factories->GetMediaContextProvider();

    // GetMediaContextProvider() may return nullptr when the context was lost
    // (e.g. after GPU process crash). To handle this case, RenderThreadImpl
    // creates a new GpuVideoAcceleratorFactories with a new ContextProvider
    // instance, but there is no way to get it here. For now just don't add
    // FuchsiaVideoDecoder in that scenario.
    //
    // TODO(crbug.com/995902) Handle lost context.
    if (context_provider) {
      video_decoders->push_back(std::make_unique<FuchsiaVideoDecoder>(
          context_provider, media_resource_provider_.get()));
    } else {
      LOG(ERROR) << "Can't create FuchsiaVideoDecoder due to GPU context loss.";
    }
  }
}

}  // namespace media
