// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_text_selector.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"

using ui::GestureDetector;
using ui::MotionEvent;

namespace content {
namespace {
std::unique_ptr<GestureDetector> CreateGestureDetector(
    ui::GestureListener* listener) {
  GestureDetector::Config config =
      ui::GetGestureProviderConfig(
          ui::GestureProviderConfigType::CURRENT_PLATFORM)
          .gesture_detector_config;

  ui::DoubleTapListener* null_double_tap_listener = nullptr;

  // Doubletap, showpress and longpress detection are not required, and
  // should be explicitly disabled for efficiency.
  std::unique_ptr<ui::GestureDetector> detector(
      new ui::GestureDetector(config, listener, null_double_tap_listener));
  detector->set_longpress_enabled(false);
  detector->set_showpress_enabled(false);

  return detector;
}

}  // namespace

StylusTextSelector::StylusTextSelector(StylusTextSelectorClient* client)
    : client_(client),
      text_selection_triggered_(false),
      secondary_button_pressed_(false),
      drag_state_(NO_DRAG),
      anchor_x_(0.0f),
      anchor_y_(0.0f) {
  DCHECK(client);
}

StylusTextSelector::~StylusTextSelector() {
}

bool StylusTextSelector::OnTouchEvent(const MotionEvent& event) {
  // Only trigger selection on ACTION_DOWN to prevent partial touch or gesture
  // sequences from being forwarded.
  if (event.GetAction() == MotionEvent::Action::DOWN)
    text_selection_triggered_ = ShouldStartTextSelection(event);

  if (!text_selection_triggered_)
    return false;

  // For Android version < M, stylus button pressed state is BUTTON_SECONDARY.
  // From Android M, this state has changed to BUTTON_STYLUS_PRIMARY.
  secondary_button_pressed_ =
      event.GetButtonState() == MotionEvent::BUTTON_SECONDARY ||
      event.GetButtonState() == MotionEvent::BUTTON_STYLUS_PRIMARY;

  switch (event.GetAction()) {
    case MotionEvent::Action::DOWN:
      drag_state_ = NO_DRAG;
      anchor_x_ = event.GetX();
      anchor_y_ = event.GetY();
      break;

    case MotionEvent::Action::MOVE:
      if (!secondary_button_pressed_) {
        if (drag_state_ == DRAGGING_WITH_BUTTON_PRESSED)
          drag_state_ = DRAGGING_WITH_BUTTON_RELEASED;
        anchor_x_ = event.GetX();
        anchor_y_ = event.GetY();
      }
      break;

    case MotionEvent::Action::UP:
    case MotionEvent::Action::CANCEL:
      if (drag_state_ == DRAGGING_WITH_BUTTON_PRESSED ||
          drag_state_ == DRAGGING_WITH_BUTTON_RELEASED)
        client_->OnStylusSelectEnd(event.GetX(), event.GetY());
      drag_state_ = NO_DRAG;
      break;

    case MotionEvent::Action::POINTER_UP:
    case MotionEvent::Action::POINTER_DOWN:
      break;
    case MotionEvent::Action::NONE:
    case MotionEvent::Action::HOVER_ENTER:
    case MotionEvent::Action::HOVER_EXIT:
    case MotionEvent::Action::HOVER_MOVE:
    case MotionEvent::Action::BUTTON_PRESS:
    case MotionEvent::Action::BUTTON_RELEASE:
      NOTREACHED();
      break;
  }

  if (!gesture_detector_)
    gesture_detector_ = CreateGestureDetector(this);

  gesture_detector_->OnTouchEvent(event, false /* should_process_double_tap */);

  // Always return true, even if |gesture_detector_| technically doesn't
  // consume the event. This prevents forwarding of a partial touch stream.
  return true;
}

bool StylusTextSelector::OnSingleTapUp(const MotionEvent& e, int tap_count) {
  DCHECK(text_selection_triggered_);
  DCHECK_NE(DRAGGING_WITH_BUTTON_PRESSED, drag_state_);
  client_->OnStylusSelectTap(e.GetEventTime(), e.GetX(), e.GetY());
  return true;
}

bool StylusTextSelector::OnScroll(const MotionEvent& e1,
                                  const MotionEvent& e2,
                                  const MotionEvent& secondary_pointer_down,
                                  float distance_x,
                                  float distance_y) {
  DCHECK(text_selection_triggered_);

  // Return if Stylus button is not pressed.
  if (!secondary_button_pressed_)
    return true;

  if (drag_state_ == NO_DRAG || drag_state_ == DRAGGING_WITH_BUTTON_RELEASED) {
    drag_state_ = DRAGGING_WITH_BUTTON_PRESSED;
    client_->OnStylusSelectBegin(anchor_x_, anchor_y_, e2.GetX(), e2.GetY());
  } else {
    client_->OnStylusSelectUpdate(e2.GetX(), e2.GetY());
  }

  return true;
}

// static
bool StylusTextSelector::ShouldStartTextSelection(const MotionEvent& event) {
  DCHECK_GT(event.GetPointerCount(), 0u);
  // Currently we are supporting stylus-only cases.
  const bool is_stylus = event.GetToolType(0) == MotionEvent::ToolType::STYLUS;

  // For Android version < M, stylus button pressed state is BUTTON_SECONDARY.
  // From Android M, this state has changed to BUTTON_STYLUS_PRIMARY.
  const bool is_only_secondary_button_pressed =
      event.GetButtonState() == MotionEvent::BUTTON_SECONDARY ||
      event.GetButtonState() == MotionEvent::BUTTON_STYLUS_PRIMARY;
  return is_stylus && is_only_secondary_button_pressed;
}

}  // namespace content
