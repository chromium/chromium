// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CAST_RENDERER_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_CAST_RENDERER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
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

// RendererFactory implementation for Cast. This class is similar to
// RendererImplFactory, but provides its own CastAudioRenderer for audio.
class CastRendererFactory final : public media::RendererFactory {
 public:
  using GetGpuFactoriesCB =
      base::RepeatingCallback<media::GpuVideoAcceleratorFactories*()>;

  CastRendererFactory(media::MediaLog* media_log,
                      media::DecoderFactory* decoder_factory,
                      GetGpuFactoriesCB get_gpu_factories_cb,
                      blink::BrowserInterfaceBrokerProxy* interface_broker);
  CastRendererFactory(const CastRendererFactory&) = delete;
  CastRendererFactory& operator=(const CastRendererFactory&) = delete;
  ~CastRendererFactory() final;

  // media::RendererFactory implementation:
  std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) final;

 private:
  std::vector<std::unique_ptr<media::VideoDecoder>> CreateVideoDecoders(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      media::GpuVideoAcceleratorFactories* gpu_factories);

  media::MediaLog* const media_log_;
  media::DecoderFactory* const decoder_factory_;
  blink::BrowserInterfaceBrokerProxy* const interface_broker_;
  const GetGpuFactoriesCB get_gpu_factories_cb_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CAST_RENDERER_FACTORY_H_
