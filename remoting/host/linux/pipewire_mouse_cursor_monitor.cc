// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_mouse_cursor_monitor.h"

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

PipewireMouseCursorMonitor::PipewireMouseCursorMonitor(
    base::WeakPtr<PipewireMouseCursorCapturer> capturer)
    : capturer_(capturer) {}

PipewireMouseCursorMonitor::~PipewireMouseCursorMonitor() = default;

void PipewireMouseCursorMonitor::Init(Callback* callback) {
  if (!capturer_) {
    return;
  }
  callback_ = callback;
  subscription_ = capturer_->AddObserver(this);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PipewireMouseCursorMonitor::ReportInitialCursorInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PipewireMouseCursorMonitor::SetPreferredCaptureInterval(
    base::TimeDelta interval) {
  // No-op since callback will be run once cursor is changed.
}

void PipewireMouseCursorMonitor::OnCursorShapeChanged(
    PipewireMouseCursorCapturer* capturer) {
  if (!callback_) {
    return;
  }
  auto cursor = capturer->GetLatestCursor();
  if (cursor) {
    callback_->OnMouseCursor(std::move(cursor));
  }
}

void PipewireMouseCursorMonitor::OnCursorPositionChanged(
    PipewireMouseCursorCapturer* capturer) {
  if (!callback_) {
    return;
  }
  auto position = capturer->GetLatestFractionalCursorPosition();
  if (position) {
    callback_->OnMouseCursorFractionalPosition(*position);
  }
}

void PipewireMouseCursorMonitor::ReportInitialCursorInfo() {
  if (!capturer_) {
    return;
  }
  OnCursorShapeChanged(capturer_.get());
  OnCursorPositionChanged(capturer_.get());
}

}  // namespace remoting
