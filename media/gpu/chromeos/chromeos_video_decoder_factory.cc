// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/chromeos_video_decoder_factory.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/video_decoder.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_decoder.h"
#elif BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_video_decoder.h"
#else
#error Either VA-API or V4L2 must be used for decode acceleration on Chrome OS.
#endif

namespace media {

// static
SupportedVideoDecoderConfigs ChromeosVideoDecoderFactory::GetSupportedConfigs(
    const gpu::GpuDriverBugWorkarounds& workarounds) {
#if BUILDFLAG(USE_VAAPI)
  auto configs = VaapiVideoDecoder::GetSupportedConfigs();
#elif BUILDFLAG(USE_V4L2_CODEC)
  auto configs = V4L2VideoDecoder::GetSupportedConfigs();
#endif

  if (workarounds.disable_accelerated_vp8_decode) {
    base::EraseIf(configs, [](const auto& config) {
      return config.profile_min >= VP8PROFILE_MIN &&
             config.profile_max <= VP8PROFILE_MAX;
    });
  }

  if (workarounds.disable_accelerated_vp9_decode) {
    base::EraseIf(configs, [](const auto& config) {
      return config.profile_min >= VP9PROFILE_PROFILE0 &&
             config.profile_max <= VP9PROFILE_PROFILE0;
    });
  }

  if (workarounds.disable_accelerated_vp9_profile2_decode) {
    base::EraseIf(configs, [](const auto& config) {
      return config.profile_min >= VP9PROFILE_PROFILE2 &&
             config.profile_max <= VP9PROFILE_PROFILE2;
    });
  }

  return configs;
}
// static
std::unique_ptr<VideoDecoder> ChromeosVideoDecoderFactory::Create(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter,
    std::unique_ptr<MediaLog> media_log) {
  return VideoDecoderPipeline::Create(
      std::move(client_task_runner), std::move(frame_pool),
      std::move(frame_converter), std::move(media_log),
#if BUILDFLAG(USE_VAAPI)
      base::BindRepeating(&VaapiVideoDecoder::Create)
#elif BUILDFLAG(USE_V4L2_CODEC)
      base::BindRepeating(&V4L2VideoDecoder::Create)
#endif
  );
}

}  // namespace media
