// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_MOUSE_CURSOR_MONITOR_WIN_H_
#define REMOTING_HOST_WIN_MOUSE_CURSOR_MONITOR_WIN_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/win/desktop_event_handler.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

// A MouseCursorMonitor implementation for Windows. Unlike WebRTC's
// MouseCursorMonitor, which is polling, this implementation is event-driven and
// has much better performance.
class MouseCursorMonitorWin : public protocol::MouseCursorMonitor {
 public:
  explicit MouseCursorMonitorWin(
      std::unique_ptr<DesktopDisplayInfoMonitor> display_monitor);
  ~MouseCursorMonitorWin() override;

  MouseCursorMonitorWin(const MouseCursorMonitorWin&) = delete;
  MouseCursorMonitorWin& operator=(const MouseCursorMonitorWin&) = delete;

  // MouseCursorMonitor implementation.
  void Init(Callback* callback) override;
  void SetPreferredCaptureInterval(base::TimeDelta interval) override;

 private:
  friend class MouseCursorMonitorWinTest;
  class Delegate;

  void OnMouseCursor(std::unique_ptr<webrtc::MouseCursor> cursor);
  void OnMouseCursorPosition(const webrtc::DesktopVector& position);

  SEQUENCE_CHECKER(sequence_checker_);

  DesktopEventHandler event_handler_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<DesktopDisplayInfoMonitor> display_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<Callback> callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<MouseCursorMonitorWin> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_MOUSE_CURSOR_MONITOR_WIN_H_
