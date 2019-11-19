// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_action_filter.h"

#include <math.h>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "ui/events/blink/blink_features.h"

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GestureEventFilterResults {
  kGSBAllowedByMain = 0,
  kGSBAllowedByCC = 1,
  kGSBFilteredByMain = 2,
  kGSBFilteredByCC = 3,
  kGSBDeferred = 4,
  kGSUAllowedByMain = 5,
  kGSUAllowedByCC = 6,
  kGSUFilteredByMain = 7,
  kGSUFilteredByCC = 8,
  kGSUDeferred = 9,
  kFilterResultsCount = 10,
  kMaxValue = kFilterResultsCount
};

void ReportGestureEventFilterResults(bool is_gesture_scroll_begin,
                                     bool active_touch_action_known,
                                     FilterGestureEventResult result) {
  GestureEventFilterResults report_type;
  if (is_gesture_scroll_begin) {
    if (result == FilterGestureEventResult::kFilterGestureEventAllowed) {
      if (active_touch_action_known)
        report_type = GestureEventFilterResults::kGSBAllowedByMain;
      else
        report_type = GestureEventFilterResults::kGSBAllowedByCC;
    } else if (result ==
               FilterGestureEventResult::kFilterGestureEventFiltered) {
      if (active_touch_action_known)
        report_type = GestureEventFilterResults::kGSBFilteredByMain;
      else
        report_type = GestureEventFilterResults::kGSBFilteredByCC;
    } else {
      report_type = GestureEventFilterResults::kGSBDeferred;
    }
  } else {
    if (result == FilterGestureEventResult::kFilterGestureEventAllowed) {
      if (active_touch_action_known)
        report_type = GestureEventFilterResults::kGSUAllowedByMain;
      else
        report_type = GestureEventFilterResults::kGSUAllowedByCC;
    } else if (result ==
               FilterGestureEventResult::kFilterGestureEventFiltered) {
      if (active_touch_action_known)
        report_type = GestureEventFilterResults::kGSUFilteredByMain;
      else
        report_type = GestureEventFilterResults::kGSUFilteredByCC;
    } else {
      report_type = GestureEventFilterResults::kGSUDeferred;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("TouchAction.GestureEventFilterResults",
                            report_type, GestureEventFilterResults::kMaxValue);
}

}  // namespace

TouchActionFilter::TouchActionFilter()
    : compositor_touch_action_enabled_(
          base::FeatureList::IsEnabled(features::kCompositorTouchAction)) {
  ResetTouchAction();
}

TouchActionFilter::~TouchActionFilter() {}

FilterGestureEventResult TouchActionFilter::FilterGestureEvent(
    WebGestureEvent* gesture_event) {
  if (gesture_event->SourceDevice() != blink::WebGestureDevice::kTouchscreen)
    return FilterGestureEventResult::kFilterGestureEventAllowed;

  if (compositor_touch_action_enabled_ && has_deferred_events_) {
    WebInputEvent::Type type = gesture_event->GetType();
    if (type == WebInputEvent::kGestureScrollBegin ||
        type == WebInputEvent::kGestureScrollUpdate) {
      ReportGestureEventFilterResults(
          type == WebInputEvent::kGestureScrollBegin, false,
          FilterGestureEventResult::kFilterGestureEventDelayed);
    }
    return FilterGestureEventResult::kFilterGestureEventDelayed;
  }

  cc::TouchAction touch_action = active_touch_action_.has_value()
                                     ? active_touch_action_.value()
                                     : white_listed_touch_action_;

  // Filter for allowable touch actions first (eg. before the TouchEventQueue
  // can decide to send a touch cancel event).
  switch (gesture_event->GetType()) {
    case WebInputEvent::kGestureScrollBegin: {
      // In VR or virtual keyboard (https://crbug.com/880701),
      // GestureScrollBegin could come without GestureTapDown.
      if (!gesture_sequence_in_progress_) {
        gesture_sequence_in_progress_ = true;
        if (allowed_touch_action_.has_value()) {
          active_touch_action_ = allowed_touch_action_;
          touch_action = allowed_touch_action_.value();
        } else {
          if (compositor_touch_action_enabled_) {
            touch_action = white_listed_touch_action_;
          } else {
            gesture_sequence_.append("B");
            SetTouchAction(cc::kTouchActionAuto);
            touch_action = cc::kTouchActionAuto;
          }
        }
      }
      drop_scroll_events_ =
          ShouldSuppressScrolling(*gesture_event, touch_action);
      FilterGestureEventResult res;
      if (!drop_scroll_events_) {
        res = FilterGestureEventResult::kFilterGestureEventAllowed;
      } else if (active_touch_action_.has_value()) {
        res = FilterGestureEventResult::kFilterGestureEventFiltered;
      } else {
        has_deferred_events_ = true;
        res = FilterGestureEventResult::kFilterGestureEventDelayed;
      }
      ReportGestureEventFilterResults(true, active_touch_action_.has_value(),
                                      res);
      return res;
    }

    case WebInputEvent::kGestureScrollUpdate: {
      if (drop_scroll_events_) {
        ReportGestureEventFilterResults(
            false, active_touch_action_.has_value(),
            FilterGestureEventResult::kFilterGestureEventFiltered);
        return FilterGestureEventResult::kFilterGestureEventFiltered;
      }

      gesture_sequence_.append("U");
      // Scrolls restricted to a specific axis shouldn't permit movement
      // in the perpendicular axis.
      //
      // Note the direction suppression with pinch-zoom here, which matches
      // Edge: a "touch-action: pan-y pinch-zoom" region allows vertical
      // two-finger scrolling but a "touch-action: pan-x pinch-zoom" region
      // doesn't.
      // TODO(mustaq): Add it to spec?
      if (!compositor_touch_action_enabled_ &&
          !active_touch_action_.has_value()) {
        static auto* crash_key = base::debug::AllocateCrashKeyString(
            "scrollupdate-gestures", base::debug::CrashKeySize::Size256);
        base::debug::SetCrashKeyString(crash_key, gesture_sequence_);
        gesture_sequence_.clear();
      }
      if (IsYAxisActionDisallowed(touch_action)) {
        if (compositor_touch_action_enabled_ &&
            !active_touch_action_.has_value() &&
            gesture_event->data.scroll_update.delta_y != 0) {
          has_deferred_events_ = true;
          ReportGestureEventFilterResults(
              false, active_touch_action_.has_value(),
              FilterGestureEventResult::kFilterGestureEventDelayed);
          return FilterGestureEventResult::kFilterGestureEventDelayed;
        }
        gesture_event->data.scroll_update.delta_y = 0;
        gesture_event->data.scroll_update.velocity_y = 0;
      } else if (IsXAxisActionDisallowed(touch_action)) {
        if (compositor_touch_action_enabled_ &&
            !active_touch_action_.has_value() &&
            gesture_event->data.scroll_update.delta_x != 0) {
          has_deferred_events_ = true;
          ReportGestureEventFilterResults(
              false, active_touch_action_.has_value(),
              FilterGestureEventResult::kFilterGestureEventDelayed);
          return FilterGestureEventResult::kFilterGestureEventDelayed;
        }
        gesture_event->data.scroll_update.delta_x = 0;
        gesture_event->data.scroll_update.velocity_x = 0;
      }
      ReportGestureEventFilterResults(
          false, active_touch_action_.has_value(),
          FilterGestureEventResult::kFilterGestureEventAllowed);
      break;
    }

    case WebInputEvent::kGestureFlingStart:
      // Fling controller processes FlingStart event, and we should never get
      // it here.
      NOTREACHED();
      break;

    case WebInputEvent::kGestureScrollEnd:
      if (gesture_sequence_.size() >= 1000)
        gesture_sequence_.erase(gesture_sequence_.begin(),
                                gesture_sequence_.end() - 250);
      // Do not reset |white_listed_touch_action_|. In the fling cancel case,
      // the ack for the second touch sequence start, which sets the white
      // listed touch action, could arrive before the GSE of the first fling
      // sequence, we do not want to reset the white listed touch action.
      gesture_sequence_in_progress_ = false;
      ReportGestureEventFiltered(drop_scroll_events_);
      return FilterScrollEventAndResetState();

    // Evaluate the |drop_pinch_events_| here instead of GSB because pinch
    // events could arrive without GSB, e.g. double-tap-drag.
    case WebInputEvent::kGesturePinchBegin:
      drop_pinch_events_ = (touch_action & cc::kTouchActionPinchZoom) == 0;
      FALLTHROUGH;
    case WebInputEvent::kGesturePinchUpdate:
      gesture_sequence_.append("P");
      if (!drop_pinch_events_)
        return FilterGestureEventResult::kFilterGestureEventAllowed;
      if (compositor_touch_action_enabled_ &&
          !active_touch_action_.has_value()) {
        has_deferred_events_ = true;
        return FilterGestureEventResult::kFilterGestureEventDelayed;
      }
      return FilterGestureEventResult::kFilterGestureEventFiltered;
    case WebInputEvent::kGesturePinchEnd:
      ReportGestureEventFiltered(drop_pinch_events_);
      return FilterPinchEventAndResetState();

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
      DCHECK_EQ(1, gesture_event->data.tap.tap_count);
      gesture_sequence_.append("C");
      if (!compositor_touch_action_enabled_ &&
          !active_touch_action_.has_value()) {
        static auto* crash_key = base::debug::AllocateCrashKeyString(
            "tapunconfirmed-gestures", base::debug::CrashKeySize::Size256);
        base::debug::SetCrashKeyString(crash_key, gesture_sequence_);
        gesture_sequence_.clear();
      }
      allow_current_double_tap_event_ =
          (touch_action & cc::kTouchActionDoubleTapZoom) != 0;
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
      if (allowed_touch_action_.has_value())
        gesture_sequence_.append("AY");
      else
        gesture_sequence_.append("AN");
      if (active_touch_action_.has_value())
        gesture_sequence_.append("OY");
      else
        gesture_sequence_.append("ON");
      // In theory, the num_of_active_touches_ should be > 0 at this point. But
      // crash reports suggest otherwise.
      if (num_of_active_touches_ <= 0)
        SetTouchAction(cc::kTouchActionAuto);
      active_touch_action_ = allowed_touch_action_;
      gesture_sequence_.append(
          base::NumberToString(gesture_event->unique_touch_event_id));
      DCHECK(!drop_current_tap_ending_event_);
      break;

    case WebInputEvent::kGestureLongTap:
    case WebInputEvent::kGestureTwoFingerTap:
      gesture_sequence_.append("G");
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
  active_touch_action_ = allowed_touch_action_;
  white_listed_touch_action_ = touch_action;
}

FilterGestureEventResult TouchActionFilter::FilterPinchEventAndResetState() {
  if (drop_pinch_events_) {
    drop_pinch_events_ = false;
    return FilterGestureEventResult::kFilterGestureEventFiltered;
  }
  return FilterGestureEventResult::kFilterGestureEventAllowed;
}

FilterGestureEventResult TouchActionFilter::FilterScrollEventAndResetState() {
  if (drop_scroll_events_) {
    drop_scroll_events_ = false;
    return FilterGestureEventResult::kFilterGestureEventFiltered;
  }
  return FilterGestureEventResult::kFilterGestureEventAllowed;
}

void TouchActionFilter::ForceResetTouchActionForTest() {
  allowed_touch_action_.reset();
  active_touch_action_.reset();
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
  active_touch_action_ = allowed_touch_action_;
  has_deferred_events_ = false;
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
  if (!active_touch_action_.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("TouchAction.EffectiveTouchAction",
                            active_touch_action_.value(),
                            cc::kTouchActionMax + 1);

  // Report how often the effective touch action computed by blink is or is
  // not equivalent to the whitelisted touch action computed by the
  // compositor.
  UMA_HISTOGRAM_BOOLEAN(
      "TouchAction.EquivalentEffectiveAndWhiteListed",
      active_touch_action_.value() == white_listed_touch_action_);
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
    white_listed_touch_action_ = cc::kTouchActionAuto;
  } else {
    // Lack of a touch handler indicates that the page either has no
    // touch-action modifiers or that all its touch-action modifiers are auto.
    // Resetting the touch-action here allows forwarding of subsequent gestures
    // even if the underlying touches never reach the router.
    SetTouchAction(cc::kTouchActionAuto);
  }
}

void TouchActionFilter::OnSetWhiteListedTouchAction(
    cc::TouchAction white_listed_touch_action) {
  // We use '&' here to account for the multiple-finger case, which is the same
  // as OnSetTouchAction.
  white_listed_touch_action_ =
      white_listed_touch_action_ & white_listed_touch_action;
}

bool TouchActionFilter::ShouldSuppressScrolling(
    const blink::WebGestureEvent& gesture_event,
    cc::TouchAction touch_action) {
  DCHECK(gesture_event.GetType() == WebInputEvent::kGestureScrollBegin);

  if (gesture_event.data.scroll_begin.pointer_count >= 2) {
    // Any GestureScrollBegin with more than one fingers is like a pinch-zoom
    // for touch-actions, see crbug.com/632525. Therefore, we switch to
    // blocked-manipulation mode iff pinch-zoom is disallowed.
    return (touch_action & cc::kTouchActionPinchZoom) == 0;
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

  return (touch_action & minimal_conforming_touch_action) == 0;
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
    if (has_touch_event_handler_) {
      gesture_sequence_.append("H");
      active_touch_action_.reset();
    }
  }
}

}  // namespace content
