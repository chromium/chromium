// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_HELPER_H_
#define REMOTING_CODEC_VIDEO_ENCODER_HELPER_H_

#include <memory>

#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {
class DesktopFrame;
class DesktopRegion;
}  // namespace webrtc

namespace remoting {

class VideoPacket;

class VideoEncoderHelper {
 public:
  VideoEncoderHelper();

  VideoEncoderHelper(const VideoEncoderHelper&) = delete;
  VideoEncoderHelper& operator=(const VideoEncoderHelper&) = delete;

  // Returns a new VideoPacket with common fields (e.g. capture_time_ms, rects
  // list, frame shape if any) initialized based on the supplied |frame|.
  // Screen width and height will be set iff |frame|'s size differs from that
  // of the previously-supplied frame.
  std::unique_ptr<VideoPacket> CreateVideoPacket(
      const webrtc::DesktopFrame& frame);

  // Returns a new VideoPacket with the common fields populated from |frame|,
  // but the updated rects overridden by |updated_region|. This is useful for
  // encoders which alter the updated region e.g. by expanding it to macroblock
  // boundaries.
  std::unique_ptr<VideoPacket> CreateVideoPacketWithUpdatedRegion(
      const webrtc::DesktopFrame& frame,
      const webrtc::DesktopRegion& updated_region);

 private:
  // The most recent screen size. Used to detect screen size changes.
  webrtc::DesktopSize screen_size_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_HELPER_H_
