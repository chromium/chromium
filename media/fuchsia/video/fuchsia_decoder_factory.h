// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_VIDEO_FUCHSIA_DECODER_FACTORY_H_
#define MEDIA_FUCHSIA_VIDEO_FUCHSIA_DECODER_FACTORY_H_

#include "media/base/decoder_factory.h"
#include "media/fuchsia/mojom/fuchsia_media_resource_provider.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace blink {
class BrowserInterfaceBrokerProxy;
}  // namespace blink

namespace media {

class FuchsiaDecoderFactory final : public DecoderFactory {
 public:
  explicit FuchsiaDecoderFactory(
      blink::BrowserInterfaceBrokerProxy* interface_broker);
  ~FuchsiaDecoderFactory() final;

  // DecoderFactory implementation.
  void CreateAudioDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) final;
  SupportedVideoDecoderConfigs GetSupportedVideoDecoderConfigsForWebRTC() final;
  void CreateVideoDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      MediaLog* media_log,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) final;

 private:
  mojo::PendingRemote<media::mojom::FuchsiaMediaResourceProvider>
      media_resource_provider_handle_;
  mojo::Remote<media::mojom::FuchsiaMediaResourceProvider>
      media_resource_provider_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_VIDEO_FUCHSIA_DECODER_FACTORY_H_