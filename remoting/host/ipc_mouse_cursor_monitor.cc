// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_mouse_cursor_monitor.h"

#include "remoting/host/desktop_session_proxy.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

IpcMouseCursorMonitor::IpcMouseCursorMonitor(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : callback_(nullptr), desktop_session_proxy_(desktop_session_proxy) {}

IpcMouseCursorMonitor::~IpcMouseCursorMonitor() = default;

void IpcMouseCursorMonitor::Init(Callback* callback, Mode mode) {
  DCHECK(!callback_);
  DCHECK(callback);
  DCHECK_EQ(webrtc::MouseCursorMonitor::SHAPE_ONLY, mode);
  callback_ = callback;
  desktop_session_proxy_->SetMouseCursorMonitor(weak_factory_.GetWeakPtr());
}

void IpcMouseCursorMonitor::Capture() {
  // Ignore. DesktopSessionAgent will capture the cursor at the same time it
  // captures a screen frame when |IpcVideoFrameCapturer::Capture()| is called.
  // This saves an IPC roundtrip.
}

void IpcMouseCursorMonitor::OnMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> cursor) {
  DCHECK(callback_);
  callback_->OnMouseCursor(cursor.release());
}

}  // namespace remoting

