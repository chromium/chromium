// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_VERBATIM_H_
#define REMOTING_CODEC_VIDEO_ENCODER_VERBATIM_H_

#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_helper.h"

namespace remoting {

// VideoEncoderVerbatim implements a VideoEncoder that sends image data as a
// sequence of RGB values, without compression.
class VideoEncoderVerbatim : public VideoEncoder {
 public:
  VideoEncoderVerbatim();

  VideoEncoderVerbatim(const VideoEncoderVerbatim&) = delete;
  VideoEncoderVerbatim& operator=(const VideoEncoderVerbatim&) = delete;

  ~VideoEncoderVerbatim() override;

  // VideoEncoder interface.
  std::unique_ptr<VideoPacket> Encode(
      const webrtc::DesktopFrame& frame) override;

 private:
  VideoEncoderHelper helper_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_VERBATIM_H_
