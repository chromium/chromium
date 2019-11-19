// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/direct_manipulation_event_handler_win.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/direct_manipulation_helper_win.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/win/window_event_target.h"

namespace content {

namespace {

bool FloatEquals(float f1, float f2) {
  // The idea behind this is to use this fraction of the larger of the
  // two numbers as the limit of the difference.  This breaks down near
  // zero, so we reuse this as the minimum absolute size we will use
  // for the base of the scale too.
  static const float epsilon_scale = 0.00001f;
  return fabs(f1 - f2) <
         epsilon_scale *
             std::fmax(std::fmax(std::fabs(f1), std::fabs(f2)), epsilon_scale);
}

}  // namespace

DirectManipulationEventHandler::DirectManipulationEventHandler(
    ui::WindowEventTarget* event_target)
    : event_target_(event_target) {}

bool DirectManipulationEventHandler::SetViewportSizeInPixels(
    const gfx::Size& viewport_size_in_pixels) {
  if (viewport_size_in_pixels_ == viewport_size_in_pixels)
    return false;
  viewport_size_in_pixels_ = viewport_size_in_pixels;
  return true;
}

void DirectManipulationEventHandler::SetDeviceScaleFactor(
    float device_scale_factor) {
  device_scale_factor_ = device_scale_factor;
}

void DirectManipulationEventHandler::SetDirectManipulationHelper(
    DirectManipulationHelper* helper) {
  helper_ = helper;
}

DirectManipulationEventHandler::~DirectManipulationEventHandler() {}

void DirectManipulationEventHandler::TransitionToState(
    GestureState new_gesture_state) {
  if (gesture_state_ == new_gesture_state)
    return;

  if (LoggingEnabled()) {
    std::string s = "TransitionToState " +
                    base::NumberToString(static_cast<int>(gesture_state_)) +
                    " -> " +
                    base::NumberToString(static_cast<int>(new_gesture_state));
    DebugLogging(s, S_OK);
  }

  GestureState previous_gesture_state = gesture_state_;
  gesture_state_ = new_gesture_state;

  // End the previous sequence.
  switch (previous_gesture_state) {
    case GestureState::kScroll: {
      // kScroll -> kNone, kPinch, ScrollEnd.
      // kScroll -> kFling, we don't want to end the current scroll sequence.
      if (new_gesture_state != GestureState::kFling)
        event_target_->ApplyPanGestureScrollEnd(new_gesture_state ==
                                                GestureState::kPinch);
      break;
    }
    case GestureState::kFling: {
      // kFling -> *, FlingEnd.
      event_target_->ApplyPanGestureFlingEnd();
      break;
    }
    case GestureState::kPinch: {
      DCHECK_EQ(new_gesture_state, GestureState::kNone);
      // kPinch -> kNone, PinchEnd. kPinch should only transition to kNone.
      event_target_->ApplyPinchZoomEnd();
      break;
    }
    case GestureState::kNone: {
      // kNone -> *, no cleanup is needed.
      break;
    }
    default:
      NOTREACHED();
  }

  // Start the new sequence.
  switch (new_gesture_state) {
    case GestureState::kScroll: {
      // kFling, kNone -> kScroll, ScrollBegin.
      // ScrollBegin is different phase event with others. It must send within
      // the first scroll event.
      should_send_scroll_begin_ = true;
      break;
    }
    case GestureState::kFling: {
      // Only kScroll can transition to kFling.
      DCHECK_EQ(previous_gesture_state, GestureState::kScroll);
      event_target_->ApplyPanGestureFlingBegin();
      break;
    }
    case GestureState::kPinch: {
      // * -> kPinch, PinchBegin.
      // Pinch gesture may begin with some scroll events.
      event_target_->ApplyPinchZoomBegin();
      break;
    }
    case GestureState::kNone: {
      // * -> kNone, only cleanup is needed.
      break;
    }
    default:
      NOTREACHED();
  }
}

HRESULT DirectManipulationEventHandler::OnViewportStatusChanged(
    IDirectManipulationViewport* viewport,
    DIRECTMANIPULATION_STATUS current,
    DIRECTMANIPULATION_STATUS previous) {
  // MSDN never mention |viewport| are nullable and we never saw it is null when
  // testing.
  DCHECK(viewport);

  if (LoggingEnabled()) {
    std::string s = "ViewportStatusChanged " + base::NumberToString(previous) +
                    " -> " + base::NumberToString(current);
    DebugLogging(s, S_OK);
  }

  // The state of our viewport has changed! We'l be in one of three states:
  // - ENABLED: initial state
  // - READY: the previous gesture has been completed
  // - RUNNING: gesture updating
  // - INERTIA: finger leave touchpad content still updating by inertia

  // Windows should not call this when event_target_ is null since we do not
  // pass the DM_POINTERHITTEST to DirectManipulation.
  if (!event_target_)
    return S_OK;

  if (current == previous)
    return S_OK;

  if (current == DIRECTMANIPULATION_INERTIA) {
    // Fling must lead by Scroll. We can actually hit here when user pinch then
    // quickly pan gesture and leave touchpad. In this case, we don't want to
    // start a new sequence until the gesture end. The rest events in sequence
    // will be ignore since sequence still in pinch and only scale factor
    // changes will be applied.
    if (previous != DIRECTMANIPULATION_RUNNING ||
        gesture_state_ != GestureState::kScroll) {
      return S_OK;
    }

    TransitionToState(GestureState::kFling);
  }

  if (current == DIRECTMANIPULATION_RUNNING) {
    // INERTIA -> RUNNING, should start a new sequence.
    if (previous == DIRECTMANIPULATION_INERTIA)
      TransitionToState(GestureState::kNone);
  }

  if (current != DIRECTMANIPULATION_READY)
    return S_OK;

  // Normally gesture sequence will receive 2 READY message, the first one is
  // gesture end, the second one is from viewport reset. We don't have content
  // transform in the second RUNNING -> READY. We should not reset on an empty
  // RUNNING -> READY sequence.
  if (last_scale_ != 1.0f || last_x_offset_ != 0 || last_y_offset_ != 0) {
    HRESULT hr = viewport->ZoomToRect(
        static_cast<float>(0), static_cast<float>(0),
        static_cast<float>(viewport_size_in_pixels_.width()),
        static_cast<float>(viewport_size_in_pixels_.height()), FALSE);
    if (!SUCCEEDED(hr)) {
      DebugLogging("Viewport zoom to rect failed.", hr);
      return hr;
    }
  }

  last_scale_ = 1.0f;
  last_x_offset_ = 0.0f;
  last_y_offset_ = 0.0f;

  TransitionToState(GestureState::kNone);

  return S_OK;
}

HRESULT DirectManipulationEventHandler::OnViewportUpdated(
    IDirectManipulationViewport* viewport) {
  if (LoggingEnabled())
    DebugLogging("OnViewportUpdated", S_OK);
  // Nothing to do here.
  return S_OK;
}

HRESULT DirectManipulationEventHandler::OnContentUpdated(
    IDirectManipulationViewport* viewport,
    IDirectManipulationContent* content) {
  // MSDN never mention these params are nullable and we never saw they are null
  // when testing.
  DCHECK(viewport);
  DCHECK(content);

  if (LoggingEnabled())
    DebugLogging("OnContentUpdated", S_OK);

  // Windows should not call this when event_target_ is null since we do not
  // pass the DM_POINTERHITTEST to DirectManipulation.
  if (!event_target_) {
    DebugLogging("OnContentUpdated event_target_ is null.", S_OK);
    return S_OK;
  }

  float xform[6];
  HRESULT hr = content->GetContentTransform(xform, ARRAYSIZE(xform));
  if (!SUCCEEDED(hr)) {
    DebugLogging("DirectManipulationContent get transform failed.", hr);
    return hr;
  }

  float scale = xform[0];
  int x_offset = xform[4] / device_scale_factor_;
  int y_offset = xform[5] / device_scale_factor_;

  // Ignore if Windows pass scale=0 to us.
  if (scale == 0.0f) {
    LOG(ERROR) << "Windows DirectManipulation API pass scale = 0.";
    return hr;
  }

  // Ignore the scale factor change less than float point rounding error and
  // scroll offset change less than 1.
  // TODO(456622) Because we don't fully support fractional scroll, pass float
  // scroll offset feels steppy. eg.
  // first x_offset is 0.1 ignored, but last_x_offset_ set to 0.1
  // second x_offset is 1 but x_offset - last_x_offset_ is 0.9 ignored.
  if (FloatEquals(scale, last_scale_) && x_offset == last_x_offset_ &&
      y_offset == last_y_offset_) {
    if (LoggingEnabled()) {
      std::string s =
          "OnContentUpdated ignored. scale=" + base::NumberToString(scale) +
          ", last_scale=" + base::NumberToString(last_scale_) +
          ", x_offset=" + base::NumberToString(x_offset) +
          ", last_x_offset=" + base::NumberToString(last_x_offset_) +
          ", y_offset=" + base::NumberToString(y_offset) +
          ", last_y_offset=" + base::NumberToString(last_y_offset_);
      DebugLogging(s, S_OK);
    }
    return hr;
  }

  DCHECK_NE(last_scale_, 0.0f);

  // DirectManipulation will send xy transform move to down-right which is noise
  // when pinch zoom. We should consider the gesture either Scroll or Pinch at
  // one sequence. But Pinch gesture may begin with some scroll transform since
  // DirectManipulation recognition maybe wrong at start if the user pinch with
  // slow motion. So we allow kScroll -> kPinch.

  // Consider this is a Scroll when scale factor equals 1.0.
  if (FloatEquals(scale, 1.0f)) {
    if (gesture_state_ == GestureState::kNone)
      TransitionToState(GestureState::kScroll);
  } else {
    // Pinch gesture may begin with some scroll events.
    TransitionToState(GestureState::kPinch);
  }

  if (gesture_state_ == GestureState::kScroll) {
    if (should_send_scroll_begin_) {
      event_target_->ApplyPanGestureScrollBegin(x_offset - last_x_offset_,
                                                y_offset - last_y_offset_);
      should_send_scroll_begin_ = false;
    } else {
      event_target_->ApplyPanGestureScroll(x_offset - last_x_offset_,
                                           y_offset - last_y_offset_);
    }
  } else if (gesture_state_ == GestureState::kFling) {
    event_target_->ApplyPanGestureFling(x_offset - last_x_offset_,
                                        y_offset - last_y_offset_);
  } else {
    event_target_->ApplyPinchZoomScale(scale / last_scale_);
  }

  last_scale_ = scale;
  last_x_offset_ = x_offset;
  last_y_offset_ = y_offset;

  return hr;
}

HRESULT DirectManipulationEventHandler::OnInteraction(
    IDirectManipulationViewport2* viewport,
    DIRECTMANIPULATION_INTERACTION_TYPE interaction) {
  if (!helper_)
    return S_OK;

  if (interaction == DIRECTMANIPULATION_INTERACTION_BEGIN) {
    DebugLogging("OnInteraction BEGIN.", S_OK);
    helper_->AddAnimationObserver();
  } else if (interaction == DIRECTMANIPULATION_INTERACTION_END) {
    DebugLogging("OnInteraction END.", S_OK);
    helper_->RemoveAnimationObserver();
  }

  return S_OK;
}

}  // namespace content
