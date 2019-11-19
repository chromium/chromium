// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GESTURE_EVENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GESTURE_EVENT_H_

#include "cc/paint/element_id.h"
#include "third_party/blink/public/platform/web_float_size.h"
#include "third_party/blink/public/platform/web_gesture_device.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_scroll_types.h"
#include "ui/events/types/scroll_types.h"

namespace blink {

// See web_input_event.h for details why this pack is here.
#pragma pack(push, 4)

// WebGestureEvent ---------------------------------------------------------

class WebGestureEvent : public WebInputEvent {
 public:
  enum class InertialPhaseState : uint8_t {
    kUnknownMomentum = 0,  // No phase information.
    kNonMomentum,          // Regular scrolling phase.
    kMomentum,             // Momentum phase.
    kMaxValue = kMomentum,
  };

  bool is_source_touch_event_set_non_blocking;

  // The pointer type for the first touch point in the gesture.
  WebPointerProperties::PointerType primary_pointer_type =
      WebPointerProperties::PointerType::kUnknown;

  // If the WebGestureEvent has source_device == WebGestureDevice::kTouchscreen,
  // this field contains the unique identifier for the touch event that released
  // this event at TouchDispositionGestureFilter. If the WebGestureEvents was
  // not released through a touch event (e.g. timer-released gesture events or
  // gesture events with source_device != WebGestureDevice::kTouchscreen), the
  // field contains 0. See crbug.com/618738.
  uint32_t unique_touch_event_id;

  // This field exists to allow BrowserPlugin to mark GestureScroll events as
  // 'resent' to handle the case where an event is not consumed when first
  // encountered; it should be handled differently by the plugin when it is
  // sent for thesecond time. No code within Blink touches this, other than to
  // plumb it through event conversions.
  int resending_plugin_id;

  union {
    // Tap information must be set for GestureTap, GestureTapUnconfirmed,
    // and GestureDoubleTap events.
    struct {
      int tap_count;
      float width;
      float height;
      // |needs_wheel_event| for touchpad GestureDoubleTap has the same
      // semantics as |needs_wheel_event| for touchpad pinch described below.
      bool needs_wheel_event;
    } tap;

    struct {
      float width;
      float height;
    } tap_down;

    struct {
      float width;
      float height;
    } show_press;

    // This is used for both GestureLongPress and GestureLongTap.
    struct {
      float width;
      float height;
    } long_press;

    struct {
      float first_finger_width;
      float first_finger_height;
    } two_finger_tap;

    struct {
      // If set, used to target a scrollable area directly instead of performing
      // a hit-test. Should be used for gestures queued up internally within
      // the renderer process. This is an ElementIdType instead of ElementId
      // due to the fact that ElementId has a non-trivial constructor that
      // can't easily participate in this union of structs.
      cc::ElementIdType scrollable_area_element_id;
      // Initial motion that triggered the scroll.
      float delta_x_hint;
      float delta_y_hint;
      // number of pointers down.
      int pointer_count;
      // Default initialized to kScrollByPrecisePixel.
      ui::input_types::ScrollGranularity delta_hint_units;
      // The state of inertial phase scrolling. OSX has unique phases for normal
      // and momentum scroll events. Should always be kUnknownMomentumPhase for
      // touch based input as it generates GestureFlingStart instead.
      InertialPhaseState inertial_phase;
      // If true, this event will skip hit testing to find a scroll
      // target and instead just scroll the viewport.
      bool target_viewport;
      // True if this event is generated from a wheel event with synthetic
      // phase.
      bool synthetic;
    } scroll_begin;

    struct {
      float delta_x;
      float delta_y;
      float velocity_x;
      float velocity_y;
      InertialPhaseState inertial_phase;
      // Default initialized to kScrollByPrecisePixel.
      ui::input_types::ScrollGranularity delta_units;
    } scroll_update;

    struct {
      // The original delta units the ScrollBegin and ScrollUpdates
      // were sent as.
      ui::input_types::ScrollGranularity delta_units;
      // The state of inertial phase scrolling. OSX has unique phases for normal
      // and momentum scroll events. Should always be kUnknownMomentumPhase for
      // touch based input as it generates GestureFlingStart instead.
      InertialPhaseState inertial_phase;
      // True if this event is generated from a wheel event with synthetic
      // phase.
      bool synthetic;
      // True if this event is generated by the fling controller. The GSE
      // generated at the end of a fling should not get pushed to the
      // debouncing_deferral_queue_. This attribute is not mojofied and will not
      // get transferred across since it is only used on the browser.
      bool generated_by_fling_controller;
    } scroll_end;

    struct {
      float velocity_x;
      float velocity_y;
      // If true, this event will skip hit testing to find a scroll
      // target and instead just scroll the viewport.
      bool target_viewport;
    } fling_start;

    struct {
      bool target_viewport;
      // If set to true, don't treat fling_cancel
      // as a part of fling boost events sequence.
      bool prevent_boosting;
    } fling_cancel;

    // Note that for the pinch event types, |needs_wheel_event| and
    // |zoom_disabled| are browser side implementation details for touchpad
    // pinch zoom. From the renderer's perspective, both are always false.
    // TODO(mcnee): Remove these implementation details once the browser has its
    // own representation of a touchpad pinch event. See
    // https://crbug.com/563730
    struct {
      // |needs_wheel_event| is used to indicate the phase of handling a
      // touchpad pinch. When the event is created it is set to true, so that
      // the InputRouter knows to offer the event to the renderer as a wheel
      // event. Once the wheel event is acknowledged, |needs_wheel_event| is set
      // to false and the event is resent. When the InputRouter receives it the
      // second time, it knows to send the real gesture event to the renderer.
      bool needs_wheel_event;
      // If true, this event will not cause a change in page scale, but will
      // still be offered as a wheel event to any handlers.
      bool zoom_disabled;
      float scale;
    } pinch_update;

