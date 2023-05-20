// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_ACTION_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_ACTION_FILTER_H_

#include <string>

#include "cc/input/touch_action.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {
class WebGestureEvent;
}

namespace content {

class MockRenderWidgetHost;

enum class FilterGestureEventResult {
  kFilterGestureEventAllowed,
  kFilterGestureEventFiltered,
  kFilterGestureEventDelayed
};

// The TouchActionFilter is responsible for filtering scroll and pinch gesture
// events according to the CSS touch-action values the renderer has sent for
// each touch point.
// For details see the touch-action design doc at http://goo.gl/KcKbxQ.
class CONTENT_EXPORT TouchActionFilter {
 public:
  TouchActionFilter();

  TouchActionFilter(const TouchActionFilter&) = delete;
  TouchActionFilter& operator=(const TouchActionFilter&) = delete;

  ~TouchActionFilter();

  // Returns kFilterGestureEventFiltered if the supplied gesture event should be
  // dropped based on the current touch-action state.
  // kFilterGestureEventDelayed if the |scrolling_touch_action_| has no value.
  // Returns kFilterGestureEventAllowed, and possibly modifies the event's
  // directional parameters to make the event compatible with the effective
  // touch-action.
  FilterGestureEventResult FilterGestureEvent(
      blink::WebGestureEvent* gesture_event);

  // Called when a set-touch-action message is received from the renderer
  // for a touch start event that is currently in flight.
  void OnSetTouchAction(cc::TouchAction touch_action);

  // Called at the end of a touch action sequence in order to log when a
  // compositor allowed touch action is or is not equivalent to the allowed
  // touch action.
  void ReportAndResetTouchAction();

  // Called when a set-compositor-allowed-touch-action message is received from
  // the renderer for a touch start event that is currently in flight.
  void OnSetCompositorAllowedTouchAction(cc::TouchAction);

  absl::optional<cc::TouchAction> allowed_touch_action() const {
    return allowed_touch_action_;
  }

  absl::optional<cc::TouchAction> active_touch_action() const {
    return active_touch_action_;
  }

  cc::TouchAction compositor_allowed_touch_action() const {
    return compositor_allowed_touch_action_;
  }

  bool has_touch_event_handler_for_testing() const {
    return has_touch_event_handler_;
  }

  void SetForceEnableZoom(bool enabled) { force_enable_zoom_ = enabled; }

  void OnHasTouchEventHandlers(bool has_handlers);

  void IncreaseActiveTouches();
  void DecreaseActiveTouches();

  void ForceResetTouchActionForTest();

  // Debugging only.
  void AppendToGestureSequenceForDebugging(const char* str);

 private:
  friend class InputRouterImplTest;
  friend class InputRouterImplTestBase;
  friend class MockRenderWidgetHost;
  friend class TouchActionFilterTest;
  friend class TouchActionFilterPinchTest;
  friend class SitePerProcessBrowserTouchActionTest;

  bool ShouldSuppressScrolling(const blink::WebGestureEvent&,
                               cc::TouchAction touch_action,
                               bool is_active_touch_action);
  FilterGestureEventResult FilterScrollEventAndResetState();
  FilterGestureEventResult FilterPinchEventAndResetState();
  void ResetTouchAction();
  void SetTouchAction(cc::TouchAction touch_action);

  // Whether scroll gestures should be discarded due to touch-action.
  bool drop_scroll_events_ = false;

  // Whether pinch gestures should be discarded due to touch-action.
  bool drop_pinch_events_ = false;

  // Whether a tap ending event in this sequence should be discarded because a
  // previous GestureTapUnconfirmed event was turned into a GestureTap.
  bool drop_current_tap_ending_event_ = false;

  // True iff the touch action of the last TapUnconfirmed or Tap event was
  // TOUCH_ACTION_AUTO. The double tap event depends on the touch action of the
  // previous tap or tap unconfirmed. Only valid between a TapUnconfirmed or Tap
  // and the next DoubleTap.
  bool allow_current_double_tap_event_ = true;

  // Force enable zoom for Accessibility.
  bool force_enable_zoom_ = false;

  // Indicates whether this page has touch event handler or not. Set by
  // InputRouterImpl::OnHasTouchEventHandlers. Default to false because one
  // could not scroll anyways when there is no content, and this is consistent
  // with the default state committed after DocumentLoader::DidCommitNavigation.
  // TODO(savella): Split touch_event_handler into touch_event_handler and
  // non_auto_touch_action.
  bool has_touch_event_handler_ = false;

  // True if an active gesture sequence is in progress. i.e. after GTD and
  // before GSE.
  bool gesture_sequence_in_progress_ = false;

  bool has_deferred_events_ = false;

  // True if scroll gestures are allowed to be used for cursor control. We set
  // this to false if a long press or double press has occurred in the current
  // gesture sequence, to prevent the cursor control feature from interfering
  // with long press drag selection and double press drag selection.
  bool allow_cursor_control_ = true;

  // Increment at receiving ACK for touch start and decrement at touch end.
  int num_of_active_touches_ = 0;

  // What touch actions are currently permitted.
  absl::optional<cc::TouchAction> allowed_touch_action_;

  // The touch action that is used for the current gesture sequence. At the
  // touch sequence end, the |allowed_touch_action_| is reset while this remains
  // set as the effective touch action, for the still in progress gesture
  // sequence due to fling.
  absl::optional<cc::TouchAction> active_touch_action_;

  // Allowed touch action received from the compositor.
  cc::TouchAction compositor_allowed_touch_action_;

  // Debugging only.
  std::string gesture_sequence_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_ACTION_FILTER_H_
