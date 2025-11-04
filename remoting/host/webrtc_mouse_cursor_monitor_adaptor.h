// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBRTC_MOUSE_CURSOR_MONITOR_ADAPTOR_H_
#define REMOTING_HOST_WEBRTC_MOUSE_CURSOR_MONITOR_ADAPTOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

// An adaptor that adapts webrtc::MouseCursorMonitor to
// remoting::MouseCursorMonitor.
//
// TODO: crbug.com/447440351 - Make this class call
// OnMouseCursorFractionalPosition() for client side cursor rendering. It will
// need to take a map of screen_id => DesktopCapturer to convert the global
// cursor coordinate into the fractional coordinate.
class WebrtcMouseCursorMonitorAdaptor : public protocol::MouseCursorMonitor,
                                        webrtc::MouseCursorMonitor::Callback {
 public:
  static base::TimeDelta GetDefaultCaptureInterval();

  explicit WebrtcMouseCursorMonitorAdaptor(
      std::unique_ptr<webrtc::MouseCursorMonitor> monitor);
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

  std::unique_ptr<webrtc::MouseCursorMonitor> monitor_;
  raw_ptr<protocol::MouseCursorMonitor::Callback> callback_;
  base::RepeatingTimer capture_timer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBRTC_MOUSE_CURSOR_MONITOR_ADAPTOR_H_
