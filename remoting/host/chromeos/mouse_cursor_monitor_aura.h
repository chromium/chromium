// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_MOUSE_CURSOR_MONITOR_AURA_H_
#define REMOTING_HOST_CHROMEOS_MOUSE_CURSOR_MONITOR_AURA_H_

#include "base/memory/raw_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/point.h"

namespace remoting {

// Monitors changes in cursor shape and location.  It can be constructed on any
// thread but all public methods must be called on the capturer thread.
class MouseCursorMonitorAura : public webrtc::MouseCursorMonitor {
 public:
  MouseCursorMonitorAura();

  MouseCursorMonitorAura(const MouseCursorMonitorAura&) = delete;
  MouseCursorMonitorAura& operator=(const MouseCursorMonitorAura&) = delete;

  // webrtc::MouseCursorMonitor implementation.
  void Init(Callback* callback, Mode mode) override;
  void Capture() override;

 private:
  void NotifyCursorChanged(const ui::Cursor& cursor);

  raw_ptr<Callback> callback_;
  Mode mode_;
  ui::Cursor last_cursor_;
  gfx::Point last_mouse_location_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_MOUSE_CURSOR_MONITOR_AURA_H_
