// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_CAST_SOFTWARE_VIDEO_ENCODER_ADAPTER_H_
#define REMOTING_CODEC_CAST_SOFTWARE_VIDEO_ENCODER_ADAPTER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "remoting/codec/webrtc_video_encoder.h"

namespace media {
class VideoFrame;
namespace cast {
struct SenderEncodedFrame;
class SoftwareVideoEncoder;
}  // namespace cast
}  // namespace media

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

// Implements WebrtcVideoEncoder by using a media::cast::SoftwareVideoEncoder.
class CastSoftwareVideoEncoderAdapter final : public WebrtcVideoEncoder {
 public:
  CastSoftwareVideoEncoderAdapter(
      std::unique_ptr<media::cast::SoftwareVideoEncoder> encoder,
      webrtc::VideoCodecType codec);
  ~CastSoftwareVideoEncoderAdapter() override;

  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& param,
              EncodeCallback done) override;

 private:
  scoped_refptr<media::VideoFrame> CreateVideoFrame(
      const webrtc::DesktopFrame& frame) const;
  std::unique_ptr<EncodedFrame> CreateEncodedFrame(
      const webrtc::DesktopFrame& frame,
      media::cast::SenderEncodedFrame&& media_frame) const;

  const std::unique_ptr<media::cast::SoftwareVideoEncoder> encoder_;
  const webrtc::VideoCodecType codec_;
  // The timestamp of the first Encode() function call. is_null() also indicates
  // that this instance is not initialized.
  base::TimeTicks start_timestamp_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_CAST_SOFTWARE_VIDEO_ENCODER_ADAPTER_H_
