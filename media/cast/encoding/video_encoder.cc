// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/video_encoder.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/cast/encoding/encoding_support.h"
#include "media/cast/encoding/external_video_encoder.h"
#include "media/cast/encoding/media_video_encoder_wrapper.h"
#include "media/cast/encoding/video_encoder_impl.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace media::cast {

namespace {

// UMA histogram for recording the encoder type
constexpr char kHistogramEncoderType[] =
    "CastStreaming.Sender.Video.EncoderType";

// A multidimensional enum for the encoder configuration, containing the cross
// product of the encoder codec and a boolean indicating whether it is software
// or hardware based.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CastStreamingVideoEncoderType {
  kUnknown = 0,
  kH264Software = 1,
  kH264Hardware = 2,
  kVP8Software = 3,
  kVP8Hardware = 4,
  kVP9Software = 5,
  kVP9Hardware = 6,
  kAV1Software = 7,
  kAV1Hardware = 8,

  kMaxValue = kAV1Hardware,  // Must equal the last "real" codec above.
};

CastStreamingVideoEncoderType ToEncoderType(media::VideoCodec codec,
                                            bool is_hardware) {
  switch (codec) {
    case media::VideoCodec::kUnknown:
      return CastStreamingVideoEncoderType::kUnknown;
    case media::VideoCodec::kH264:
      return is_hardware ? CastStreamingVideoEncoderType::kH264Hardware
                         : CastStreamingVideoEncoderType::kH264Software;
    case media::VideoCodec::kVP8:
      return is_hardware ? CastStreamingVideoEncoderType::kVP8Hardware
                         : CastStreamingVideoEncoderType::kVP8Software;
    case media::VideoCodec::kVP9:
      return is_hardware ? CastStreamingVideoEncoderType::kVP9Hardware
                         : CastStreamingVideoEncoderType::kVP9Software;
    case media::VideoCodec::kAV1:
      return is_hardware ? CastStreamingVideoEncoderType::kAV1Hardware
                         : CastStreamingVideoEncoderType::kAV1Software;
    default:
      NOTREACHED();
  }
}

}  // namespace

// static
std::unique_ptr<VideoEncoder> VideoEncoder::Create(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const FrameSenderConfig& video_config,
    std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
    StatusChangeCallback status_change_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  // Records the type of video encoder that was instantiated.
  base::UmaHistogramEnumeration(
      kHistogramEncoderType, ToEncoderType(video_config.video_codec(),
                                           video_config.use_hardware_encoder));

  // Use the media::VideoEncoder wrapper, if the feature is enabled.
  if (base::FeatureList::IsEnabled(media::kCastStreamingMediaVideoEncoder)) {
    return std::make_unique<MediaVideoEncoderWrapper>(
        cast_environment, video_config, std::move(metrics_provider),
        std::move(status_change_cb), gpu_factories);
  }

  // If the system provides a hardware-accelerated encoder, use it.
  if (video_config.use_hardware_encoder) {
    return std::make_unique<SizeAdaptableExternalVideoEncoder>(
        cast_environment, video_config, std::move(metrics_provider),
        std::move(status_change_cb), create_vea_cb);
  }

  // Otherwise we must have a software configuration.
  CHECK(encoding_support::IsSoftwareEnabled(video_config.video_codec()));
  return std::make_unique<VideoEncoderImpl>(cast_environment, video_config,
                                            std::move(metrics_provider),
                                            status_change_cb);
}

}  // namespace media::cast
