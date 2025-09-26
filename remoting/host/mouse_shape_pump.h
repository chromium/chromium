// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOUSE_SHAPE_PUMP_H_
#define REMOTING_HOST_MOUSE_SHAPE_PUMP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "remoting/protocol/mouse_cursor_monitor.h"

namespace remoting {

namespace protocol {
class CursorShapeStub;
}  // namespace protocol

// MouseShapePump is responsible for capturing mouse shape using
// MouseCursorMonitor and sending it to a CursorShapeStub.
// TODO: crbug.com/447440351 - Maybe rename this class to CursorInfoPump.
class MouseShapePump : public MouseCursorMonitor::Callback {
 public:
  // This initializes `mouse_cursor_monitor` to capture both the cursor
  // shape and position. The caller should not set any monitor-callback on
  // `mouse_cursor_monitor` - it will be overwritten by this class.
  // `cursor_shape_stub` is optional - if provided, mouse-cursor messages will
  // be sent to it.
  MouseShapePump(std::unique_ptr<MouseCursorMonitor> mouse_cursor_monitor,
                 protocol::CursorShapeStub* cursor_shape_stub);

  MouseShapePump(const MouseShapePump&) = delete;
  MouseShapePump& operator=(const MouseShapePump&) = delete;

  ~MouseShapePump() override;

  // Restarts the mouse shape capture timer using |new_capture_interval|.
  void SetCursorCaptureInterval(base::TimeDelta new_capture_interval);

  // Sets or unsets the callback to which to delegate MouseCursorMonitor events
  // after they have been processed.
  void SetMouseCursorMonitorCallback(MouseCursorMonitor::Callback* callback);

  // Sets whether the fractional cursor position should be sent to the client
  // for client side cursor rendering in relative mouse mode.
  void SetSendCursorPositionToClient(bool send_cursor_position_to_client);

 private:
  void Capture();

  void StartCaptureTimer(base::TimeDelta capture_interval);

  // MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* mouse_cursor) override;
  void OnMouseCursorPosition(const webrtc::DesktopVector& position) override;
  void OnMouseCursorFractionalPosition(webrtc::ScreenId screen_id,
                                       float fractional_x,
                                       float fractional_y) override;

  base::ThreadChecker thread_checker_;
  std::unique_ptr<MouseCursorMonitor> mouse_cursor_monitor_;
  raw_ptr<protocol::CursorShapeStub> cursor_shape_stub_;

  base::RepeatingTimer capture_timer_;
  raw_ptr<MouseCursorMonitor::Callback> callback_ = nullptr;
  bool send_cursor_position_to_client_ = false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOUSE_SHAPE_PUMP_H_
