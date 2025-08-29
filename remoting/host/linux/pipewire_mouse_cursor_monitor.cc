// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_mouse_cursor_monitor.h"

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

PipewireMouseCursorMonitor::PipewireMouseCursorMonitor(
    base::WeakPtr<const PipewireCaptureStreamManager> stream_manager)
    : stream_manager_(std::move(stream_manager)) {}

PipewireMouseCursorMonitor::~PipewireMouseCursorMonitor() = default;

void PipewireMouseCursorMonitor::Init(Callback* callback, Mode mode) {
  callback_ = callback;
  report_position_ = mode == SHAPE_AND_POSITION;
}

void PipewireMouseCursorMonitor::Capture() {
  if (!stream_manager_) {
    return;
  }

  auto active_streams = stream_manager_->GetActiveStreams();
  bool need_position = report_position_;
  bool need_cursor = true;
  for (auto [screen_id, stream] : active_streams) {
    if (!stream) {
      continue;
    }

    if (need_position) {
      // Any stream can capture the cursor position.
      std::optional<webrtc::DesktopVector> cursor_position =
          stream->CaptureCursorPosition();
      if (cursor_position.has_value()) {
        callback_->OnMouseCursorPosition(*cursor_position);
        need_position = false;
      }
    }

    if (need_cursor) {
      // Only the stream where the cursor is currently located can capture the
      // cursor.
      std::unique_ptr<webrtc::MouseCursor> cursor = stream->CaptureCursor();
      if (cursor && cursor->image()->data()) {
        callback_->OnMouseCursor(cursor.release());
        need_cursor = false;
      }
    }

    if (!need_position && !need_cursor) {
      break;
    }
  }
}

}  // namespace remoting
