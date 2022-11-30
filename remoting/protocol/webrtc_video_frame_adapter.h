// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_FRAME_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_FRAME_ADAPTER_H_

#include <memory>

#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/api/video/video_frame.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting::protocol {

// Adapter class to wrap a DesktopFrame produced by the capturer, and provide
// it as a VideoFrame to the WebRTC video sink. The encoder will extract the
// captured DesktopFrame from VideoFrame::video_frame_buffer().
class WebrtcVideoFrameAdapter : public webrtc::VideoFrameBuffer {
 public:
  WebrtcVideoFrameAdapter(
      std::unique_ptr<webrtc::DesktopFrame> frame,
      std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats);
  ~WebrtcVideoFrameAdapter() override;
  WebrtcVideoFrameAdapter(const WebrtcVideoFrameAdapter&) = delete;
  WebrtcVideoFrameAdapter& operator=(const WebrtcVideoFrameAdapter&) = delete;

  // Returns a VideoFrame that wraps the provided DesktopFrame.
  static webrtc::VideoFrame CreateVideoFrame(
      std::unique_ptr<webrtc::DesktopFrame> desktop_frame,
      std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats);

  // Used by the encoder. After this returns, the adapter no longer wraps a
  // DesktopFrame.
  std::unique_ptr<webrtc::DesktopFrame> TakeDesktopFrame();

  // Called by the encoder to transfer the frame stats out of this adapter
  // into the EncodedFrame. The encoder will also set the encode start/end
  // times.
  std::unique_ptr<WebrtcVideoEncoder::FrameStats> TakeFrameStats();

  // webrtc::VideoFrameBuffer overrides.
  Type type() const override;
  int width() const override;
  int height() const override;
  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;

 private:
  std::unique_ptr<webrtc::DesktopFrame> frame_;
  webrtc::DesktopSize frame_size_;
  std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_FRAME_ADAPTER_H_
