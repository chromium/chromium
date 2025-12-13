// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_MONITOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"
#include "remoting/protocol/mouse_cursor_monitor.h"

namespace remoting {

class PipewireMouseCursorMonitor
    : public protocol::MouseCursorMonitor,
      public PipewireMouseCursorCapturer::Observer {
 public:
  explicit PipewireMouseCursorMonitor(
      base::WeakPtr<PipewireMouseCursorCapturer> capturer);
  ~PipewireMouseCursorMonitor() override;

  // MouseCursorMonitor implementation.
  void Init(Callback* callback) override;
  void SetPreferredCaptureInterval(base::TimeDelta interval) override;

 private:
  // PipewireMouseCursorCapturer::Observer overrides.
  void OnCursorShapeChanged(PipewireMouseCursorCapturer* capturer) override;
  void OnCursorPositionChanged(PipewireMouseCursorCapturer* capturer) override;

  void ReportInitialCursorInfo();

  PipewireMouseCursorCapturer::Observer::Subscription subscription_;
  base::WeakPtr<PipewireMouseCursorCapturer> capturer_;
  raw_ptr<Callback> callback_;
  base::WeakPtrFactory<PipewireMouseCursorMonitor> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_MONITOR_H_
