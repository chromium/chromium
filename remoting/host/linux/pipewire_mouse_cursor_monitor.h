// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_MONITOR_H_

#include "base/memory/weak_ptr.h"
#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"
#include "remoting/protocol/mouse_cursor_monitor.h"

namespace remoting {

class PipewireMouseCursorMonitor : public MouseCursorMonitor {
 public:
  explicit PipewireMouseCursorMonitor(
      base::WeakPtr<PipewireMouseCursorCapturer> capturer);
  ~PipewireMouseCursorMonitor() override;
  // MouseCursorMonitor implementation.
  void Init(Callback* callback, Mode mode) override;
  void Capture() override;

 private:
  base::WeakPtr<PipewireMouseCursorCapturer> capturer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_MONITOR_H_
