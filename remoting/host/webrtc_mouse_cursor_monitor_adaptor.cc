// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webrtc_mouse_cursor_monitor_adaptor.h"

#include <optional>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "remoting/proto/coordinates.pb.h"
#include "remoting/protocol/coordinate_conversion.h"
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
    std::unique_ptr<webrtc::MouseCursorMonitor> cursor_monitor,
    std::unique_ptr<DesktopDisplayInfoMonitor> display_info_monitor)
    : cursor_monitor_(std::move(cursor_monitor)),
      display_info_monitor_(std::move(display_info_monitor)) {}

WebrtcMouseCursorMonitorAdaptor::~WebrtcMouseCursorMonitorAdaptor() = default;

void WebrtcMouseCursorMonitorAdaptor::Init(
    MouseCursorMonitor::Callback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;
  cursor_monitor_->Init(this, webrtc::MouseCursorMonitor::SHAPE_AND_POSITION);
  display_info_monitor_->Start();
  StartCaptureTimer(GetDefaultCaptureInterval());
}

void WebrtcMouseCursorMonitorAdaptor::SetPreferredCaptureInterval(
    base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartCaptureTimer(std::clamp(interval, kMinCursorCaptureInterval,
                               kMaxCursorCaptureInterval));
}

void WebrtcMouseCursorMonitorAdaptor::OnMouseCursor(
    webrtc::MouseCursor* cursor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_->OnMouseCursor(base::WrapUnique(cursor));
}

void WebrtcMouseCursorMonitorAdaptor::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_->OnMouseCursorPosition(position);

  const auto* display_info = display_info_monitor_->GetLatestDisplayInfo();
  if (!display_info) {
    return;
  }
  std::optional<protocol::FractionalCoordinate> fractional_position =
      display_info->ToFractionalCoordinate(position);
  if (fractional_position.has_value()) {
    callback_->OnMouseCursorFractionalPosition(*fractional_position);
  } else {
    LOG(ERROR) << "Cursor position " << position.x() << ", " << position.y()
               << " is not within any display.";
  }
}

void WebrtcMouseCursorMonitorAdaptor::StartCaptureTimer(
    base::TimeDelta capture_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  capture_timer_.Start(
      FROM_HERE, capture_interval,
      base::BindRepeating(&webrtc::MouseCursorMonitor::Capture,
                          base::Unretained(cursor_monitor_.get())));
}

}  // namespace remoting
