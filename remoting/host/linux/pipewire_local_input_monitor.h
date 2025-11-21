// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_LOCAL_INPUT_MONITOR_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_LOCAL_INPUT_MONITOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

class PipewireLocalInputMonitor : public LocalInputMonitor,
                                  public PipewireMouseCursorCapturer::Observer {
 public:
  explicit PipewireLocalInputMonitor(
      PipewireMouseCursorCapturer& cursor_capturer);
  ~PipewireLocalInputMonitor() override;
  PipewireLocalInputMonitor(const PipewireLocalInputMonitor&) = delete;
  PipewireLocalInputMonitor& operator=(const PipewireLocalInputMonitor&) =
      delete;

  void StartMonitoringForClientSession(
      base::WeakPtr<ClientSessionControl> client_session_control) override;
  void StartMonitoring(PointerMoveCallback on_pointer_input,
                       KeyPressedCallback on_keyboard_input,
                       base::RepeatingClosure on_error) override;

 private:
  void OnCursorPositionChanged(PipewireMouseCursorCapturer* capturer) override;

  base::WeakPtr<ClientSessionControl> client_session_control_;
  PointerMoveCallback on_pointer_input_;
  PipewireMouseCursorCapturer::Observer::Subscription cursor_subscription_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_LOCAL_INPUT_MONITOR_H_
