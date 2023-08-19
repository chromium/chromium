// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_ENCODER_METRICS_PROVIDER_H_
#define MEDIA_BASE_VIDEO_ENCODER_METRICS_PROVIDER_H_

#include "media/base/encoder_status.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoEncoderMetricsProvider {
 public:
  virtual ~VideoEncoderMetricsProvider() = default;
  void Initialize(VideoCodecProfile codec_profile,
                  const gfx::Size& encode_size,
                  bool is_hardware_encoder) {
    Initialize(codec_profile, encode_size, is_hardware_encoder,
               SVCScalabilityMode::kL1T1);
  }
  virtual void Initialize(VideoCodecProfile codec_profile,
                          const gfx::Size& encode_size,
                          bool is_hardware_encoder,
                          SVCScalabilityMode svc_mode) = 0;
  virtual void IncrementEncodedFrameCount() = 0;
  virtual void SetError(const media::EncoderStatus& status) = 0;
};

}  // namespace media
#endif  // MEDIA_BASE_VIDEO_ENCODER_METRICS_PROVIDER_H_
