// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_RENDERER_IMPL_FACTORY_H_
#define MEDIA_RENDERERS_RENDERER_IMPL_FACTORY_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/base/media_player_logging_id.h"
#include "media/base/renderer_factory.h"

#if !BUILDFLAG(IS_ANDROID)
#include "media/base/speech_recognition_client.h"
#endif

namespace media {

class AudioBuffer;
class AudioDecoder;
class AudioRendererSink;
class DecoderFactory;
class GpuVideoAcceleratorFactories;
class MediaLog;
class VideoDecoder;
class VideoRendererSink;

using CreateAudioDecodersCB =
    base::RepeatingCallback<std::vector<std::unique_ptr<AudioDecoder>>()>;
using CreateVideoDecodersCB =
    base::RepeatingCallback<std::vector<std::unique_ptr<VideoDecoder>>()>;

// The default factory class for creating RendererImpl.
class MEDIA_EXPORT RendererImplFactory final : public RendererFactory {
 public:
  using GetGpuFactoriesCB =
      base::RepeatingCallback<GpuVideoAcceleratorFactories*()>;

#if BUILDFLAG(IS_ANDROID)
  RendererImplFactory(MediaLog* media_log,
                      DecoderFactory* decoder_factory,
                      const GetGpuFactoriesCB& get_gpu_factories_cb,
                      MediaPlayerLoggingID media_player_id);
#else
  RendererImplFactory(
      MediaLog* media_log,
      DecoderFactory* decoder_factory,
      const GetGpuFactoriesCB& get_gpu_factories_cb,
      MediaPlayerLoggingID media_player_id,
      std::unique_ptr<SpeechRecognitionClient> speech_recognition_client);
#endif

  RendererImplFactory(const RendererImplFactory&) = delete;
  RendererImplFactory& operator=(const RendererImplFactory&) = delete;

  ~RendererImplFactory() final;

  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) final;

 private:
  std::vector<std::unique_ptr<AudioDecoder>> CreateAudioDecoders(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner);
  std::vector<std::unique_ptr<VideoDecoder>> CreateVideoDecoders(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      GpuVideoAcceleratorFactories* gpu_factories);

  raw_ptr<MediaLog> media_log_;

  // Factory to create extra audio and video decoders.
  // Could be nullptr if not extra decoders are available.
  raw_ptr<DecoderFactory, DanglingUntriaged> decoder_factory_;

  // Creates factories for supporting video accelerators. May be null.
  GetGpuFactoriesCB get_gpu_factories_cb_;

  // WebMediaPlayerImpl id.
  MediaPlayerLoggingID media_player_id_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<SpeechRecognitionClient> speech_recognition_client_;
#endif
};

}  // namespace media

#endif  // MEDIA_RENDERERS_RENDERER_IMPL_FACTORY_H_
