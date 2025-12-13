// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_local_input_monitor.h"

#include "remoting/host/client_session_control.h"
#include "ui/events/types/event_type.h"

namespace remoting {

PipewireLocalInputMonitor::PipewireLocalInputMonitor(
    PipewireMouseCursorCapturer& cursor_capturer) {
  cursor_subscription_ = cursor_capturer.AddObserver(this);
}

PipewireLocalInputMonitor::~PipewireLocalInputMonitor() = default;

void PipewireLocalInputMonitor::StartMonitoringForClientSession(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  client_session_control_ = client_session_control;
}

void PipewireLocalInputMonitor::StartMonitoring(
    PointerMoveCallback on_pointer_input,
    KeyPressedCallback on_keyboard_input,
    base::RepeatingClosure on_error) {
  // TODO: crbug.com/453133338 - Hook up `on_keyboard_input`.
  on_pointer_input_ = std::move(on_pointer_input);
}

void PipewireLocalInputMonitor::OnCursorPositionChanged(
    PipewireMouseCursorCapturer* capturer) {
  auto position = capturer->GetLatestGlobalCursorPosition();
  DCHECK(position);
  if (client_session_control_) {
    client_session_control_->OnLocalPointerMoved(*position,
                                                 ui::EventType::kMouseMoved);
  }
  if (on_pointer_input_) {
    on_pointer_input_.Run(*position, ui::EventType::kMouseMoved);
  }
}

}  // namespace remoting
