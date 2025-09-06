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
    base::WeakPtr<PipewireMouseCursorCapturer> capturer)
    : capturer_(capturer) {}

PipewireMouseCursorMonitor::~PipewireMouseCursorMonitor() {
  if (capturer_) {
    // Prevent `callback` from being called.
    capturer_->SetCallback(nullptr, Mode::SHAPE_AND_POSITION);
  }
}

void PipewireMouseCursorMonitor::Init(Callback* callback, Mode mode) {
  if (capturer_) {
    capturer_->SetCallback(callback, mode);
  }
}

void PipewireMouseCursorMonitor::Capture() {
  if (capturer_) {
    capturer_->Capture();
  }
}

}  // namespace remoting
