// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBRTC_MOUSE_CURSOR_MONITOR_ADAPTOR_H_
#define REMOTING_HOST_WEBRTC_MOUSE_CURSOR_MONITOR_ADAPTOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

// An adaptor that adapts webrtc::MouseCursorMonitor to
// remoting::MouseCursorMonitor.
class WebrtcMouseCursorMonitorAdaptor : public protocol::MouseCursorMonitor,
                                        webrtc::MouseCursorMonitor::Callback {
 public:
  static base::TimeDelta GetDefaultCaptureInterval();

  WebrtcMouseCursorMonitorAdaptor(
      std::unique_ptr<webrtc::MouseCursorMonitor> cursor_monitor,
      std::unique_ptr<DesktopDisplayInfoMonitor> display_info_monitor);
  ~WebrtcMouseCursorMonitorAdaptor() override;

  WebrtcMouseCursorMonitorAdaptor(const WebrtcMouseCursorMonitorAdaptor&) =
      delete;
  WebrtcMouseCursorMonitorAdaptor& operator=(
      const WebrtcMouseCursorMonitorAdaptor&) = delete;

  void Init(protocol::MouseCursorMonitor::Callback* callback) override;
  void SetPreferredCaptureInterval(base::TimeDelta interval) override;

 private:
  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* cursor) override;
  void OnMouseCursorPosition(const webrtc::DesktopVector& position) override;

  void StartCaptureTimer(base::TimeDelta capture_interval);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<webrtc::MouseCursorMonitor> cursor_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<DesktopDisplayInfoMonitor> display_info_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<protocol::MouseCursorMonitor::Callback> callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingTimer capture_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBRTC_MOUSE_CURSOR_MONITOR_ADAPTOR_H_
