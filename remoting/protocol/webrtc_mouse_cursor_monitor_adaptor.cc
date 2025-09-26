// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_mouse_cursor_monitor_adaptor.h"

namespace remoting {

WebrtcMouseCursorMonitorAdaptor::WebrtcMouseCursorMonitorAdaptor(
    std::unique_ptr<webrtc::MouseCursorMonitor> monitor)
    : monitor_(std::move(monitor)) {}

WebrtcMouseCursorMonitorAdaptor::~WebrtcMouseCursorMonitorAdaptor() = default;

void WebrtcMouseCursorMonitorAdaptor::Init(Callback* callback, Mode mode) {
  monitor_->Init(callback, mode);
}

void WebrtcMouseCursorMonitorAdaptor::Capture() {
  monitor_->Capture();
}

}  // namespace remoting
