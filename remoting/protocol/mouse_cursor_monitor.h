// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_PROTOCOL_MOUSE_CURSOR_MONITOR_H_

#include <memory>

#include "base/time/time.h"
#include "remoting/proto/coordinates.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting::protocol {

// This class is similar to webrtc::MouseCursorMonitor, but provides a
// OnMouseCursorFractionalPosition() method in the callback, and does not
// require calling Capture() in a constant interval.
class MouseCursorMonitor {
 public:
  class Callback {
   public:
    virtual ~Callback() = default;

    // Called when the cursor shape has changed.
    virtual void OnMouseCursor(std::unique_ptr<webrtc::MouseCursor> cursor) {}

    // Called when the cursor position has changed. `position` indicates cursor
    // absolute position on the system in fullscreen coordinate, i.e. the
    // top-left monitor always starts from (0, 0).
    // The coordinates of the position is controlled by OS, but it's always
    // consistent with DesktopFrame.rect().top_left().
    // TODO: crbug.com/455622961 - Remove this method once the
    // clientRenderedHostCursor capability is fully rolled out.
    virtual void OnMouseCursorPosition(const webrtc::DesktopVector& position) {}

    // Called when the cursor position has changed.
    // See comment in remoting/proto/coordinates.proto.
    virtual void OnMouseCursorFractionalPosition(
        const FractionalCoordinate& fractional_position) {}
  };

  virtual ~MouseCursorMonitor() = default;

  // Initializes the monitor with the `callback`, which must remain valid until
  // capturer is destroyed.
  // An implementation may either call one of the
  // OnMouseCursorPosition/OnMouseCursorFractionalPosition methods, or both,
  // whenever the cursor position is changed, depending on what it supports.
  virtual void Init(Callback* callback) = 0;

  // Sets the preferred interval between two cursor captures. Note that not all
  // implementations may honor this value; an implementation could either
  // maintain its own capture frequency, or use a push model that doesn't poll.
  virtual void SetPreferredCaptureInterval(base::TimeDelta interval) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_MOUSE_CURSOR_MONITOR_H_
