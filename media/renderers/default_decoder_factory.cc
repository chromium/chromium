// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/default_decoder_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/filters/gpu_video_decoder.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/libaom/av1_buildflags.h"

#if !defined(OS_ANDROID)
#include "media/filters/decrypting_audio_decoder.h"
#include "media/filters/decrypting_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_AV1_DECODER)
#include "media/filters/aom_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

namespace media {

DefaultDecoderFactory::DefaultDecoderFactory(
    std::unique_ptr<DecoderFactory> external_decoder_factory)
    : external_decoder_factory_(std::move(external_decoder_factory)) {}

DefaultDecoderFactory::~DefaultDecoderFactory() = default;

void DefaultDecoderFactory::CreateAudioDecoders(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) {
#if !defined(OS_ANDROID)
  // DecryptingAudioDecoder is only needed in External Clear Key testing to
  // cover the audio decrypt-and-decode path.
  if (base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting)) {
    audio_decoders->push_back(
        std::make_unique<DecryptingAudioDecoder>(task_runner, media_log));
  }
#endif

#if BUILDFLAG(ENABLE_FFMPEG)
  audio_decoders->push_back(
      std::make_unique<FFmpegAudioDecoder>(task_runner, media_log));
#endif

  if (external_decoder_factory_) {
    external_decoder_factory_->CreateAudioDecoders(task_runner, media_log,
                                                   audio_decoders);
  }
}

void DefaultDecoderFactory::CreateVideoDecoders(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    const RequestOverlayInfoCB& request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) {
#if !defined(OS_ANDROID)
  video_decoders->push_back(
      std::make_unique<DecryptingVideoDecoder>(task_runner, media_log));
#endif

  // Perfer an external decoder since one will only exist if it is hardware
  // accelerated.
  // Remember that |gpu_factories| will be null if HW video decode is turned
  // off in chrome://flags.
  if (gpu_factories) {
    // |gpu_factories_| requires that its entry points be called on its
    // |GetTaskRunner()|. Since |pipeline_| will own decoders created from the
    // factories, require that their message loops are identical.
    DCHECK_EQ(gpu_factories->GetTaskRunner(), task_runner);

    if (external_decoder_factory_) {
      external_decoder_factory_->CreateVideoDecoders(
          task_runner, gpu_factories, media_log, request_overlay_info_cb,
          target_color_space, video_decoders);
    }

    // MojoVideoDecoder replaces any VDA for this platform when it's enabled.
    if (!base::FeatureList::IsEnabled(media::kMojoVideoDecoder)) {
      video_decoders->push_back(std::make_unique<GpuVideoDecoder>(
          gpu_factories, request_overlay_info_cb, target_color_space,
          media_log));
    }
  }

#if BUILDFLAG(ENABLE_LIBVPX)
  video_decoders->push_back(std::make_unique<OffloadingVpxVideoDecoder>());
#endif

#if BUILDFLAG(ENABLE_AV1_DECODER)
  if (base::FeatureList::IsEnabled(kAv1Decoder))
    video_decoders->push_back(std::make_unique<AomVideoDecoder>(media_log));
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  video_decoders->push_back(std::make_unique<FFmpegVideoDecoder>(media_log));
#endif
}

}  // namespace media
