// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/codec/video_encoder_verbatim.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

static uint8_t* GetPacketOutputBuffer(VideoPacket* packet, size_t size) {
  packet->mutable_data()->resize(size);
  return reinterpret_cast<uint8_t*>(std::data(*packet->mutable_data()));
}

VideoEncoderVerbatim::VideoEncoderVerbatim() = default;
VideoEncoderVerbatim::~VideoEncoderVerbatim() = default;

std::unique_ptr<VideoPacket> VideoEncoderVerbatim::Encode(
    const webrtc::DesktopFrame& frame) {
  DCHECK(frame.data());

  // If nothing has changed in the frame then return NULL to indicate that
  // we don't need to actually send anything (e.g. nothing to top-off).
  if (frame.updated_region().is_empty()) {
    return nullptr;
  }

  // Create a VideoPacket with common fields (e.g. DPI, rects, shape) set.
  std::unique_ptr<VideoPacket> packet(helper_.CreateVideoPacket(frame));
  packet->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VERBATIM);

  // Calculate output size.
  size_t output_size = 0;
  for (webrtc::DesktopRegion::Iterator iter(frame.updated_region());
       !iter.IsAtEnd(); iter.Advance()) {
    const webrtc::DesktopRect& rect = iter.rect();
    output_size +=
        rect.width() * rect.height() * webrtc::DesktopFrame::kBytesPerPixel;
  }

  uint8_t* out = GetPacketOutputBuffer(packet.get(), output_size);
  const int in_stride = frame.stride();

  // Encode pixel data for all changed rectangles into the packet.
  for (webrtc::DesktopRegion::Iterator iter(frame.updated_region());
       !iter.IsAtEnd(); iter.Advance()) {
    const webrtc::DesktopRect& rect = iter.rect();
    const int row_size = webrtc::DesktopFrame::kBytesPerPixel * rect.width();
    const uint8_t* in = frame.data() + rect.top() * in_stride +
                        rect.left() * webrtc::DesktopFrame::kBytesPerPixel;
    for (int y = rect.top(); y < rect.top() + rect.height(); ++y) {
      memcpy(out, in, row_size);
      out += row_size;
      in += in_stride;
    }
  }

  return packet;
}

}  // namespace remoting
