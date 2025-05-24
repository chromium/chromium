// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_FAKE_SOFTWARE_VIDEO_ENCODER_H_
#define MEDIA_CAST_ENCODING_FAKE_SOFTWARE_VIDEO_ENCODER_H_

#include <stdint.h>

#include "media/base/video_encoder_metrics_provider.h"
#include "media/cast/cast_config.h"
#include "media/cast/common/frame_id.h"
#include "media/cast/encoding/software_video_encoder.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace cast {

class FakeSoftwareVideoEncoder final : public SoftwareVideoEncoder {
 public:
  explicit FakeSoftwareVideoEncoder(
      const FrameSenderConfig& video_config,
      std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider);
  ~FakeSoftwareVideoEncoder() final;

  // SoftwareVideoEncoder implementations.
  void Initialize() final;
  void Encode(scoped_refptr<media::VideoFrame> video_frame,
              base::TimeTicks reference_time,
              SenderEncodedFrame* encoded_frame) final;
  void UpdateRates(uint32_t new_bitrate) final;
  void GenerateKeyFrame() final;

 private:
  const FrameSenderConfig video_config_;
  std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider_;

  gfx::Size last_frame_size_;
  bool next_frame_is_key_ = true;
  FrameId frame_id_ = FrameId::first();
  int frame_size_ = 0u;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_FAKE_SOFTWARE_VIDEO_ENCODER_H_
