// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_FUCHSIA_RENDERER_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_FUCHSIA_RENDERER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "media/base/renderer_factory.h"

namespace blink {
class BrowserInterfaceBrokerProxy;
}  // namespace blink

namespace media {
class AudioRendererSink;
class DecoderFactory;
class GpuVideoAcceleratorFactories;
class MediaLog;
class VideoDecoder;
class VideoRendererSink;
}  // namespace media

namespace content {

// RendererFactory implementation used on Fuchsia. It works the same as
// DefaultRendererFactory, except that it uses FuchsiaAudioRenderer for audio.
class CONTENT_EXPORT FuchsiaRendererFactory final
    : public media::RendererFactory {
 public:
  using GetGpuFactoriesCB =
      base::RepeatingCallback<media::GpuVideoAcceleratorFactories*()>;

  FuchsiaRendererFactory(media::MediaLog* media_log,
                         media::DecoderFactory* decoder_factory,
                         GetGpuFactoriesCB get_gpu_factories_cb,
                         blink::BrowserInterfaceBrokerProxy* interface_broker);
  ~FuchsiaRendererFactory() override;

  // RendererFactory interface.
  std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) override;

 private:
  std::vector<std::unique_ptr<media::VideoDecoder>> CreateVideoDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      media::GpuVideoAcceleratorFactories* gpu_factories);

  media::MediaLog* const media_log_;

  // Factory to create extra audio and video decoders.
  // Could be nullptr if not extra decoders are available.
  media::DecoderFactory* const decoder_factory_;

  // Creates factories for supporting video accelerators. May be null.
  GetGpuFactoriesCB get_gpu_factories_cb_;

  blink::BrowserInterfaceBrokerProxy* const interface_broker_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_FUCHSIA_RENDERER_FACTORY_H_
