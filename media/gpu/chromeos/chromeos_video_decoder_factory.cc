// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/chromeos_video_decoder_factory.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "media/base/video_decoder.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_decoder.h"
#endif

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_video_decoder.h"
#endif

namespace media {

namespace {

// Gets a list of the available functions for creating VideoDecoders.
VideoDecoderPipeline::CreateDecoderFunctions GetCreateDecoderFunctions() {
  constexpr VideoDecoderPipeline::CreateDecoderFunction kCreateVDFuncs[] = {
#if BUILDFLAG(USE_VAAPI)
    &VaapiVideoDecoder::Create,
#endif  // BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(USE_V4L2_CODEC)
    &V4L2VideoDecoder::Create,
#endif  // BUILDFLAG(USE_V4L2_CODEC)
  };

  return VideoDecoderPipeline::CreateDecoderFunctions(
      kCreateVDFuncs, kCreateVDFuncs + base::size(kCreateVDFuncs));
}

}  // namespace

// static
SupportedVideoDecoderConfigs ChromeosVideoDecoderFactory::GetSupportedConfigs(
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  SupportedVideoDecoderConfigs supported_configs;
  SupportedVideoDecoderConfigs configs;

#if BUILDFLAG(USE_VAAPI)
  configs = VaapiVideoDecoder::GetSupportedConfigs(workarounds);
  supported_configs.insert(supported_configs.end(), configs.begin(),
                           configs.end());
#endif  // BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(USE_V4L2_CODEC)
  configs = V4L2VideoDecoder::GetSupportedConfigs();
  supported_configs.insert(supported_configs.end(), configs.begin(),
                           configs.end());
#endif  // BUILDFLAG(USE_V4L2_CODEC)

  return supported_configs;
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
      base::BindRepeating(&GetCreateDecoderFunctions));
}

}  // namespace media
