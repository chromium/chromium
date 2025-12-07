// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_decoder_verbatim.h"

#include <cstddef>
#include <cstdint>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

static const int kBytesPerPixel = 4;

VideoDecoderVerbatim::VideoDecoderVerbatim() = default;
VideoDecoderVerbatim::~VideoDecoderVerbatim() = default;

void VideoDecoderVerbatim::SetPixelFormat(
    VideoDecoder::PixelFormat pixel_format) {
  NOTIMPLEMENTED();
}

bool VideoDecoderVerbatim::DecodePacket(const VideoPacket& packet,
                                        webrtc::DesktopFrame* frame) {
  webrtc::DesktopRegion* region = frame->mutable_updated_region();
  region->Clear();
  base::span<const uint8_t> packet_data = base::as_byte_span(packet.data());
  base::span<const uint8_t> remaining_data = packet_data;
  for (int i = 0; i < packet.dirty_rects_size(); ++i) {
    Rect proto_rect = packet.dirty_rects(i);
    webrtc::DesktopRect rect =
        webrtc::DesktopRect::MakeXYWH(proto_rect.x(), proto_rect.y(),
                                      proto_rect.width(), proto_rect.height());
    region->AddRect(rect);

    if (!DoesRectContain(webrtc::DesktopRect::MakeSize(frame->size()), rect)) {
      LOG(ERROR) << "Invalid packet received.";
      return false;
    }

    const size_t rect_data_size = kBytesPerPixel * rect.width() * rect.height();
    if (remaining_data.size() < rect_data_size) {
      LOG(ERROR) << "Invalid packet received.";
      return false;
    }

    frame->CopyPixelsFrom(remaining_data.data(), kBytesPerPixel * rect.width(),
                          rect);
    remaining_data = remaining_data.subspan(rect_data_size);
  }

  if (!remaining_data.empty()) {
    LOG(ERROR) << "Invalid packet received.";
    return false;
  }

  return true;
}

}  // namespace remoting
