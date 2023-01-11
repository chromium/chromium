// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/gesture_interpreter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "remoting/client/chromoting_session.h"
#include "remoting/client/display/renderer_proxy.h"
#include "remoting/client/input/direct_touch_input_strategy.h"
#include "remoting/client/input/trackpad_input_strategy.h"

namespace {

const float kOneFingerFlingTimeConstant = 180.f;
const float kScrollFlingTimeConstant = 250.f;

}  // namespace

namespace remoting {
GestureInterpreter::GestureInterpreter()
    // TODO(yuweih): These animations are better to take GetWeakPtr().
    : pan_animation_(
          kOneFingerFlingTimeConstant,
          base::BindRepeating(&GestureInterpreter::PanWithoutAbortAnimations,
                              base::Unretained(this))),
      scroll_animation_(
          kScrollFlingTimeConstant,
          base::BindRepeating(&GestureInterpreter::ScrollWithoutAbortAnimations,
                              base::Unretained(this))) {}

GestureInterpreter::~GestureInterpreter() = default;

void GestureInterpreter::SetContext(RendererProxy* renderer,
                                    ChromotingSession* input_stub) {
  renderer_ = renderer;
  input_stub_ = input_stub;
  auto transformation_callback =
      renderer_ ? base::BindRepeating(&RendererProxy::SetTransformation,
                                      base::Unretained(renderer_))
                : DesktopViewport::TransformationCallback();
  viewport_.RegisterOnTransformationChangedCallback(transformation_callback,
                                                    true);
}

void GestureInterpreter::SetInputMode(InputMode mode) {
  switch (mode) {
    case DIRECT_INPUT_MODE:
      input_strategy_ = std::make_unique<DirectTouchInputStrategy>();
      break;
    case TRACKPAD_INPUT_MODE:
      input_strategy_ = std::make_unique<TrackpadInputStrategy>(viewport_);
      break;
    default:
      NOTREACHED();
  }
  input_mode_ = mode;
  if (!renderer_) {
    return;
  }
  renderer_->SetCursorVisibility(input_strategy_->IsCursorVisible());
  ViewMatrix::Point cursor_position = input_strategy_->GetCursorPosition();
  renderer_->SetCursorPosition(cursor_position.x, cursor_position.y);
}

GestureInterpreter::InputMode GestureInterpreter::GetInputMode() const {
  return input_mode_;
}

void GestureInterpreter::Zoom(float pivot_x,
                              float pivot_y,
                              float scale,
                              GestureState state) {
  AbortAnimations();
  SetGestureInProgress(TouchInputStrategy::ZOOM, state != GESTURE_ENDED);
  if (viewport_.IsViewportReady()) {
    input_strategy_->HandleZoom({pivot_x, pivot_y}, scale, &viewport_);
  }
}

void GestureInterpreter::Pan(float translation_x, float translation_y) {
  AbortAnimations();
  PanWithoutAbortAnimations(translation_x, translation_y);
}

void GestureInterpreter::Tap(float x, float y) {
  AbortAnimations();

  InjectMouseClick(x, y, protocol::MouseEvent_MouseButton_BUTTON_LEFT);
}

void GestureInterpreter::TwoFingerTap(float x, float y) {
  AbortAnimations();

  InjectMouseClick(x, y, protocol::MouseEvent_MouseButton_BUTTON_RIGHT);
}

void GestureInterpreter::ThreeFingerTap(float x, float y) {
  AbortAnimations();

  InjectMouseClick(x, y, protocol::MouseEvent_MouseButton_BUTTON_MIDDLE);
}

void GestureInterpreter::Drag(float x, float y, GestureState state) {
  AbortAnimations();

  bool is_dragging_mode = state != GESTURE_ENDED;
  SetGestureInProgress(TouchInputStrategy::DRAG, is_dragging_mode);

  if (!input_stub_ || !viewport_.IsViewportReady() ||
      !input_strategy_->TrackTouchInput({x, y}, viewport_)) {
    return;
  }
  ViewMatrix::Point cursor_position = input_strategy_->GetCursorPosition();

  switch (state) {
    case GESTURE_BEGAN:
      StartInputFeedback(cursor_position.x, cursor_position.y,
                         TouchInputStrategy::DRAG_FEEDBACK);
      input_stub_->SendMouseEvent(cursor_position.x, cursor_position.y,
                                  protocol::MouseEvent_MouseButton_BUTTON_LEFT,
                                  true);
      break;
    case GESTURE_CHANGED:
      InjectCursorPosition(cursor_position.x, cursor_position.y);
      break;
    case GESTURE_ENDED:
      input_stub_->SendMouseEvent(cursor_position.x, cursor_position.y,
                                  protocol::MouseEvent_MouseButton_BUTTON_LEFT,
                                  false);
      break;
    default:
      NOTREACHED();
  }
}

void GestureInterpreter::OneFingerFling(float velocity_x, float velocity_y) {
  AbortAnimations();
  pan_animation_.SetVelocity(velocity_x, velocity_y);
  pan_animation_.Tick();
}

void GestureInterpreter::Scroll(float x, float y, float dx, float dy) {
  AbortAnimations();

  if (!viewport_.IsViewportReady() ||
      !input_strategy_->TrackTouchInput({x, y}, viewport_)) {
    return;
  }
  ViewMatrix::Point cursor_position = input_strategy_->GetCursorPosition();

  // Inject the cursor position to the host so that scrolling can happen on the
  // right place.
  InjectCursorPosition(cursor_position.x, cursor_position.y);

  ScrollWithoutAbortAnimations(dx, dy);
}

void GestureInterpreter::ScrollWithVelocity(float velocity_x,
                                            float velocity_y) {
  AbortAnimations();

  scroll_animation_.SetVelocity(velocity_x, velocity_y);
  scroll_animation_.Tick();
}

void GestureInterpreter::ProcessAnimations() {
  pan_animation_.Tick();

  // TODO(yuweih): It's probably not right to handle host side virtual scroll
  // momentum in the renderer's callback.
  scroll_animation_.Tick();
}

void GestureInterpreter::OnSurfaceSizeChanged(int width, int height) {
  viewport_.SetSurfaceSize(width, height);
  if (viewport_.IsViewportReady()) {
    input_strategy_->FocusViewportOnCursor(&viewport_);
  }
}

void GestureInterpreter::OnDesktopSizeChanged(int width, int height) {
  viewport_.SetDesktopSize(width, height);
  if (viewport_.IsViewportReady()) {
    input_strategy_->FocusViewportOnCursor(&viewport_);
  }
}

void GestureInterpreter::OnSafeInsetsChanged(int left,
                                             int top,
                                             int right,
                                             int bottom) {
  viewport_.SetSafeInsets(left, top, right, bottom);
  if (viewport_.IsViewportReady()) {
    input_strategy_->FocusViewportOnCursor(&viewport_);
  }
}

base::WeakPtr<GestureInterpreter> GestureInterpreter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void GestureInterpreter::PanWithoutAbortAnimations(float translation_x,
                                                   float translation_y) {
  if (viewport_.IsViewportReady() &&
      input_strategy_->HandlePan({translation_x, translation_y},
                                 gesture_in_progress_, &viewport_)) {
    // Cursor position changed.
    ViewMatrix::Point cursor_position = input_strategy_->GetCursorPosition();
    if (gesture_in_progress_ != TouchInputStrategy::DRAG) {
      // Drag() will inject the position so don't need to do that in that case.
      InjectCursorPosition(cursor_position.x, cursor_position.y);
    }
    if (renderer_) {
      renderer_->SetCursorPosition(cursor_position.x, cursor_position.y);
    }
  }
}

void GestureInterpreter::InjectCursorPosition(float x, float y) {
  if (!input_stub_) {
    return;
  }
  input_stub_->SendMouseEvent(
      x, y, protocol::MouseEvent_MouseButton_BUTTON_UNDEFINED, false);
}

void GestureInterpreter::ScrollWithoutAbortAnimations(float dx, float dy) {
  if (!input_stub_ || !viewport_.IsViewportReady()) {
    return;
  }
  ViewMatrix::Point desktopDelta =
      input_strategy_->MapScreenVectorToDesktop({dx, dy}, viewport_);
  input_stub_->SendMouseWheelEvent(desktopDelta.x, desktopDelta.y);
}

void GestureInterpreter::AbortAnimations() {
  pan_animation_.Abort();
  scroll_animation_.Abort();
}

void GestureInterpreter::InjectMouseClick(
    float touch_x,
    float touch_y,
    protocol::MouseEvent_MouseButton button) {
  if (!input_stub_ || !viewport_.IsViewportReady() ||
      !input_strategy_->TrackTouchInput({touch_x, touch_y}, viewport_)) {
    return;
  }
  ViewMatrix::Point cursor_position = input_strategy_->GetCursorPosition();
  StartInputFeedback(cursor_position.x, cursor_position.y,
                     TouchInputStrategy::TAP_FEEDBACK);

