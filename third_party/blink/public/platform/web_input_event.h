/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_INPUT_EVENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_INPUT_EVENT_H_

#include <string.h>

#include "base/time/time.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_pointer_properties.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_touch_point.h"

namespace blink {

// The classes defined in this file are intended to be used with
// WebWidget's HandleInputEvent method.  These event types are cross-
// platform and correspond closely to WebCore's Platform*Event classes.
//
// WARNING! These classes must remain PODs (plain old data).  They are
// intended to be "serializable" by copying their raw bytes, so they must
// not contain any non-bit-copyable member variables!
//
// Furthermore, the class members need to be packed so they are aligned
// properly and don't have paddings/gaps, otherwise memory check tools
// like Valgrind will complain about uninitialized memory usage when
// transferring these classes over the wire.

#pragma pack(push, 4)

// WebInputEvent --------------------------------------------------------------

class WebInputEvent {
 public:
  // When we use an input method (or an input method editor), we receive
  // two events for a keypress. The former event is a keydown, which
  // provides a keycode, and the latter is a textinput, which provides
  // a character processed by an input method. (The mapping from a
  // keycode to a character code is not trivial for non-English
  // keyboards.)
  // To support input methods, Safari sends keydown events to WebKit for
  // filtering. WebKit sends filtered keydown events back to Safari,
  // which sends them to input methods.
  // Unfortunately, it is hard to apply this design to Chrome because of
  // our multiprocess architecture. An input method is running in a
  // browser process. On the other hand, WebKit is running in a renderer
  // process. So, this design results in increasing IPC messages.
  // To support input methods without increasing IPC messages, Chrome
  // handles keyboard events in a browser process and send asynchronous
  // input events (to be translated to DOM events) to a renderer
  // process.
  // This design is mostly the same as the one of Windows and Mac Carbon.
  // So, for what it's worth, our Linux and Mac front-ends emulate our
  // Windows front-end. To emulate our Windows front-end, we can share
  // our back-end code among Windows, Linux, and Mac.
  // TODO(hbono): Issue 18064: remove the KeyDown type since it isn't
  // used in Chrome any longer.

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blink_public.web
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: WebInputEventType
  enum Type {
    kUndefined = -1,
    kTypeFirst = kUndefined,

    // WebMouseEvent
    kMouseDown,
    kMouseTypeFirst = kMouseDown,
    kMouseUp,
    kMouseMove,
    kMouseEnter,
    kMouseLeave,
    kContextMenu,
    kMouseTypeLast = kContextMenu,

    // WebMouseWheelEvent
    kMouseWheel,

    // WebKeyboardEvent
    kRawKeyDown,
    kKeyboardTypeFirst = kRawKeyDown,
    // KeyDown is a single event combining RawKeyDown and Char.  If KeyDown is
    // sent for a given keystroke, those two other events will not be sent.
    // Platforms tend to prefer sending in one format (Android uses KeyDown,
    // Windows uses RawKeyDown+Char, for example), but this is a weakly held
    // property as tools like WebDriver/DevTools might still send the other
    // format.
    kKeyDown,
    kKeyUp,
    kChar,
    kKeyboardTypeLast = kChar,

    // WebGestureEvent - input interpreted semi-semantically, most commonly from
    // touchscreen but also used for touchpad, mousewheel, and gamepad
    // scrolling.
    kGestureScrollBegin,
    kGestureTypeFirst = kGestureScrollBegin,
    kGestureScrollEnd,
    kGestureScrollUpdate,
    // Fling is a high-velocity and quickly released finger movement.
    // FlingStart is sent once and kicks off a scroll animation.
    kGestureFlingStart,
    kGestureFlingCancel,
    // Pinch is two fingers moving closer or farther apart.
    kGesturePinchBegin,
    kGesturePinchTypeFirst = kGesturePinchBegin,
    kGesturePinchEnd,
    kGesturePinchUpdate,
    kGesturePinchTypeLast = kGesturePinchUpdate,

