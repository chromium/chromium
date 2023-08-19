// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/video_encoder.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/cast/encoding/encoding_support.h"
#include "media/cast/encoding/external_video_encoder.h"
#include "media/cast/encoding/video_encoder_impl.h"

#if BUILDFLAG(IS_APPLE)
#include "media/cast/encoding/h264_vt_encoder.h"
#endif

namespace media::cast {

// static
std::unique_ptr<VideoEncoder> VideoEncoder::Create(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const FrameSenderConfig& video_config,
    std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
    StatusChangeCallback status_change_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb) {
// On MacOS and iOS: attempt to use the system VideoToolbox library to
// perform optimized H.264 encoding.
//
// TODO(https://crbug.com/1015482): currently we don't check the toolbox encoder
// due to hardware encoding being disabled on Apple platforms.
#if BUILDFLAG(IS_APPLE)
  if (video_config.use_hardware_encoder &&
      H264VideoToolboxEncoder::IsSupported(video_config)) {
    return base::WrapUnique<VideoEncoder>(new H264VideoToolboxEncoder(
        cast_environment, video_config, std::move(metrics_provider),
        status_change_cb));
  }
#endif  // BUILDFLAG(IS_APPLE)

  // If the system provides a hardware-accelerated encoder, use it.
  if (video_config.use_hardware_encoder) {
    return base::WrapUnique<VideoEncoder>(new SizeAdaptableExternalVideoEncoder(
        cast_environment, video_config, std::move(metrics_provider),
        std::move(status_change_cb), create_vea_cb));
  }

  // Otherwise we must have a software configuration.
  DCHECK(encoding_support::IsSoftwareEnabled(video_config.codec));
  return base::WrapUnique<VideoEncoder>(
      new VideoEncoderImpl(cast_environment, video_config,
                           std::move(metrics_provider), status_change_cb));
}

std::unique_ptr<VideoFrameFactory> VideoEncoder::CreateVideoFrameFactory() {
  return nullptr;
}

void VideoEncoder::EmitFrames() {}

}  // namespace media::cast
