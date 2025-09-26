// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_PROTOCOL_MOUSE_CURSOR_MONITOR_H_

#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

// This class is similar to webrtc::MouseCursorMonitor, but provides a
// OnMouseCursorFractionalPosition() method in the callback. We can't make
// this class extend webrtc::MouseCursorMonitor, because Chromium C++ has RTTI
// disabled so we can't just pass webrtc::MouseCursorMonitor::Callback to Init()
// and have subclasses to do dynamic casts.
//
// The plan is to update all subclasses to only call
// OnMouseCursorFractionalPosition(), and remove OnMouseCursorPosition() from
// Callback. Currently if a subclass calls OnMouseCursorPosition() then the
// position will be passed to DesktopAndCursorConditionalComposer for host side
// cursor rendering in relative mouse mode, meanwhile if a subclass calls
// OnMouseCursorFractionalPosition(), it will be sent to the client for client
// side rendering of the cursor. In the longer term, all host platforms should
// do client side cursor rendering in relative mouse mode.
class MouseCursorMonitor {
 public:
  using Mode = webrtc::MouseCursorMonitor::Mode;

  class Callback : public webrtc::MouseCursorMonitor::Callback {
   public:
    // Called in response to Capture(). See comment in
    // remoting/proto/coordinates.proto.
    virtual void OnMouseCursorFractionalPosition(webrtc::ScreenId screen_id,
                                                 float fractional_x,
                                                 float fractional_y) {}
  };

  virtual ~MouseCursorMonitor() = default;

  // Initializes the monitor with the `callback`, which must remain valid until
  // capturer is destroyed.
  virtual void Init(Callback* callback, Mode mode) = 0;

  // Captures current cursor shape and position (depending on the `mode` passed
  // to Init()). Calls Callback::OnMouseCursor() if cursor shape has
  // changed since the last call (or when Capture() is called for the first
  // time) and then Callback::OnMouseCursorPosition() if mode is set to
  // SHAPE_AND_POSITION.
  virtual void Capture() = 0;
};

#endif  // REMOTING_PROTOCOL_MOUSE_CURSOR_MONITOR_H_
