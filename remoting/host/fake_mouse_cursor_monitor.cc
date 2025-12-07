// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_mouse_cursor_monitor.h"

#include <memory>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

FakeMouseCursorMonitor::FakeMouseCursorMonitor() : callback_(nullptr) {}

FakeMouseCursorMonitor::~FakeMouseCursorMonitor() = default;

void FakeMouseCursorMonitor::Init(MouseCursorMonitor::Callback* callback) {
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = callback;
}

void FakeMouseCursorMonitor::SetPreferredCaptureInterval(
    base::TimeDelta interval) {}

}  // namespace remoting
