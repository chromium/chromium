// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_VPX_ENCODER_H_
#define MEDIA_CAST_ENCODING_VPX_ENCODER_H_

#include <stdint.h>

#include "base/memory/raw_ref.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/feedback_signal_accumulator.h"
#include "media/cast/cast_config.h"
#include "media/cast/common/frame_id.h"
#include "media/cast/encoding/software_video_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoEncoderMetricsProvider;
class VideoFrame;

namespace cast {

class VpxEncoder final : public SoftwareVideoEncoder {
 public:
  VpxEncoder(const FrameSenderConfig& video_config,
             std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider);

  ~VpxEncoder() final;

  VpxEncoder(const VpxEncoder&) = delete;
  VpxEncoder& operator=(const VpxEncoder&) = delete;
  VpxEncoder(VpxEncoder&&) = delete;
  VpxEncoder& operator=(VpxEncoder&&) = delete;

  // SoftwareVideoEncoder implementations.
  void Initialize() final;
  void Encode(scoped_refptr<media::VideoFrame> video_frame,
              base::TimeTicks reference_time,
              SenderEncodedFrame* encoded_frame) final;
  void UpdateRates(uint32_t new_bitrate) final;
  void GenerateKeyFrame() final;

 private:
  bool is_initialized() const {
    // ConfigureForNewFrameSize() sets the timebase denominator value to
    // non-zero if the encoder is successfully initialized, and it is zero
    // otherwise.
    return config_.g_timebase.den != 0;
  }

  // If the |encoder_| is live, attempt reconfiguration to allow it to encode
  // frames at a new |frame_size|.  Otherwise, tear it down and re-create a new
  // |encoder_| instance.
  void ConfigureForNewFrameSize(const gfx::Size& frame_size);

  const FrameSenderConfig cast_config_;
  const raw_ref<const VideoCodecParams> codec_params_;

  const double target_encoder_utilization_;

  const std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider_;

  // VPX internal objects.  These are valid for use only while is_initialized()
  // returns true.
  vpx_codec_enc_cfg_t config_;
  vpx_codec_ctx_t encoder_;

  // Set to true to request the next frame emitted by VpxEncoder be a key frame.
  bool key_frame_requested_;

  // Saves the current bitrate setting, for when the |encoder_| is reconfigured
  // for different frame sizes.
  int bitrate_kbit_;

  // The |VideoFrame::timestamp()| of the last encoded frame.  This is used to
  // predict the duration of the next frame.
  base::TimeDelta last_frame_timestamp_;

  // The ID for the next frame to be emitted.
  FrameId next_frame_id_;

  // This is bound to the thread where Initialize() is called.
  THREAD_CHECKER(thread_checker_);

  // The accumulator (time averaging) of the encoding speed.
  FeedbackSignalAccumulator<base::TimeDelta> encoding_speed_acc_;

  // The higher the speed, the less CPU usage, and the lower quality.
  int encoding_speed_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_VPX_ENCODER_H_
