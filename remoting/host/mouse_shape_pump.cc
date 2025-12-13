// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_shape_pump.h"

#include <stdint.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

MouseShapePump::MouseShapePump(
    std::unique_ptr<protocol::MouseCursorMonitor> mouse_cursor_monitor,
    protocol::CursorShapeStub* cursor_shape_stub)
    : mouse_cursor_monitor_(std::move(mouse_cursor_monitor)),
      cursor_shape_stub_(cursor_shape_stub) {
  mouse_cursor_monitor_->Init(this);
}

MouseShapePump::~MouseShapePump() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MouseShapePump::SetCursorCaptureInterval(base::TimeDelta new_interval) {
  mouse_cursor_monitor_->SetPreferredCaptureInterval(new_interval);
}

void MouseShapePump::SetSendCursorPositionToClient(
    bool send_cursor_position_to_client) {
  if (send_cursor_position_to_client == send_cursor_position_to_client_) {
    return;
  }
  send_cursor_position_to_client_ = send_cursor_position_to_client;
  if (!send_cursor_position_to_client_) {
    // Send an empty HostCursorPosition to the client to disable rendering of
    // the host's cursor and revert to tracking the cursor position locally.
    protocol::HostCursorPosition position;
    cursor_shape_stub_->SetHostCursorPosition(position);
  }
}

void MouseShapePump::SetMouseCursorMonitorCallback(
    protocol::MouseCursorMonitor::Callback* callback) {
  callback_ = callback;
}

void MouseShapePump::OnMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> cursor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!cursor) {
    return;
  }

  if (cursor_shape_stub_) {
    std::unique_ptr<protocol::CursorShapeInfo> cursor_proto(
        new protocol::CursorShapeInfo());

    cursor_proto->set_data(std::string());
    if (cursor->image() && cursor->image()->data()) {
      cursor_proto->set_width(cursor->image()->size().width());
      cursor_proto->set_height(cursor->image()->size().height());
      cursor_proto->set_hotspot_x(cursor->hotspot().x());
      cursor_proto->set_hotspot_y(cursor->hotspot().y());
      if (cursor->image()->dpi().x() > 0) {
        cursor_proto->set_dpi(cursor->image()->dpi().x());
      }

      CHECK_EQ(cursor->image()->pixel_format(), webrtc::FOURCC_ARGB);
      uint8_t* current_row = cursor->image()->data();
      for (int y = 0; y < cursor->image()->size().height(); ++y) {
        cursor_proto->mutable_data()->append(
            current_row, UNSAFE_TODO(current_row +
                                     cursor->image()->size().width() *
                                         webrtc::DesktopFrame::kBytesPerPixel));
        UNSAFE_TODO(current_row += cursor->image()->stride());
      }
    } else {
      cursor_proto->set_width(0);
      cursor_proto->set_height(0);
      cursor_proto->set_hotspot_x(0);
      cursor_proto->set_hotspot_y(0);
    }

    cursor_shape_stub_->SetCursorShape(*cursor_proto);
  }

  if (callback_) {
    callback_->OnMouseCursor(std::move(cursor));
  }
}

void MouseShapePump::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  if (callback_) {
    callback_->OnMouseCursorPosition(position);
  }
}

void MouseShapePump::OnMouseCursorFractionalPosition(
    const protocol::FractionalCoordinate& fractional_position) {
  if (!send_cursor_position_to_client_) {
    return;
  }

  if (cursor_shape_stub_) {
    protocol::HostCursorPosition position;
    *position.mutable_fractional_coordinate() = fractional_position;
    cursor_shape_stub_->SetHostCursorPosition(position);
  }

  if (callback_) {
    callback_->OnMouseCursorFractionalPosition(fractional_position);
  }
}

}  // namespace remoting
