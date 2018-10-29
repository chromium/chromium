// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_action_filter.h"

#include <math.h>

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/platform/web_gesture_event.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace content {
namespace {

// Actions on an axis are disallowed if the perpendicular axis has a filter set
// and no filter is set for the queried axis.
bool IsYAxisActionDisallowed(cc::TouchAction action) {
  return (action & cc::kTouchActionPanX) && !(action & cc::kTouchActionPanY);
}

bool IsXAxisActionDisallowed(cc::TouchAction action) {
  return (action & cc::kTouchActionPanY) && !(action & cc::kTouchActionPanX);
}

// Report how often the gesture event is or is not dropped due to the current
// allowed touch action state not matching the gesture event.
void ReportGestureEventFiltered(bool event_filtered) {
  UMA_HISTOGRAM_BOOLEAN("TouchAction.GestureEventFiltered", event_filtered);
}

}  // namespace

TouchActionFilter::TouchActionFilter()
    : suppress_manipulation_events_(false),
      drop_current_tap_ending_event_(false),
      allow_current_double_tap_event_(true),
      force_enable_zoom_(false) {
  ResetTouchAction();
}

TouchActionFilter::~TouchActionFilter() {}

FilterGestureEventResult TouchActionFilter::FilterGestureEvent(
    WebGestureEvent* gesture_event) {
  if (gesture_event->SourceDevice() != blink::kWebGestureDeviceTouchscreen)
    return FilterGestureEventResult::kFilterGestureEventAllowed;

  // Filter for allowable touch actions first (eg. before the TouchEventQueue
  // can decide to send a touch cancel event).
  switch (gesture_event->GetType()) {
    case WebInputEvent::kGestureScrollBegin: {
      DCHECK(!suppress_manipulation_events_);
      // In VR or virtual keyboard (https://crbug.com/880701),
      // GestureScrollBegin could come without GestureTapDown.
      if (!gesture_sequence_in_progress_) {
        gesture_sequence_in_progress_ = true;
        if (!scrolling_touch_action_.has_value())
          SetTouchAction(cc::kTouchActionAuto);
      }
      gesture_scroll_in_progress_ = true;
      gesture_sequence_.append("B");
      if (!scrolling_touch_action_.has_value()) {
        static auto* crash_key = base::debug::AllocateCrashKeyString(
            "scrollbegin-gestures", base::debug::CrashKeySize::Size256);
        base::debug::SetCrashKeyString(crash_key, gesture_sequence_);
        gesture_sequence_.clear();
      }
      suppress_manipulation_events_ =
          ShouldSuppressManipulation(*gesture_event);
      return suppress_manipulation_events_
                 ? FilterGestureEventResult::kFilterGestureEventFiltered
                 : FilterGestureEventResult::kFilterGestureEventAllowed;
    }

    case WebInputEvent::kGestureScrollUpdate: {
      if (suppress_manipulation_events_)
        return FilterGestureEventResult::kFilterGestureEventFiltered;

      gesture_sequence_.append("U");
      // Scrolls restricted to a specific axis shouldn't permit movement
      // in the perpendicular axis.
      //
      // Note the direction suppression with pinch-zoom here, which matches
      // Edge: a "touch-action: pan-y pinch-zoom" region allows vertical
      // two-finger scrolling but a "touch-action: pan-x pinch-zoom" region
      // doesn't.
      // TODO(mustaq): Add it to spec?
      if (!scrolling_touch_action_.has_value()) {
        static auto* crash_key = base::debug::AllocateCrashKeyString(
            "scrollupdate-gestures", base::debug::CrashKeySize::Size256);
        base::debug::SetCrashKeyString(crash_key, gesture_sequence_);
        gesture_sequence_.clear();
      }
      if (IsYAxisActionDisallowed(scrolling_touch_action_.value())) {
        gesture_event->data.scroll_update.delta_y = 0;
        gesture_event->data.scroll_update.velocity_y = 0;
      } else if (IsXAxisActionDisallowed(scrolling_touch_action_.value())) {
        gesture_event->data.scroll_update.delta_x = 0;
        gesture_event->data.scroll_update.velocity_x = 0;
      }
      break;
    }

    case WebInputEvent::kGestureFlingStart:
      // Fling controller processes FlingStart event, and we should never get
      // it here.
      NOTREACHED();
      break;

    case WebInputEvent::kGestureScrollEnd:
      gesture_sequence_.clear();
      gesture_scroll_in_progress_ = false;
      gesture_sequence_in_progress_ = false;
      ReportGestureEventFiltered(suppress_manipulation_events_);
      return FilterManipulationEventAndResetState()
                 ? FilterGestureEventResult::kFilterGestureEventFiltered
                 : FilterGestureEventResult::kFilterGestureEventAllowed;

    case WebInputEvent::kGesturePinchBegin:
      if (!gesture_scroll_in_progress_) {
        suppress_manipulation_events_ =
            ShouldSuppressManipulation(*gesture_event);
      }
      FALLTHROUGH;
    case WebInputEvent::kGesturePinchUpdate:
      gesture_sequence_.append("P");
      ReportGestureEventFiltered(suppress_manipulation_events_);
      return suppress_manipulation_events_
                 ? FilterGestureEventResult::kFilterGestureEventFiltered
                 : FilterGestureEventResult::kFilterGestureEventAllowed;
    case WebInputEvent::kGesturePinchEnd:
      ReportGestureEventFiltered(suppress_manipulation_events_);
      // If we're in a double-tap and drag zoom, we won't be bracketed between
      // a GSB/GSE pair so end the sequence now. Otherwise this will happen
      // when we get the GSE.
      if (!gesture_scroll_in_progress_) {
        gesture_sequence_.clear();
        gesture_sequence_in_progress_ = false;
        return FilterManipulationEventAndResetState()
                   ? FilterGestureEventResult::kFilterGestureEventFiltered
                   : FilterGestureEventResult::kFilterGestureEventAllowed;
      }
      return suppress_manipulation_events_
                 ? FilterGestureEventResult::kFilterGestureEventFiltered
                 : FilterGestureEventResult::kFilterGestureEventAllowed;

    // The double tap gesture is a tap ending event. If a double-tap gesture is
    // filtered out, replace it with a tap event but preserve the tap-count to
    // allow firing dblclick event in Blink.
    //
    // TODO(mustaq): This replacement of a double-tap gesture with a tap seems
    // buggy, it produces an inconsistent gesture event stream: GestureTapCancel
    // followed by GestureTap.  See crbug.com/874474#c47 for a repro.  We don't
    // know of any bug resulting from it, but it's better to fix the broken
    // assumption here at least to avoid introducing new bugs in future.
    case WebInputEvent::kGestureDoubleTap:
      gesture_sequence_in_progress_ = false;
      gesture_sequence_.append("D");
      DCHECK_EQ(1, gesture_event->data.tap.tap_count);
      if (!allow_current_double_tap_event_) {
        gesture_event->SetType(WebInputEvent::kGestureTap);
        gesture_event->data.tap.tap_count = 2;
      }
      allow_current_double_tap_event_ = true;
      break;

    // If double tap is disabled, there's no reason for the tap delay.
    case WebInputEvent::kGestureTapUnconfirmed: {
      gesture_sequence_.append("C");
      DCHECK_EQ(1, gesture_event->data.tap.tap_count);
      if (!scrolling_touch_action_.has_value()) {
        static auto* crash_key = base::debug::AllocateCrashKeyString(
            "tapunconfirmed-gestures", base::debug::CrashKeySize::Size256);
        base::debug::SetCrashKeyString(crash_key, gesture_sequence_);
        gesture_sequence_.clear();
      }
      allow_current_double_tap_event_ = (scrolling_touch_action_.value() &
                                         cc::kTouchActionDoubleTapZoom) != 0;
      if (!allow_current_double_tap_event_) {
        gesture_event->SetType(WebInputEvent::kGestureTap);
        drop_current_tap_ending_event_ = true;
      }
      break;
    }

    case WebInputEvent::kGestureTap:
      gesture_sequence_in_progress_ = false;
      gesture_sequence_.append("A");
      if (drop_current_tap_ending_event_) {
        drop_current_tap_ending_event_ = false;
        return FilterGestureEventResult::kFilterGestureEventFiltered;
      }
      break;

    case WebInputEvent::kGestureTapCancel:
      gesture_sequence_.append("K");
      if (drop_current_tap_ending_event_) {
        drop_current_tap_ending_event_ = false;
        return FilterGestureEventResult::kFilterGestureEventFiltered;
      }
      break;

    case WebInputEvent::kGestureTapDown:
      gesture_sequence_in_progress_ = true;
      // If the gesture is hitting a region that has a non-blocking (such as a
      // passive) event listener.
      // In theory, the num_of_active_touches_ should be > 0 at this point. But
      // crash reports suggest otherwise.
      if (gesture_event->is_source_touch_event_set_non_blocking ||
          num_of_active_touches_ <= 0)
        SetTouchAction(cc::kTouchActionAuto);
      scrolling_touch_action_ = allowed_touch_action_;
      if (scrolling_touch_action_.has_value())
        gesture_sequence_.append("OY");
      else
        gesture_sequence_.append("ON");
      gesture_sequence_.append(
          base::NumberToString(gesture_event->unique_touch_event_id));
      DCHECK(!drop_current_tap_ending_event_);
      break;

    case WebInputEvent::kGestureLongTap:
    case WebInputEvent::kGestureTwoFingerTap:
      gesture_sequence_in_progress_ = false;
      break;

    default:
      // Gesture events unrelated to touch actions (panning/zooming) are left
      // alone.
      break;
  }

  return FilterGestureEventResult::kFilterGestureEventAllowed;
}

void TouchActionFilter::SetTouchAction(cc::TouchAction touch_action) {
  allowed_touch_action_ = touch_action;
  scrolling_touch_action_ = allowed_touch_action_;
}

bool TouchActionFilter::FilterManipulationEventAndResetState() {
  if (suppress_manipulation_events_) {
    suppress_manipulation_events_ = false;
    return true;
  }
  return false;
}

void TouchActionFilter::ForceResetTouchActionForTest() {
  allowed_touch_action_.reset();
  scrolling_touch_action_.reset();
}

void TouchActionFilter::OnSetTouchAction(cc::TouchAction touch_action) {
  // TODO(https://crbug.com/849819): add a DCHECK for
  // |has_touch_event_handler_|.
  // For multiple fingers, we take the intersection of the touch actions for
  // all fingers that have gone down during this action.  In the majority of
  // real-world scenarios the touch action for all fingers will be the same.
  // This is left as implementation-defined in the pointer events
  // specification because of the relationship to gestures (which are off
  // limits for the spec).  I believe the following are desirable properties
  // of this choice:
  // 1. Not sensitive to finger touch order.  Behavior of putting two fingers
  //    down "at once" will be deterministic.
  // 2. Only subtractive - eg. can't trigger scrolling on a element that
  //    otherwise has scrolling disabling by the addition of a finger.
  allowed_touch_action_ =
      allowed_touch_action_.value_or(cc::kTouchActionAuto) & touch_action;

  // When user enabled force enable zoom, we should always allow pinch-zoom
  // except for touch-action:none.
  if (force_enable_zoom_ && allowed_touch_action_ != cc::kTouchActionNone) {
    allowed_touch_action_ =
        allowed_touch_action_.value() | cc::kTouchActionPinchZoom;
  }
  scrolling_touch_action_ = allowed_touch_action_;
}

void TouchActionFilter::IncreaseActiveTouches() {
  num_of_active_touches_++;
}

void TouchActionFilter::DecreaseActiveTouches() {
  num_of_active_touches_--;
}

void TouchActionFilter::ReportAndResetTouchAction() {
  if (has_touch_event_handler_)
    gesture_sequence_.append("RY");
  else
    gesture_sequence_.append("RN");
  ReportTouchAction();
  if (num_of_active_touches_ <= 0)
    ResetTouchAction();
}

void TouchActionFilter::ReportTouchAction() {
  // Report the effective touch action computed by blink such as
  // kTouchActionNone, kTouchActionPanX, etc.
  // Since |cc::kTouchActionAuto| is equivalent to |cc::kTouchActionMax|, we
  // must add one to the upper bound to be able to visualize the number of
  // times |cc::kTouchActionAuto| is hit.
  // https://crbug.com/879511, remove this temporary fix.
  if (!scrolling_touch_action_.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("TouchAction.EffectiveTouchAction",
                            scrolling_touch_action_.value(),
                            cc::kTouchActionMax + 1);

  // Report how often the effective touch action computed by blink is or is
  // not equivalent to the whitelisted touch action computed by the
  // compositor.
  if (white_listed_touch_action_.has_value()) {
    UMA_HISTOGRAM_BOOLEAN(
        "TouchAction.EquivalentEffectiveAndWhiteListed",
        scrolling_touch_action_.value() == white_listed_touch_action_.value());
  }
}

void TouchActionFilter::AppendToGestureSequenceForDebugging(const char* str) {
  gesture_sequence_.append(str);
}

void TouchActionFilter::ResetTouchAction() {
  // Note that resetting the action mid-sequence is tolerated. Gestures that had
  // their begin event(s) suppressed will be suppressed until the next
  // sequenceo.
  if (has_touch_event_handler_) {
    allowed_touch_action_.reset();
    white_listed_touch_action_.reset();
  } else {
    // Lack of a touch handler indicates that the page either has no
    // touch-action modifiers or that all its touch-action modifiers are auto.
    // Resetting the touch-action here allows forwarding of subsequent gestures
    // even if the underlying touches never reach the router.
    SetTouchAction(cc::kTouchActionAuto);
    white_listed_touch_action_ = cc::kTouchActionAuto;
  }
}

void TouchActionFilter::OnSetWhiteListedTouchAction(
    cc::TouchAction white_listed_touch_action) {
  // We use '&' here to account for the multiple-finger case, which is the same
  // as OnSetTouchAction.
  if (white_listed_touch_action_.has_value()) {
    white_listed_touch_action_ =
        white_listed_touch_action_.value() & white_listed_touch_action;
  } else {
    white_listed_touch_action_ = white_listed_touch_action;
  }
}

bool TouchActionFilter::ShouldSuppressManipulation(
    const blink::WebGestureEvent& gesture_event) {
  DCHECK(gesture_event.GetType() == WebInputEvent::kGestureScrollBegin ||
         WebInputEvent::IsPinchGestureEventType(gesture_event.GetType()));

  if (WebInputEvent::IsPinchGestureEventType(gesture_event.GetType())) {
    return (scrolling_touch_action_.value() & cc::kTouchActionPinchZoom) == 0;
  }

  if (gesture_event.data.scroll_begin.pointer_count >= 2) {
    // Any GestureScrollBegin with more than one fingers is like a pinch-zoom
    // for touch-actions, see crbug.com/632525. Therefore, we switch to
    // blocked-manipulation mode iff pinch-zoom is disallowed.
    return (scrolling_touch_action_.value() & cc::kTouchActionPinchZoom) == 0;
  }

  const float& deltaXHint = gesture_event.data.scroll_begin.delta_x_hint;
  const float& deltaYHint = gesture_event.data.scroll_begin.delta_y_hint;

  if (deltaXHint == 0.0 && deltaYHint == 0.0)
    return false;

  const float absDeltaXHint = fabs(deltaXHint);
  const float absDeltaYHint = fabs(deltaYHint);

  cc::TouchAction minimal_conforming_touch_action = cc::kTouchActionNone;
  if (absDeltaXHint >= absDeltaYHint) {
    if (deltaXHint > 0)
      minimal_conforming_touch_action |= cc::kTouchActionPanLeft;
    else if (deltaXHint < 0)
      minimal_conforming_touch_action |= cc::kTouchActionPanRight;
  }
  if (absDeltaYHint >= absDeltaXHint) {
    if (deltaYHint > 0)
      minimal_conforming_touch_action |= cc::kTouchActionPanUp;
    else if (deltaYHint < 0)
      minimal_conforming_touch_action |= cc::kTouchActionPanDown;
  }
  DCHECK(minimal_conforming_touch_action != cc::kTouchActionNone);

  return (scrolling_touch_action_.value() & minimal_conforming_touch_action) ==
         0;
}

void TouchActionFilter::OnHasTouchEventHandlers(bool has_handlers) {
  // The has_touch_event_handler_ is default to false which is why we have the
  // "&&" condition here, to ensure that touch actions will be set if there is
  // no touch event handler on a page.
  if (has_handlers && has_touch_event_handler_ == has_handlers)
    return;
  has_touch_event_handler_ = has_handlers;
  if (has_touch_event_handler_)
    gesture_sequence_.append("LY");
  else
    gesture_sequence_.append("LN");
  // We have set the associated touch action if the touch start already happened
  // or there is a gesture in progress. In these cases, we should not reset the
  // associated touch action.
  if (!gesture_sequence_in_progress_ && num_of_active_touches_ <= 0) {
    ResetTouchAction();
    if (has_touch_event_handler_)
      scrolling_touch_action_.reset();
  }
}

}  // namespace content
