// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_frame_adapter.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using webrtc::BasicDesktopFrame;
using webrtc::DesktopFrame;
using webrtc::DesktopRect;
using webrtc::DesktopSize;
using webrtc::VideoFrame;

namespace {

std::unique_ptr<DesktopFrame> MakeDesktopFrame(int width, int height) {
  return std::make_unique<BasicDesktopFrame>(DesktopSize(width, height));
}

}  // namespace

namespace remoting::protocol {

TEST(WebrtcVideoFrameAdapter, CreateVideoFrameWrapsDesktopFrame) {
  auto desktop_frame = MakeDesktopFrame(100, 200);
  auto frame_stats = std::make_unique<WebrtcVideoEncoder::FrameStats>();
  DesktopFrame* desktop_frame_ptr = desktop_frame.get();

  VideoFrame video_frame = WebrtcVideoFrameAdapter::CreateVideoFrame(
      std::move(desktop_frame), std::move(frame_stats));

  auto* adapter = static_cast<WebrtcVideoFrameAdapter*>(
      video_frame.video_frame_buffer().get());
  auto wrapped_desktop_frame = adapter->TakeDesktopFrame();
  EXPECT_EQ(wrapped_desktop_frame.get(), desktop_frame_ptr);
}

TEST(WebrtcVideoFrameAdapter, AdapterHasCorrectSize) {
  auto desktop_frame = MakeDesktopFrame(100, 200);
  auto frame_stats = std::make_unique<WebrtcVideoEncoder::FrameStats>();
  rtc::scoped_refptr<WebrtcVideoFrameAdapter> adapter(
      new rtc::RefCountedObject<WebrtcVideoFrameAdapter>(
          std::move(desktop_frame), std::move(frame_stats)));

  EXPECT_EQ(100, adapter->width());
  EXPECT_EQ(200, adapter->height());
}

TEST(WebrtcVideoFrameAdapter, EmptyUpdateRegionGivesFrameWithEmptyUpdateRect) {
  auto desktop_frame = MakeDesktopFrame(100, 200);
  ASSERT_TRUE(desktop_frame->updated_region().is_empty());
  auto frame_stats = std::make_unique<WebrtcVideoEncoder::FrameStats>();
  VideoFrame video_frame = WebrtcVideoFrameAdapter::CreateVideoFrame(
      std::move(desktop_frame), std::move(frame_stats));

  EXPECT_TRUE(video_frame.update_rect().IsEmpty());
}

TEST(WebrtcVideoFrameAdapter, VideoUpdateRectSpansDesktopUpdateRegion) {
  auto desktop_frame = MakeDesktopFrame(100, 200);
  desktop_frame->mutable_updated_region()->AddRect(
      DesktopRect::MakeLTRB(10, 20, 30, 40));
  desktop_frame->mutable_updated_region()->AddRect(
      DesktopRect::MakeLTRB(50, 60, 70, 80));

  auto frame_stats = std::make_unique<WebrtcVideoEncoder::FrameStats>();
  VideoFrame video_frame = WebrtcVideoFrameAdapter::CreateVideoFrame(
      std::move(desktop_frame), std::move(frame_stats));

  EXPECT_EQ(video_frame.update_rect(),
            (VideoFrame::UpdateRect{
                .offset_x = 10, .offset_y = 20, .width = 60, .height = 60}));
}

}  // namespace remoting::protocol
