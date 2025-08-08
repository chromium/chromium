// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_MONITOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

class PipewireMouseCursorMonitor : public webrtc::MouseCursorMonitor {
 public:
  explicit PipewireMouseCursorMonitor(
      base::WeakPtr<PipewireCaptureStream> stream);
  ~PipewireMouseCursorMonitor() override;
  // MouseCursorMonitor implementation.
  void Init(Callback* callback, Mode mode) override;
  void Capture() override;

 private:
  raw_ptr<Callback> callback_;
  bool report_position_;
  base::WeakPtr<PipewireCaptureStream> stream_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_MONITOR_H_
