// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_TOUCH_INPUT_STRATEGY_H_
#define REMOTING_CLIENT_INPUT_TOUCH_INPUT_STRATEGY_H_

#include "remoting/client/ui/view_matrix.h"

namespace remoting {

class DesktopViewport;

// This is an interface used by GestureInterpreter to customize the way gestures
// are handled.
class TouchInputStrategy {
 public:
  enum TouchFeedbackType {
    TAP_FEEDBACK,
    DRAG_FEEDBACK,
  };

  enum Gesture {
    NONE,
    ZOOM,
    DRAG,
  };

  virtual ~TouchInputStrategy() {}

  // Called when the GestureInterpreter receives a zoom gesture. The
  // implementation is responsible for modifying the viewport and observing the
  // change.
  virtual void HandleZoom(const ViewMatrix::Point& pivot,
                          float scale,
                          DesktopViewport* viewport) = 0;

  // Called when the GestureInterpreter receives a pan gesture. The
  // implementation is responsible for modifying the viewport and observing the
  // change.
  // simultaneous_gesture: Gesture that is simultaneously in progress.
  // Returns true if this changes the cursor position.
  virtual bool HandlePan(const ViewMatrix::Vector2D& translation,
                         Gesture simultaneous_gesture,
                         DesktopViewport* viewport) = 0;

  // Called when a touch input (which will end up injecting a mouse event at
  // certain position in the host) is done at |touch_point|.
  // The implementation should move the cursor to proper position.
  //
  // Returns true if |touch_point| is a valid input, false otherwise. If the
  // input is not valid, the implementation should not change its cursor
  // position.
  virtual bool TrackTouchInput(const ViewMatrix::Point& touch_point,
                               const DesktopViewport& viewport) = 0;

  // Returns the current cursor position.
  virtual ViewMatrix::Point GetCursorPosition() const = 0;

  // Focuses the viewport on the cursor position if necessary.
  virtual void FocusViewportOnCursor(DesktopViewport* viewport) const = 0;

  // Maps a vector (or movement) in the surface coordinate to the vector to be
  // used on the desktop. For example it can be used to map a scroll gesture
  // on the screen to change in mouse wheel position.
  virtual ViewMatrix::Vector2D MapScreenVectorToDesktop(
      const ViewMatrix::Vector2D& delta,
      const DesktopViewport& viewport) const = 0;

  // Returns the maximum radius of the feedback animation on the surface's
  // coordinate for the given input type. The feedback will then be shown on the
  // cursor positions returned by GetCursorPosition(). Return 0 if no feedback
  // should be shown.
  virtual float GetFeedbackRadius(TouchFeedbackType type) const = 0;

  // Returns true if the input strategy maintains a visible cursor on the
  // desktop.
  virtual bool IsCursorVisible() const = 0;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_INPUT_TOUCH_INPUT_STRATEGY_H_
