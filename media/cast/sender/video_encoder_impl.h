// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_VIDEO_ENCODER_IMPL_H_
#define MEDIA_CAST_SENDER_VIDEO_ENCODER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/sender/software_video_encoder.h"
#include "media/cast/sender/video_encoder.h"

namespace media {
class VideoFrame;

namespace cast {

// This object is called external from the main cast thread and internally from
// the video encoder thread.
class VideoEncoderImpl : public VideoEncoder {
 public:
  struct CodecDynamicConfig {
    bool key_frame_requested;
    int bit_rate;
  };

  // Returns true if VideoEncoderImpl can be used with the given |video_config|.
  static bool IsSupported(const FrameSenderConfig& video_config);

  VideoEncoderImpl(scoped_refptr<CastEnvironment> cast_environment,
                   const FrameSenderConfig& video_config,
                   const StatusChangeCallback& status_change_cb);

  ~VideoEncoderImpl() final;

  // VideoEncoder implementation.
  bool EncodeVideoFrame(
      scoped_refptr<media::VideoFrame> video_frame,
      const base::TimeTicks& reference_time,
      const FrameEncodedCallback& frame_encoded_callback) final;
  void SetBitRate(int new_bit_rate) final;
  void GenerateKeyFrame() final;

 private:
  scoped_refptr<CastEnvironment> cast_environment_;
  CodecDynamicConfig dynamic_config_;

  // This member belongs to the video encoder thread. It must not be
  // dereferenced on the main thread. We manage the lifetime of this member
  // manually because it needs to be initialize, used and destroyed on the
  // video encoder thread and video encoder thread can out-live the main thread.
  std::unique_ptr<SoftwareVideoEncoder> encoder_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_VIDEO_ENCODER_IMPL_H_