    struct {
      bool needs_wheel_event;
    } pinch_begin;

    struct {
      bool needs_wheel_event;
    } pinch_end;
  } data;

 private:
  // Widget coordinate, which is relative to the bound of current RenderWidget
  // (e.g. a plugin or OOPIF inside a RenderView). Similar to viewport
  // coordinates but without DevTools emulation transform or overscroll applied.
  WebFloatPoint position_in_widget_;

  // Screen coordinate
  WebFloatPoint position_in_screen_;

  WebGestureDevice source_device_;

 public:
  WebGestureEvent(Type type,
                  int modifiers,
                  base::TimeTicks time_stamp,
                  WebGestureDevice device = WebGestureDevice::kUninitialized)
      : WebInputEvent(sizeof(WebGestureEvent), type, modifiers, time_stamp),
        resending_plugin_id(-1),
        source_device_(device) {}

  WebGestureEvent()
      : WebInputEvent(sizeof(WebGestureEvent)),
        resending_plugin_id(-1),
        source_device_(WebGestureDevice::kUninitialized) {}

  const WebFloatPoint& PositionInWidget() const { return position_in_widget_; }
  const WebFloatPoint& PositionInScreen() const { return position_in_screen_; }

  void SetPositionInWidget(const WebFloatPoint& point) {
    position_in_widget_ = point;
  }

  void SetPositionInScreen(const WebFloatPoint& point) {
    position_in_screen_ = point;
  }

  WebGestureDevice SourceDevice() const { return source_device_; }
  void SetSourceDevice(WebGestureDevice device) { source_device_ = device; }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT float DeltaXInRootFrame() const;
  BLINK_PLATFORM_EXPORT float DeltaYInRootFrame() const;
  BLINK_PLATFORM_EXPORT ui::input_types::ScrollGranularity DeltaUnits() const;
  BLINK_PLATFORM_EXPORT WebFloatPoint PositionInRootFrame() const;
  BLINK_PLATFORM_EXPORT InertialPhaseState InertialPhase() const;
  BLINK_PLATFORM_EXPORT bool Synthetic() const;

  BLINK_PLATFORM_EXPORT float VelocityX() const;
  BLINK_PLATFORM_EXPORT float VelocityY() const;

  BLINK_PLATFORM_EXPORT WebFloatSize TapAreaInRootFrame() const;
  BLINK_PLATFORM_EXPORT int TapCount() const;

  BLINK_PLATFORM_EXPORT void ApplyTouchAdjustment(
      WebFloatPoint root_frame_coords);

  // Sets any scaled values to be their computed values and sets |frame_scale_|
  // back to 1 and |frame_translate_| X and Y coordinates back to 0.
  BLINK_PLATFORM_EXPORT void FlattenTransform();

  bool IsScrollEvent() const {
    switch (type_) {
      case kGestureScrollBegin:
      case kGestureScrollEnd:
      case kGestureScrollUpdate:
      case kGestureFlingStart:
      case kGestureFlingCancel:
      case kGesturePinchBegin:
      case kGesturePinchEnd:
      case kGesturePinchUpdate:
        return true;
      case kGestureTap:
      case kGestureTapUnconfirmed:
      case kGestureTapDown:
      case kGestureShowPress:
      case kGestureTapCancel:
      case kGestureTwoFingerTap:
      case kGestureLongPress:
      case kGestureLongTap:
        return false;
      default:
        NOTREACHED();
        return false;
    }
  }
#endif

  bool IsTargetViewport() const {
    switch (type_) {
      case kGestureScrollBegin:
        return data.scroll_begin.target_viewport;
      case kGestureFlingStart:
        return data.fling_start.target_viewport;
      case kGestureFlingCancel:
        return data.fling_cancel.target_viewport;
      default:
        return false;
    }
  }

  bool IsTouchpadZoomEvent() const {
    // Touchpad GestureDoubleTap also causes a page scale change like a touchpad
    // pinch gesture.
    return source_device_ == WebGestureDevice::kTouchpad &&
           (WebInputEvent::IsPinchGestureEventType(type_) ||
            type_ == kGestureDoubleTap);
  }

  bool NeedsWheelEvent() const {
    DCHECK(IsTouchpadZoomEvent());
    switch (type_) {
      case kGesturePinchBegin:
        return data.pinch_begin.needs_wheel_event;
      case kGesturePinchUpdate:
        return data.pinch_update.needs_wheel_event;
      case kGesturePinchEnd:
        return data.pinch_end.needs_wheel_event;
      case kGestureDoubleTap:
        return data.tap.needs_wheel_event;
      default:
        NOTREACHED();
        return false;
    }
  }

  void SetNeedsWheelEvent(bool needs_wheel_event) {
    DCHECK(!needs_wheel_event || IsTouchpadZoomEvent());
    switch (type_) {
      case kGesturePinchBegin:
        data.pinch_begin.needs_wheel_event = needs_wheel_event;
        break;
      case kGesturePinchUpdate:
        data.pinch_update.needs_wheel_event = needs_wheel_event;
        break;
      case kGesturePinchEnd:
        data.pinch_end.needs_wheel_event = needs_wheel_event;
        break;
      case kGestureDoubleTap:
        data.tap.needs_wheel_event = needs_wheel_event;
        break;
      default:
        break;
    }
  }
};

#pragma pack(pop)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GESTURE_EVENT_H_
