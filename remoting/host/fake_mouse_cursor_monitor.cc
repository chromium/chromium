// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_mouse_cursor_monitor.h"

#include <memory>

#include "base/check.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

FakeMouseCursorMonitor::FakeMouseCursorMonitor() : callback_(nullptr) {}

FakeMouseCursorMonitor::~FakeMouseCursorMonitor() = default;

void FakeMouseCursorMonitor::Init(
    webrtc::MouseCursorMonitor::Callback* callback,
    webrtc::MouseCursorMonitor::Mode mode) {
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = callback;
}

void FakeMouseCursorMonitor::Capture() {
  DCHECK(callback_);

  const int kWidth = 32;
  const int kHeight = 32;

  std::unique_ptr<webrtc::DesktopFrame> desktop_frame(
      new webrtc::BasicDesktopFrame(webrtc::DesktopSize(kWidth, kHeight)));
  memset(desktop_frame->data(), 0xFF,
         webrtc::DesktopFrame::kBytesPerPixel * kWidth * kHeight);

  std::unique_ptr<webrtc::MouseCursor> mouse_cursor(new webrtc::MouseCursor(
      desktop_frame.release(), webrtc::DesktopVector()));

  callback_->OnMouseCursor(mouse_cursor.release());
}

}  // namespace remoting