    // The following types are variations and subevents of single-taps.
    //
    // Sent the moment the user's finger hits the screen.
    kGestureTapDown,
    // Sent a short interval later, after it seems the finger is staying in
    // place.  It's used to activate the link highlight ("show the press").
    kGestureShowPress,
    // Sent on finger lift for a simple, static, quick finger tap.  This is the
    // "main" event which maps to a synthetic mouse click event.
    kGestureTap,
    // Sent when a GestureTapDown didn't turn into any variation of GestureTap
    // (likely it turned into a scroll instead).
    kGestureTapCancel,
    // Sent as soon as the long-press timeout fires, while the finger is still
    // down.
    kGestureLongPress,
    // Sent when the finger is lifted following a GestureLongPress.
    kGestureLongTap,
    // Sent on finger lift when two fingers tapped at the same time without
    // moving.
    kGestureTwoFingerTap,
    // A rare event sent in place of GestureTap on desktop pages viewed on an
    // Android phone.  This tap could not yet be resolved into a GestureTap
    // because it may still turn into a GestureDoubleTap.
    kGestureTapUnconfirmed,

    // On Android, double-tap is two single-taps spread apart in time, like a
    // double-click. This event is only sent on desktop pages, and is always
    // preceded by GestureTapUnconfirmed. It's an instruction to Blink to
    // perform a PageScaleAnimation zoom onto the double-tapped content. (It's
    // treated differently from GestureTap with tapCount=2, which can also
    // happen.)
    // On desktop, this event may be used for a double-tap with two fingers on
    // a touchpad, as the desired effect is similar to Android's double-tap.
    kGestureDoubleTap,

    kGestureTypeLast = kGestureDoubleTap,

    // WebTouchEvent - raw touch pointers not yet classified into gestures.
    kTouchStart,
    kTouchTypeFirst = kTouchStart,
    kTouchMove,
    kTouchEnd,
    kTouchCancel,
    // TODO(nzolghadr): This event should be replaced with
    // kPointerCausedUaAction
    kTouchScrollStarted,
    kTouchTypeLast = kTouchScrollStarted,

    // WebPointerEvent: work in progress
    kPointerDown,
    kPointerTypeFirst = kPointerDown,
    kPointerUp,
    kPointerMove,
    kPointerRawMove,  // To be only used within blink.
    kPointerCancel,
    kPointerCausedUaAction,
    kPointerTypeLast = kPointerCausedUaAction,

    kTypeLast = kTouchTypeLast
  };

  // The modifier constants cannot change their values since pepper
  // does a 1-1 mapping of its values; see
  // content/renderer/pepper/event_conversion.cc
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blink_public.web
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: WebInputEventModifier
  enum Modifiers {
    // modifiers for all events:
    kShiftKey = 1 << 0,
    kControlKey = 1 << 1,
    kAltKey = 1 << 2,
    kMetaKey = 1 << 3,

    // modifiers for keyboard events:
    kIsKeyPad = 1 << 4,
    kIsAutoRepeat = 1 << 5,

    // modifiers for mouse events:
    kLeftButtonDown = 1 << 6,
    kMiddleButtonDown = 1 << 7,
    kRightButtonDown = 1 << 8,

    // Toggle modifers for all events.
    kCapsLockOn = 1 << 9,
    kNumLockOn = 1 << 10,

    kIsLeft = 1 << 11,
    kIsRight = 1 << 12,

    // Indicates that an event was generated on the touch screen while
    // touch accessibility is enabled, so the event should be handled
    // by accessibility code first before normal input event processing.
    kIsTouchAccessibility = 1 << 13,

    kIsComposing = 1 << 14,

    kAltGrKey = 1 << 15,
    kFnKey = 1 << 16,
    kSymbolKey = 1 << 17,

    kScrollLockOn = 1 << 18,

    // Whether this is a compatibility event generated due to a
    // native touch event. Mouse events generated from touch
    // events will set this.
    kIsCompatibilityEventForTouch = 1 << 19,

    kBackButtonDown = 1 << 20,
    kForwardButtonDown = 1 << 21,

    // Represents movement as a result of content changing under the cursor,
    // not actual physical movement of the pointer
    kRelativeMotionEvent = 1 << 22,

    // Indication this event was injected by the devtools.
    // TODO(dtapuska): Remove this flag once we are able to bind callbacks
    // in event sending.
    kFromDebugger = 1 << 23,

    // The set of non-stateful modifiers that specifically change the
    // interpretation of the key being pressed. For example; IsLeft,
    // IsRight, IsComposing don't change the meaning of the key
    // being pressed. NumLockOn, ScrollLockOn, CapsLockOn are stateful
    // and don't indicate explicit depressed state.
    kKeyModifiers = kSymbolKey | kFnKey | kAltGrKey | kMetaKey | kAltKey |
                    kControlKey | kShiftKey,
    kNoModifiers = 0,
  };

