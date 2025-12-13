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
class MouseShapePump : public protocol::MouseCursorMonitor::Callback {
 public:
  MouseShapePump(
      std::unique_ptr<protocol::MouseCursorMonitor> mouse_cursor_monitor,
      protocol::CursorShapeStub* cursor_shape_stub);

  MouseShapePump(const MouseShapePump&) = delete;
  MouseShapePump& operator=(const MouseShapePump&) = delete;

  ~MouseShapePump() override;

  void SetCursorCaptureInterval(base::TimeDelta new_interval);
  void SetSendCursorPositionToClient(bool send_cursor_position_to_client);

  // Sets the callback to which to delegate the OnMouseCursor() and
  // OnMouseCursorPosition() methods. This is used to chain the MouseShapePump
  // to other MouseCursorMonitor::Callback implementations.
  void SetMouseCursorMonitorCallback(
      protocol::MouseCursorMonitor::Callback* callback);

 private:
  // protocol::MouseCursorMonitor::Callback interface.
  void OnMouseCursor(
      std::unique_ptr<webrtc::MouseCursor> mouse_cursor) override;
  void OnMouseCursorPosition(const webrtc::DesktopVector& position) override;
  void OnMouseCursorFractionalPosition(
      const protocol::FractionalCoordinate& fractional_position) override;

  base::ThreadChecker thread_checker_;
  std::unique_ptr<protocol::MouseCursorMonitor> mouse_cursor_monitor_;
  raw_ptr<protocol::CursorShapeStub> cursor_shape_stub_;

  raw_ptr<protocol::MouseCursorMonitor::Callback> callback_ = nullptr;
  bool send_cursor_position_to_client_ = false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOUSE_SHAPE_PUMP_H_
