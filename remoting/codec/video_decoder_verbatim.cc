// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/codec/video_decoder_verbatim.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/notreached.h"
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
  const char* current_data_pos = packet.data().data();
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

    int rect_row_size = kBytesPerPixel * rect.width();
    const char* rect_data_end =
        current_data_pos + rect_row_size * rect.height();
    if (rect_data_end > packet.data().data() + packet.data().size()) {
      LOG(ERROR) << "Invalid packet received.";
      return false;
    }

    uint8_t* source =
        reinterpret_cast<uint8_t*>(const_cast<char*>(current_data_pos));
    frame->CopyPixelsFrom(source, rect_row_size, rect);
    current_data_pos = rect_data_end;
  }

  if (current_data_pos != packet.data().data() + packet.data().size()) {
    LOG(ERROR) << "Invalid packet received.";
    return false;
  }

  return true;
}

}  // namespace remoting
