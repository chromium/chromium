// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_PROTOCOL_MOUSE_CURSOR_MONITOR_H_

#include "base/time/time.h"
#include "remoting/proto/coordinates.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting::protocol {

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
    // See comment in remoting/proto/coordinates.proto.
    virtual void OnMouseCursorFractionalPosition(
        const FractionalCoordinate& fractional_position) {}
  };

  virtual ~MouseCursorMonitor() = default;

  // Initializes the monitor with the `callback`, which must remain valid until
  // capturer is destroyed.
  // `callback` will be called whenever the cursor shape or position is changed.
  virtual void Init(Callback* callback, Mode mode) = 0;

  // Sets the preferred interval between two cursor captures. Note that not all
  // implementations may honor this value; an implementation could either
  // maintain its own capture frequency, or use a push model that doesn't poll.
  virtual void SetPreferredCaptureInterval(base::TimeDelta interval) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_MOUSE_CURSOR_MONITOR_H_
