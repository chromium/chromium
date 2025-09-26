// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_MOUSE_CURSOR_MONITOR_ADAPTOR_H_
#define REMOTING_PROTOCOL_WEBRTC_MOUSE_CURSOR_MONITOR_ADAPTOR_H_

#include <memory>

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
class WebrtcMouseCursorMonitorAdaptor : public MouseCursorMonitor {
 public:
  explicit WebrtcMouseCursorMonitorAdaptor(
      std::unique_ptr<webrtc::MouseCursorMonitor> monitor);
  ~WebrtcMouseCursorMonitorAdaptor() override;

  WebrtcMouseCursorMonitorAdaptor(const WebrtcMouseCursorMonitorAdaptor&) =
      delete;
  WebrtcMouseCursorMonitorAdaptor& operator=(
      const WebrtcMouseCursorMonitorAdaptor&) = delete;

  void Init(Callback* callback, Mode mode) override;
  void Capture() override;

 private:
  std::unique_ptr<webrtc::MouseCursorMonitor> monitor_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_MOUSE_CURSOR_MONITOR_ADAPTOR_H_
