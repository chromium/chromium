// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_VP8_ENCODER_H_
#define MEDIA_CAST_SENDER_VP8_ENCODER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/feedback_signal_accumulator.h"
#include "media/cast/cast_config.h"
#include "media/cast/sender/software_video_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoFrame;
}

namespace media {
namespace cast {

class Vp8Encoder : public SoftwareVideoEncoder {
 public:
  explicit Vp8Encoder(const FrameSenderConfig& video_config);

  ~Vp8Encoder() final;

  // SoftwareVideoEncoder implementations.
  void Initialize() final;
  void Encode(scoped_refptr<media::VideoFrame> video_frame,
              const base::TimeTicks& reference_time,
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

  const double target_encoder_utilization_;

  // VP8 internal objects.  These are valid for use only while is_initialized()
  // returns true.
  vpx_codec_enc_cfg_t config_;
  vpx_codec_ctx_t encoder_;

  // Set to true to request the next frame emitted by Vp8Encoder be a key frame.
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
  base::ThreadChecker thread_checker_;

  // The accumulator (time averaging) of the encoding speed.
  FeedbackSignalAccumulator<base::TimeDelta> encoding_speed_acc_;

  // The higher the speed, the less CPU usage, and the lower quality.
  int encoding_speed_;

  DISALLOW_COPY_AND_ASSIGN(Vp8Encoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_VP8_ENCODER_H_
