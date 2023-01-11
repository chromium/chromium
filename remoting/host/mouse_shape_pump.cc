// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_shape_pump.h"

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/timer/timer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

// Poll mouse shape 10 times a second.
static const int kCursorCaptureIntervalMs = 100;

MouseShapePump::MouseShapePump(
    std::unique_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor,
    protocol::CursorShapeStub* cursor_shape_stub)
    : mouse_cursor_monitor_(std::move(mouse_cursor_monitor)),
      cursor_shape_stub_(cursor_shape_stub) {
  mouse_cursor_monitor_->Init(this,
                              webrtc::MouseCursorMonitor::SHAPE_AND_POSITION);
  capture_timer_.Start(
      FROM_HERE, base::Milliseconds(kCursorCaptureIntervalMs),
      base::BindRepeating(&MouseShapePump::Capture, base::Unretained(this)));
}

MouseShapePump::~MouseShapePump() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MouseShapePump::SetMouseCursorMonitorCallback(
    webrtc::MouseCursorMonitor::Callback* callback) {
  callback_ = callback;
}

void MouseShapePump::Capture() {
  DCHECK(thread_checker_.CalledOnValidThread());

  mouse_cursor_monitor_->Capture();
}

void MouseShapePump::OnMouseCursor(webrtc::MouseCursor* cursor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<webrtc::MouseCursor> owned_cursor(cursor);

  std::unique_ptr<protocol::CursorShapeInfo> cursor_proto(
      new protocol::CursorShapeInfo());
  cursor_proto->set_width(cursor->image()->size().width());
  cursor_proto->set_height(cursor->image()->size().height());
  cursor_proto->set_hotspot_x(cursor->hotspot().x());
  cursor_proto->set_hotspot_y(cursor->hotspot().y());

  cursor_proto->set_data(std::string());
  uint8_t* current_row = cursor->image()->data();
  for (int y = 0; y < cursor->image()->size().height(); ++y) {
    cursor_proto->mutable_data()->append(
        current_row, current_row + cursor->image()->size().width() *
                                       webrtc::DesktopFrame::kBytesPerPixel);
    current_row += cursor->image()->stride();
  }

  cursor_shape_stub_->SetCursorShape(*cursor_proto);

  if (callback_) {
    callback_->OnMouseCursor(owned_cursor.release());
  }
}

void MouseShapePump::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  if (callback_) {
    callback_->OnMouseCursorPosition(position);
  }
}

}  // namespace remoting
