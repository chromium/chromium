// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_mouse_cursor_monitor.h"

#include <memory>
#include <optional>

#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

PipewireMouseCursorMonitor::PipewireMouseCursorMonitor(
    base::WeakPtr<PipewireCaptureStream> stream)
    : stream_(std::move(stream)) {}

PipewireMouseCursorMonitor::~PipewireMouseCursorMonitor() = default;

void PipewireMouseCursorMonitor::Init(Callback* callback, Mode mode) {
  callback_ = callback;
  report_position_ = mode == SHAPE_AND_POSITION;
}

void PipewireMouseCursorMonitor::Capture() {
  if (!stream_) {
    return;
  }

  std::optional<webrtc::DesktopVector> mouse_cursor_position =
      stream_->CaptureCursorPosition();
  // Invalid cursor or position
  if (!mouse_cursor_position.has_value()) {
    callback_->OnMouseCursor(nullptr);
    return;
  }

  std::unique_ptr<webrtc::MouseCursor> mouse_cursor = stream_->CaptureCursor();

  if (mouse_cursor && mouse_cursor->image()->data()) {
    callback_->OnMouseCursor(mouse_cursor.release());
  }

  if (report_position_) {
    callback_->OnMouseCursorPosition(*mouse_cursor_position);
  }
}

}  // namespace remoting
