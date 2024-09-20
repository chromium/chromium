// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/direct_touch_input_strategy.h"

#include "remoting/client/ui/desktop_viewport.h"

namespace remoting {

namespace {

const float kTapFeedbackRadius = 25.f;
const float kDragFeedbackRadius = 55.f;

}  // namespace

DirectTouchInputStrategy::DirectTouchInputStrategy() = default;

DirectTouchInputStrategy::~DirectTouchInputStrategy() = default;

void DirectTouchInputStrategy::HandleZoom(const ViewMatrix::Point& pivot,
                                          float scale,
                                          DesktopViewport* viewport) {
  viewport->ScaleDesktop(pivot.x, pivot.y, scale);
}

bool DirectTouchInputStrategy::HandlePan(
    const ViewMatrix::Vector2D& translation,
    Gesture simultaneous_gesture,
    DesktopViewport* viewport) {
  if (simultaneous_gesture == DRAG) {
    // If the user is dragging something, we should synchronize the movement
    // with the object that the user is trying to move on the desktop, rather
    // than moving the desktop around.
    ViewMatrix::Vector2D viewport_movement =
        viewport->GetTransformation().Invert().MapVector(translation);
    viewport->MoveViewport(viewport_movement.x, viewport_movement.y);
    return false;
  }

  viewport->MoveDesktop(translation.x, translation.y);
  return false;
}

bool DirectTouchInputStrategy::TrackTouchInput(
    const ViewMatrix::Point& touch_point,
    const DesktopViewport& viewport) {
  ViewMatrix::Point new_position =
      viewport.GetTransformation().Invert().MapPoint(touch_point);
  if (!viewport.IsPointWithinDesktopBounds(new_position)) {
    return false;
  }
  cursor_position_ = new_position;
  return true;
}

ViewMatrix::Point DirectTouchInputStrategy::GetCursorPosition() const {
  return cursor_position_;
}

void DirectTouchInputStrategy::FocusViewportOnCursor(
    DesktopViewport* viewport) const {
  // No need to focus on the previous touch point.
}

ViewMatrix::Vector2D DirectTouchInputStrategy::MapScreenVectorToDesktop(
    const ViewMatrix::Vector2D& delta,
    const DesktopViewport& viewport) const {
  return viewport.GetTransformation().Invert().MapVector(delta);
}

float DirectTouchInputStrategy::GetFeedbackRadius(
    TouchFeedbackType type) const {
  switch (type) {
    case TouchFeedbackType::TAP_FEEDBACK:
      return kTapFeedbackRadius;
    case TouchFeedbackType::DRAG_FEEDBACK:
      return kDragFeedbackRadius;
  }
  NOTREACHED();
}

bool DirectTouchInputStrategy::IsCursorVisible() const {
  return false;
}

}  // namespace remoting
