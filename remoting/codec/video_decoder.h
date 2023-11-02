// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_DECODER_H_
#define REMOTING_CODEC_VIDEO_DECODER_H_

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class VideoPacket;

// Interface for a decoder that decodes video packets.
class VideoDecoder {
 public:
  // List of supported pixel formats needed by various platforms.
  enum class PixelFormat { BGRA, RGBA };

  VideoDecoder() {}
  virtual ~VideoDecoder() {}

  virtual void SetPixelFormat(PixelFormat pixel_format) = 0;

  // Decodes a video frame. Returns false in case of a failure. The caller must
  // pre-allocate a |frame| with the size specified in the |packet|.
  virtual bool DecodePacket(const VideoPacket& packet,
                            webrtc::DesktopFrame* frame) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_H_
