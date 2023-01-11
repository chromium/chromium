// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_helper.h"

#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

VideoEncoderHelper::VideoEncoderHelper() = default;

std::unique_ptr<VideoPacket> VideoEncoderHelper::CreateVideoPacket(
    const webrtc::DesktopFrame& frame) {
  return CreateVideoPacketWithUpdatedRegion(frame, frame.updated_region());
}

std::unique_ptr<VideoPacket>
VideoEncoderHelper::CreateVideoPacketWithUpdatedRegion(
    const webrtc::DesktopFrame& frame,
    const webrtc::DesktopRegion& updated_region) {
  std::unique_ptr<VideoPacket> packet(new VideoPacket());

  // Set |screen_width| and |screen_height| iff they have changed.
  if (!frame.size().equals(screen_size_)) {
    screen_size_ = frame.size();

    VideoPacketFormat* format = packet->mutable_format();
    format->set_screen_width(screen_size_.width());
    format->set_screen_height(screen_size_.height());
  }

  // Record the list of changed rectangles.
  for (webrtc::DesktopRegion::Iterator iter(updated_region); !iter.IsAtEnd();
       iter.Advance()) {
    const webrtc::DesktopRect& rect = iter.rect();
    Rect* dirty_rect = packet->add_dirty_rects();
    dirty_rect->set_x(rect.left());
    dirty_rect->set_y(rect.top());
    dirty_rect->set_width(rect.width());
    dirty_rect->set_height(rect.height());
  }

  // Store frame DPI.
  if (!frame.dpi().is_zero()) {
    packet->mutable_format()->set_x_dpi(frame.dpi().x());
    packet->mutable_format()->set_y_dpi(frame.dpi().y());
  }

  return packet;
}

}  // namespace remoting