  // Indicates whether the browser needs to block on the ACK result for
  // this event, and if not, why (for metrics/diagnostics purposes).
  // These values are direct mappings of the values in PlatformEvent
  // so the values can be cast between the enumerations. static_asserts
  // checking this are in web/WebInputEventConversion.cpp.
  enum DispatchType {
    // Event can be canceled.
    kBlocking,
    // Event can not be canceled.
    kEventNonBlocking,
    // All listeners are passive; not cancelable.
    kListenersNonBlockingPassive,
    // This value represents a state which would have normally blocking
    // but was forced to be non-blocking during fling; not cancelable.
    kListenersForcedNonBlockingDueToFling,
    kLastDispatchType = kListenersForcedNonBlockingDueToFling,
  };

  // The rail mode for a wheel event specifies the axis on which scrolling is
  // expected to stick. If this axis is set to Free, then scrolling is not
  // stuck to any axis.
  enum RailsMode {
    kRailsModeFree = 0,
    kRailsModeHorizontal = 1,
    kRailsModeVertical = 2,
  };

  static const int kInputModifiers =
      kShiftKey | kControlKey | kAltKey | kMetaKey;

  static constexpr base::TimeTicks GetStaticTimeStampForTests() {
    // Note: intentionally use a relatively large delta from base::TimeTicks ==
    // 0. Otherwise, code that tracks the time ticks of the last event that
    // happened and computes a delta might get confused when the testing
    // timestamp is near 0, as the computed delta may very well be under the
    // delta threshhold.
    //
    // TODO(dcheng): This really shouldn't use FromInternalValue(), but
    // constexpr support for time operations is a bit busted...
    return base::TimeTicks::FromInternalValue(123'000'000);
  }

  // Returns true if the WebInputEvent |type| is a mouse event.
  static bool IsMouseEventType(WebInputEvent::Type type) {
    return kMouseTypeFirst <= type && type <= kMouseTypeLast;
  }

  // Returns true if the WebInputEvent |type| is a keyboard event.
  static bool IsKeyboardEventType(WebInputEvent::Type type) {
    return kKeyboardTypeFirst <= type && type <= kKeyboardTypeLast;
  }

  // Returns true if the WebInputEvent |type| is a touch event.
  static bool IsTouchEventType(WebInputEvent::Type type) {
    return kTouchTypeFirst <= type && type <= kTouchTypeLast;
  }

  // Returns true if the WebInputEvent is a gesture event.
  static bool IsGestureEventType(WebInputEvent::Type type) {
    return kGestureTypeFirst <= type && type <= kGestureTypeLast;
  }

  // Returns true if the WebInputEvent |type| is a pointer event.
  static bool IsPointerEventType(WebInputEvent::Type type) {
    return kPointerTypeFirst <= type && type <= kPointerTypeLast;
  }

  bool IsSameEventClass(const WebInputEvent& other) const {
    if (IsMouseEventType(type_))
      return IsMouseEventType(other.type_);
    if (IsGestureEventType(type_))
      return IsGestureEventType(other.type_);
    if (IsTouchEventType(type_))
      return IsTouchEventType(other.type_);
    if (IsKeyboardEventType(type_))
      return IsKeyboardEventType(other.type_);
    if (IsPointerEventType(type_))
      return IsPointerEventType(other.type_);
    return type_ == other.type_;
  }

  bool IsGestureScroll() const {
    switch (type_) {
      case Type::kGestureScrollBegin:
      case Type::kGestureScrollUpdate:
      case Type::kGestureScrollEnd:
        return true;
      default:
        return false;
    }
  }

  // Returns true if the WebInputEvent |type| is a pinch gesture event.
  static bool IsPinchGestureEventType(WebInputEvent::Type type) {
    return kGesturePinchTypeFirst <= type && type <= kGesturePinchTypeLast;
  }

  // Returns true if the WebInputEvent |type| is a fling gesture event.
  static bool IsFlingGestureEventType(WebInputEvent::Type type) {
    return kGestureFlingStart <= type && type <= kGestureFlingCancel;
  }

