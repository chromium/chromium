// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_frame_adapter.h"

#include <utility>

#include "base/notreached.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace remoting::protocol {

WebrtcVideoFrameAdapter::WebrtcVideoFrameAdapter(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats)
    : frame_(std::move(frame)),
      frame_size_(frame_->size()),
      frame_stats_(std::move(frame_stats)) {}

WebrtcVideoFrameAdapter::~WebrtcVideoFrameAdapter() = default;

// static
webrtc::VideoFrame WebrtcVideoFrameAdapter::CreateVideoFrame(
    std::unique_ptr<webrtc::DesktopFrame> desktop_frame,
    std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats) {
  // The frame-builder only accepts a bounding rectangle, so compute it here.
  // WebRTC only tracks and accumulates update-rectangles, not regions, when
  // sending video-frames to the encoder.
  webrtc::VideoFrame::UpdateRect video_update_rect{};
  for (webrtc::DesktopRegion::Iterator i(desktop_frame->updated_region());
       !i.IsAtEnd(); i.Advance()) {
    const auto& rect = i.rect();
    video_update_rect.Union(webrtc::VideoFrame::UpdateRect{
        rect.left(), rect.top(), rect.width(), rect.height()});
  }

  rtc::scoped_refptr<WebrtcVideoFrameAdapter> adapter(
      new rtc::RefCountedObject<WebrtcVideoFrameAdapter>(
          std::move(desktop_frame), std::move(frame_stats)));

  // In the empty case, it is important to set the video-frame's update
  // rectangle explicitly to empty, otherwise an unset value would be
  // interpreted as a full frame update - see webrtc::VideoFrame::update_rect().
  return webrtc::VideoFrame::Builder()
      .set_video_frame_buffer(adapter)
      .set_update_rect(video_update_rect)
      // Added for b/333806004 - webrtc::FrameCadenceAdapterImpl requires
      // video-frames to have monotonically increasing timestamps.
      .set_timestamp_us(base::TimeTicks::Now().since_origin().InMicroseconds())
      .build();
}

std::unique_ptr<webrtc::DesktopFrame>
WebrtcVideoFrameAdapter::TakeDesktopFrame() {
  return std::move(frame_);
}

std::unique_ptr<WebrtcVideoEncoder::FrameStats>
WebrtcVideoFrameAdapter::TakeFrameStats() {
  return std::move(frame_stats_);
}

webrtc::VideoFrameBuffer::Type WebrtcVideoFrameAdapter::type() const {
  return Type::kNative;
}

int WebrtcVideoFrameAdapter::width() const {
  return frame_size_.width();
}

int WebrtcVideoFrameAdapter::height() const {
  return frame_size_.height();
}

rtc::scoped_refptr<webrtc::I420BufferInterface>
WebrtcVideoFrameAdapter::ToI420() {
  // Strictly speaking all adapters must implement ToI420(), so that if the
  // external encoder fails, an internal libvpx could be used. But the remoting
  // encoder already uses libvpx, so there's no reason for fallback to happen.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace remoting::protocol
