// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_MEDIA_RENDERER_FACTORY_H_
#define FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_MEDIA_RENDERER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "fuchsia_web/webengine/mojom/web_engine_media_resource_provider.mojom.h"
#include "media/base/renderer_factory.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
class AudioRendererSink;
class DecoderFactory;
class GpuVideoAcceleratorFactories;
class MediaLog;
class VideoDecoder;
class VideoRendererSink;
}  // namespace media

// RendererFactory implementation used on Fuchsia. It works the same as
// RendererImplFactory, except that it uses WebEngineAudioRenderer for audio.
class WebEngineMediaRendererFactory final : public media::RendererFactory {
 public:
  using GetGpuFactoriesCB =
      base::RepeatingCallback<media::GpuVideoAcceleratorFactories*()>;

  WebEngineMediaRendererFactory(
      media::MediaLog* media_log,
      media::DecoderFactory* decoder_factory,
      GetGpuFactoriesCB get_gpu_factories_cb,
      mojo::Remote<mojom::WebEngineMediaResourceProvider>
          media_resource_provider);
  ~WebEngineMediaRendererFactory() override;

  // RendererFactory interface.
  std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) override;

 private:
  std::vector<std::unique_ptr<media::VideoDecoder>> CreateVideoDecoders(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      media::GpuVideoAcceleratorFactories* gpu_factories);

  media::MediaLog* const media_log_;

  // Factory to create extra audio and video decoders.
  // Could be nullptr if not extra decoders are available.
  media::DecoderFactory* const decoder_factory_;

  // Creates factories for supporting video accelerators. May be null.
  GetGpuFactoriesCB get_gpu_factories_cb_;

  mojo::Remote<mojom::WebEngineMediaResourceProvider> media_resource_provider_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_MEDIA_RENDERER_FACTORY_H_