  input_stub_->SendMouseEvent(cursor_position.x, cursor_position.y, button,
                              true);
  input_stub_->SendMouseEvent(cursor_position.x, cursor_position.y, button,
                              false);
}

void GestureInterpreter::SetGestureInProgress(
    TouchInputStrategy::Gesture gesture,
    bool is_in_progress) {
  if (!is_in_progress && gesture_in_progress_ == gesture) {
    gesture_in_progress_ = TouchInputStrategy::NONE;
    return;
  }
  gesture_in_progress_ = gesture;
}

void GestureInterpreter::StartInputFeedback(
    float cursor_x,
    float cursor_y,
    TouchInputStrategy::TouchFeedbackType feedback_type) {
  // This radius is on the view's coordinates. Need to be converted to desktop
  // coordinate.
  float feedback_radius = input_strategy_->GetFeedbackRadius(feedback_type);
  if (feedback_radius > 0) {
    // TODO(yuweih): The renderer takes diameter as parameter. Consider moving
    // the *2 logic inside the renderer.
    float diameter_on_desktop =
        2.f * feedback_radius / viewport_.GetTransformation().GetScale();
    if (renderer_) {
      renderer_->StartInputFeedback(cursor_x, cursor_y, diameter_on_desktop);
    }
  }
}

}  // namespace remoting
