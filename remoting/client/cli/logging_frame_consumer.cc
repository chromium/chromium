// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/cli/logging_frame_consumer.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "remoting/client/common/logging.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

LoggingFrameConsumer::LoggingFrameConsumer() = default;

LoggingFrameConsumer::~LoggingFrameConsumer() = default;

std::unique_ptr<webrtc::DesktopFrame> LoggingFrameConsumer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  return std::make_unique<webrtc::BasicDesktopFrame>(size);
}

void LoggingFrameConsumer::DrawFrame(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    base::OnceClosure done) {
  CLIENT_LOG << "DrawFrame size: " << frame->size().width() << "x"
             << frame->size().height();
  std::move(done).Run();
}

protocol::FrameConsumer::PixelFormat LoggingFrameConsumer::GetPixelFormat() {
  return FORMAT_BGRA;
}

}  // namespace remoting
