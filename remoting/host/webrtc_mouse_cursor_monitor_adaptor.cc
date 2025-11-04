// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webrtc_mouse_cursor_monitor_adaptor.h"

#include "base/memory/ptr_util.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

namespace {
// Poll mouse shape at least 10 times a second.
constexpr base::TimeDelta kMaxCursorCaptureInterval = base::Milliseconds(100);

// Poll mouse shape at most 100 times a second.
constexpr base::TimeDelta kMinCursorCaptureInterval = base::Milliseconds(10);
}  // namespace

// static
base::TimeDelta WebrtcMouseCursorMonitorAdaptor::GetDefaultCaptureInterval() {
  return kMaxCursorCaptureInterval;
}

WebrtcMouseCursorMonitorAdaptor::WebrtcMouseCursorMonitorAdaptor(
    std::unique_ptr<webrtc::MouseCursorMonitor> monitor)
    : monitor_(std::move(monitor)) {}

WebrtcMouseCursorMonitorAdaptor::~WebrtcMouseCursorMonitorAdaptor() = default;

void WebrtcMouseCursorMonitorAdaptor::Init(
    protocol::MouseCursorMonitor::Callback* callback) {
  callback_ = callback;
  monitor_->Init(this, webrtc::MouseCursorMonitor::SHAPE_AND_POSITION);
  StartCaptureTimer(GetDefaultCaptureInterval());
}

void WebrtcMouseCursorMonitorAdaptor::SetPreferredCaptureInterval(
    base::TimeDelta interval) {
  StartCaptureTimer(std::clamp(interval, kMinCursorCaptureInterval,
                               kMaxCursorCaptureInterval));
}

void WebrtcMouseCursorMonitorAdaptor::OnMouseCursor(
    webrtc::MouseCursor* cursor) {
  callback_->OnMouseCursor(base::WrapUnique(cursor));
}

void WebrtcMouseCursorMonitorAdaptor::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  callback_->OnMouseCursorPosition(position);
}

void WebrtcMouseCursorMonitorAdaptor::StartCaptureTimer(
    base::TimeDelta capture_interval) {
  capture_timer_.Start(FROM_HERE, capture_interval,
                       base::BindRepeating(&webrtc::MouseCursorMonitor::Capture,
                                           base::Unretained(monitor_.get())));
}

}  // namespace remoting
