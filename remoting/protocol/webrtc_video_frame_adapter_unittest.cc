// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_frame_adapter.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using webrtc::BasicDesktopFrame;
using webrtc::DesktopFrame;
using webrtc::DesktopSize;

namespace {

std::unique_ptr<DesktopFrame> MakeDesktopFrame(int width, int height) {
  return std::make_unique<BasicDesktopFrame>(DesktopSize(width, height));
}

}  // namespace

namespace remoting {
namespace protocol {

TEST(WebrtcVideoFrameAdapter, CreateVideoFrameWrapsDesktopFrame) {
  auto desktop_frame = MakeDesktopFrame(100, 200);
  DesktopFrame* desktop_frame_ptr = desktop_frame.get();

  webrtc::VideoFrame video_frame =
      WebrtcVideoFrameAdapter::CreateVideoFrame(std::move(desktop_frame));

  auto* adapter = static_cast<WebrtcVideoFrameAdapter*>(
      video_frame.video_frame_buffer().get());
  auto wrapped_desktop_frame = adapter->TakeDesktopFrame();
  EXPECT_EQ(wrapped_desktop_frame.get(), desktop_frame_ptr);
}

TEST(WebrtcVideoFrameAdapter, AdapterHasCorrectSize) {
  auto desktop_frame = MakeDesktopFrame(100, 200);
  rtc::scoped_refptr<WebrtcVideoFrameAdapter> adapter =
      new rtc::RefCountedObject<WebrtcVideoFrameAdapter>(
          std::move(desktop_frame));

  EXPECT_EQ(100, adapter->width());
  EXPECT_EQ(200, adapter->height());
}

}  // namespace protocol
}  // namespace remoting
