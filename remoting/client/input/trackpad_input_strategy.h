// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_TRACKPAD_INPUT_STRATEGY_H_
#define REMOTING_CLIENT_INPUT_TRACKPAD_INPUT_STRATEGY_H_

#include "remoting/client/input/touch_input_strategy.h"

namespace remoting {

// This strategy simulate the trackpad's behavior. It keeps a visible cursor
// with positions independent of the location of the touch events.
class TrackpadInputStrategy : public TouchInputStrategy {
 public:
  TrackpadInputStrategy(const DesktopViewport& viewport);
  ~TrackpadInputStrategy() override;

  // TouchInputStrategy overrides.
  void HandleZoom(const ViewMatrix::Point& pivot,
                  float scale,
                  DesktopViewport* viewport) override;

  bool HandlePan(const ViewMatrix::Vector2D& translation,
                 Gesture simultaneous_gesture,
                 DesktopViewport* viewport) override;

  bool TrackTouchInput(const ViewMatrix::Point& touch_point,
                       const DesktopViewport& viewport) override;

  ViewMatrix::Point GetCursorPosition() const override;

  void FocusViewportOnCursor(DesktopViewport* viewport) const override;

  ViewMatrix::Vector2D MapScreenVectorToDesktop(
      const ViewMatrix::Vector2D& delta,
      const DesktopViewport& viewport) const override;

  float GetFeedbackRadius(TouchFeedbackType type) const override;

  bool IsCursorVisible() const override;

 private:
  ViewMatrix::Point cursor_position_;

  // TrackpadInputStrategy is neither copyable nor movable.
  TrackpadInputStrategy(const TrackpadInputStrategy&) = delete;
  TrackpadInputStrategy& operator=(const TrackpadInputStrategy&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_INPUT_TRACKPAD_INPUT_STRATEGY_H_
