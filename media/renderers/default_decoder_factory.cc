// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/default_decoder_factory.h"

#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/viz/common/gpu/context_provider.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/libaom/libaom_buildflags.h"

#if !defined(OS_ANDROID)
#include "media/filters/decrypting_audio_decoder.h"
#include "media/filters/decrypting_video_decoder.h"
#endif

#if defined(OS_FUCHSIA)
#include "fuchsia/engine/switches.h"
#include "media/filters/fuchsia/fuchsia_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBAOM_DECODER)
#include "media/filters/aom_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_DAV1D_DECODER)
#include "media/filters/dav1d_video_decoder.h"
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
  if (base::FeatureList::IsEnabled(kExternalClearKeyForTesting)) {
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
  if (external_decoder_factory_ && gpu_factories &&
      gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    // |gpu_factories_| requires that its entry points be called on its
    // |GetTaskRunner()|. Since |pipeline_| will own decoders created from the
    // factories, require that their message loops are identical.
    DCHECK_EQ(gpu_factories->GetTaskRunner(), task_runner);

    external_decoder_factory_->CreateVideoDecoders(
        task_runner, gpu_factories, media_log, request_overlay_info_cb,
        target_color_space, video_decoders);
  }

#if defined(OS_FUCHSIA)
  if (gpu_factories) {
    video_decoders->push_back(CreateFuchsiaVideoDecoder(
        gpu_factories->SharedImageInterface(),
        gpu_factories->GetMediaContextProvider()->ContextSupport()));
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSoftwareVideoDecoders)) {
    // Bypass software codec registration.
    return;
  }
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
  video_decoders->push_back(std::make_unique<OffloadingVpxVideoDecoder>());
#endif

#if BUILDFLAG(ENABLE_DAV1D_DECODER)
  video_decoders->push_back(
      std::make_unique<OffloadingDav1dVideoDecoder>(media_log));
#elif BUILDFLAG(ENABLE_LIBAOM_DECODER)
  video_decoders->push_back(std::make_unique<AomVideoDecoder>(media_log));
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  video_decoders->push_back(std::make_unique<FFmpegVideoDecoder>(media_log));
#endif
}

}  // namespace media