  static const char* GetName(WebInputEvent::Type type) {
#define CASE_TYPE(t)        \
  case WebInputEvent::k##t: \
    return #t
    switch (type) {
      CASE_TYPE(Undefined);
      CASE_TYPE(MouseDown);
      CASE_TYPE(MouseUp);
      CASE_TYPE(MouseMove);
      CASE_TYPE(MouseEnter);
      CASE_TYPE(MouseLeave);
      CASE_TYPE(ContextMenu);
      CASE_TYPE(MouseWheel);
      CASE_TYPE(RawKeyDown);
      CASE_TYPE(KeyDown);
      CASE_TYPE(KeyUp);
      CASE_TYPE(Char);
      CASE_TYPE(GestureScrollBegin);
      CASE_TYPE(GestureScrollEnd);
      CASE_TYPE(GestureScrollUpdate);
      CASE_TYPE(GestureFlingStart);
      CASE_TYPE(GestureFlingCancel);
      CASE_TYPE(GestureShowPress);
      CASE_TYPE(GestureTap);
      CASE_TYPE(GestureTapUnconfirmed);
      CASE_TYPE(GestureTapDown);
      CASE_TYPE(GestureTapCancel);
      CASE_TYPE(GestureDoubleTap);
      CASE_TYPE(GestureTwoFingerTap);
      CASE_TYPE(GestureLongPress);
      CASE_TYPE(GestureLongTap);
      CASE_TYPE(GesturePinchBegin);
      CASE_TYPE(GesturePinchEnd);
      CASE_TYPE(GesturePinchUpdate);
      CASE_TYPE(TouchStart);
      CASE_TYPE(TouchMove);
      CASE_TYPE(TouchEnd);
      CASE_TYPE(TouchCancel);
      CASE_TYPE(TouchScrollStarted);
      CASE_TYPE(PointerDown);
      CASE_TYPE(PointerUp);
      CASE_TYPE(PointerMove);
      CASE_TYPE(PointerRawMove);
      CASE_TYPE(PointerCancel);
      CASE_TYPE(PointerCausedUaAction);
    }
#undef CASE_TYPE
    NOTREACHED();
    return "";
  }

  float FrameScale() const { return frame_scale_; }
  void SetFrameScale(float scale) { frame_scale_ = scale; }

  WebFloatPoint FrameTranslate() const { return frame_translate_; }
  void SetFrameTranslate(WebFloatPoint translate) {
    frame_translate_ = translate;
  }

  Type GetType() const { return type_; }
  void SetType(Type type_param) { type_ = type_param; }

  int GetModifiers() const { return modifiers_; }
  void SetModifiers(int modifiers_param) { modifiers_ = modifiers_param; }

  base::TimeTicks TimeStamp() const { return time_stamp_; }
  void SetTimeStamp(base::TimeTicks time_stamp) { time_stamp_ = time_stamp; }

  unsigned size() const { return size_; }

 protected:
  // The root frame scale.
  float frame_scale_;

  // The root frame translation (applied post scale).
  WebFloatPoint frame_translate_;

  WebInputEvent(unsigned size,
                Type type,
                int modifiers,
                base::TimeTicks time_stamp) {
    // TODO(dtapuska): Remove this memset when we remove the chrome IPC of this
    // struct.
    memset(this, 0, size);
    time_stamp_ = time_stamp;
    size_ = size;
    type_ = type;
    modifiers_ = modifiers;
#if DCHECK_IS_ON()
    // If dcheck is on force failures if frame scale is not initialized
    // correctly by causing DIV0.
    frame_scale_ = 0;
#else
    frame_scale_ = 1;
#endif
  }

  explicit WebInputEvent(unsigned size_param) {
    // TODO(dtapuska): Remove this memset when we remove the chrome IPC of this
    // struct.
    memset(this, 0, size_param);
    time_stamp_ = base::TimeTicks();
    size_ = size_param;
    type_ = kUndefined;
#if DCHECK_IS_ON()
    // If dcheck is on force failures if frame scale is not initialized
    // correctly by causing DIV0.
    frame_scale_ = 0;
#else
    frame_scale_ = 1;
#endif
  }

  // Event time since platform start with microsecond resolution.
  base::TimeTicks time_stamp_;
  // The size of this structure, for serialization.
  unsigned size_;
  Type type_;
  int modifiers_;
};

#pragma pack(pop)

}  // namespace blink

#endif
