// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/trackpad_input_strategy.h"

#include "remoting/client/ui/desktop_viewport.h"

namespace remoting {

namespace {

// Trackpad mode does not need tap feedback.
const float kTapFeedbackRadius = 0.f;

const float kDragFeedbackRadius = 8.f;

}  // namespace

TrackpadInputStrategy::TrackpadInputStrategy(const DesktopViewport& viewport)
    : cursor_position_(viewport.GetViewportCenter()) {}

TrackpadInputStrategy::~TrackpadInputStrategy() = default;

void TrackpadInputStrategy::HandleZoom(const ViewMatrix::Point& pivot,
                                       float scale,
                                       DesktopViewport* viewport) {
  // The cursor position is the pivot point.
  ViewMatrix::Point cursor_pivot =
      viewport->GetTransformation().MapPoint(cursor_position_);
  viewport->ScaleDesktop(cursor_pivot.x, cursor_pivot.y, scale);

  // Keep the cursor on focus.
  viewport->SetViewportCenter(cursor_position_.x, cursor_position_.y);
}

bool TrackpadInputStrategy::HandlePan(const ViewMatrix::Vector2D& translation,
                                      Gesture simultaneous_gesture,
                                      DesktopViewport* viewport) {
  if (simultaneous_gesture == ZOOM) {
    // Don't move the cursor if the user is scaling the viewport.
    return false;
  }
  ViewMatrix::Vector2D translation_on_desktop =
      viewport->GetTransformation().Invert().MapVector(translation);

  cursor_position_.x += translation_on_desktop.x;
  cursor_position_.y += translation_on_desktop.y;

  cursor_position_ = viewport->ConstrainPointToDesktop(cursor_position_);

  // Keep the cursor on focus.
  viewport->SetViewportCenter(cursor_position_.x, cursor_position_.y);
  return true;
}

bool TrackpadInputStrategy::TrackTouchInput(
    const ViewMatrix::Point& touch_point,
    const DesktopViewport& viewport) {
  // Do nothing. The cursor position is independent of the touch position.
  // |touch_point| is always valid.
  return true;
}

ViewMatrix::Point TrackpadInputStrategy::GetCursorPosition() const {
  return cursor_position_;
}

void TrackpadInputStrategy::FocusViewportOnCursor(
    DesktopViewport* viewport) const {
  viewport->SetViewportCenter(cursor_position_.x, cursor_position_.y);
}

ViewMatrix::Vector2D TrackpadInputStrategy::MapScreenVectorToDesktop(
    const ViewMatrix::Vector2D& delta,
    const DesktopViewport& viewport) const {
  // No conversion is needed for trackpad mode.
  return delta;
}

float TrackpadInputStrategy::GetFeedbackRadius(TouchFeedbackType type) const {
  switch (type) {
    case TouchFeedbackType::TAP_FEEDBACK:
      return kTapFeedbackRadius;
    case TouchFeedbackType::DRAG_FEEDBACK:
      return kDragFeedbackRadius;
  }
  NOTREACHED();
}

bool TrackpadInputStrategy::IsCursorVisible() const {
  return true;
}

}  // namespace remoting
