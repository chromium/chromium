// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_EVENT_SENDER_H_
#define CONTENT_WEB_TEST_RENDERER_EVENT_SENDER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_point.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "ui/gfx/geometry/point.h"

namespace blink {
class WebFrameWidget;
class WebLocalFrame;
class WebView;
class WebWidget;
struct WebContextMenuData;
}  // namespace blink

namespace gin {
class Arguments;
}  // namespace gin

namespace content {
class TestRunner;
class WebViewTestProxy;
class WebWidgetTestProxy;

// Key event location code introduced in DOM Level 3.
// See also: http://www.w3.org/TR/DOM-Level-3-Events/#events-keyboardevents
enum KeyLocationCode {
  DOMKeyLocationStandard = 0x00,
  DOMKeyLocationLeft = 0x01,
  DOMKeyLocationRight = 0x02,
  DOMKeyLocationNumpad = 0x03
};

class EventSender {
 public:
  explicit EventSender(WebWidgetTestProxy*);
  virtual ~EventSender();

  void Reset();
  void Install(blink::WebLocalFrame*);

  void SetContextMenuData(const blink::WebContextMenuData&);

  void DoDragDrop(const blink::WebDragData&, blink::DragOperationsMask);

  // Methods used to implement pointer requests and override behaviour.
  bool RequestPointerLock(blink::WebLocalFrame* requester_frame,
                          blink::WebWidgetClient::PointerLockCallback callback);
  void RequestPointerUnlock();
  bool IsPointerLocked() { return pointer_locked_; }

 private:
  friend class EventSenderBindings;

  void MouseDown(int button_number, int modifiers);
  void MouseUp(int button_number, int modifiers);
  void PointerDown(int button_number,
                   int modifiers,
                   blink::WebPointerProperties::PointerType,
                   int pointerId,
                   float pressure,
                   int tiltX,
                   int tiltY);
  void PointerUp(int button_number,
                 int modifiers,
                 blink::WebPointerProperties::PointerType,
                 int pointerId,
                 float pressure,
                 int tiltX,
                 int tiltY);
  void SetMouseButtonState(int button_number, int modifiers);

  void KeyDown(const std::string& code_str,
               int modifiers,
               KeyLocationCode location);

  struct SavedEvent {
    enum SavedEventType {
      TYPE_UNSPECIFIED,
      TYPE_MOUSE_UP,
      TYPE_MOUSE_MOVE,
      TYPE_LEAP_FORWARD
    };

    SavedEvent();

    SavedEventType type;
    blink::WebMouseEvent::Button button_type;  // For MouseUp.
    gfx::PointF pos;                           // For MouseMove.
    int milliseconds;                          // For LeapForward.
    int modifiers;
  };

  enum class MouseScrollType { PIXEL, TICK };

  enum class NextPointerLockAction {
    kWillSucceedAsync,
    kTestWillRespond,
    kWillFail,
  };

  TestRunner* test_runner();
  WebViewTestProxy* web_view_proxy();
  const blink::WebView* view() const;
  blink::WebView* view();
  blink::WebWidget* widget();
  blink::WebFrameWidget* MainFrameWidget();

  void EnableDOMUIEventLogging();
  void FireKeyboardEventsToElement();
  void ClearKillRing();

  std::vector<std::string> ContextClick();

  void ClearTouchPoints();
  void ReleaseTouchPoint(unsigned index);
  void UpdateTouchPoint(unsigned index, float x, float y, gin::Arguments* args);
  void CancelTouchPoint(unsigned index);
  void SetTouchModifier(const std::string& key_name, bool set_mask);
  void SetTouchCancelable(bool cancelable);
  void ThrowTouchPointError();

  void DumpFilenameBeingDragged();

  void GestureScrollFirstPoint(float x, float y);

  void TouchStart(gin::Arguments* args);
  void TouchMove(gin::Arguments* args);
  void TouchCancel(gin::Arguments* args);
  void TouchEnd(gin::Arguments* args);
  void NotifyStartOfTouchScroll();

  void LeapForward(int milliseconds);

  void BeginDragWithItems(
      const blink::WebVector<blink::WebDragData::Item>& items);
  void BeginDragWithFiles(const std::vector<std::string>& files);
  void BeginDragWithStringData(const std::string& data,
                               const std::string& mime_type);

  void AddTouchPoint(float x, float y, gin::Arguments* args);

  void GestureScrollBegin(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureScrollEnd(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureScrollUpdate(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureTap(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureTapDown(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureShowPress(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureTapCancel(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureLongPress(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureLongTap(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureTwoFingerTap(blink::WebLocalFrame* frame, gin::Arguments* args);

  void MouseScrollBy(gin::Arguments* args, MouseScrollType scroll_type);
  void MouseMoveTo(blink::WebLocalFrame* frame, gin::Arguments* args);
  void MouseLeave(blink::WebPointerProperties::PointerType, int pointerId);
  void ScheduleAsynchronousClick(blink::WebLocalFrame* frame,
                                 int button_number,
                                 int modifiers);
  void ScheduleAsynchronousKeyDown(blink::WebLocalFrame* frame,
                                   const std::string& code_str,
                                   int modifiers,
                                   KeyLocationCode location);
  // Consumes the transient user activation state for follow-up tests that don't
  // expect it.
  void ConsumeUserActivation();

  // Controls behaviour of the next call to RequestPointerLock().
  void SetNextPointerLockAction(NextPointerLockAction action);
  // One possible response to RequestPointerLock(). May be called automatically
  // or by the test directly, depending on NextPointerLockAction.
  void DidAcquirePointerLock();
  // One possible response to RequestPointerLock(). May be called automatically
  // or by the test directly, depending on NextPointerLockAction.
  void DidNotAcquirePointerLock();
  // Ends a pointer lock. May be called in response to RequestPointerUnlock() or
  // by the test directly.
  void DidLosePointerLock();

  base::TimeTicks GetCurrentEventTime() const;

  void DoLeapForward(int milliseconds);

  uint32_t GetUniqueTouchEventId(gin::Arguments* args);
  void SendCurrentTouchEvent(blink::WebInputEvent::Type, gin::Arguments* args);

  void GestureEvent(blink::WebInputEvent::Type,
                    blink::WebLocalFrame* frame,
                    gin::Arguments* args);

  void UpdateClickCountForButton(blink::WebMouseEvent::Button);

  blink::WebMouseWheelEvent GetMouseWheelEvent(gin::Arguments* args,
                                               MouseScrollType scroll_type,
                                               bool* send_gestures);
  void InitPointerProperties(gin::Arguments* args,
                             blink::WebPointerProperties* e,
                             float* radius_x,
                             float* radius_y);

  void FinishDragAndDrop(const blink::WebMouseEvent&, blink::DragOperation);

  int ModifiersForPointer(int pointer_id);
  void DoDragAfterMouseUp(const blink::WebMouseEvent&);
  void DoDragAfterMouseMove(const blink::WebMouseEvent&);
  void ReplaySavedEvents();
  blink::WebInputEventResult HandleInputEventOnViewOrPopup(
      const blink::WebInputEvent& event);

  void SendGesturesForMouseWheelEvent(
      const blink::WebMouseWheelEvent wheel_event);

  void UpdateLifecycleToPrePaint();

  base::TimeTicks last_event_timestamp() const { return last_event_timestamp_; }

  bool force_layout_on_events() const { return force_layout_on_events_; }
  void set_force_layout_on_events(bool force) {
    force_layout_on_events_ = force;
  }

  bool is_drag_mode() const { return is_drag_mode_; }
  void set_is_drag_mode(bool drag_mode) { is_drag_mode_ = drag_mode; }

#if defined(OS_WIN)
  int wm_key_down() const { return wm_key_down_; }
  void set_wm_key_down(int key_down) { wm_key_down_ = key_down; }

  int wm_key_up() const { return wm_key_up_; }
  void set_wm_key_up(int key_up) { wm_key_up_ = key_up; }

  int wm_char() const { return wm_char_; }
  void set_wm_char(int wm_char) { wm_char_ = wm_char; }

  int wm_dead_char() const { return wm_dead_char_; }
  void set_wm_dead_char(int dead_char) { wm_dead_char_ = dead_char; }

  int wm_sys_key_down() const { return wm_sys_key_down_; }
  void set_wm_sys_key_down(int key_down) { wm_sys_key_down_ = key_down; }

  int wm_sys_key_up() const { return wm_sys_key_up_; }
  void set_wm_sys_key_up(int key_up) { wm_sys_key_up_ = key_up; }

  int wm_sys_char() const { return wm_sys_char_; }
  void set_wm_sys_char(int sys_char) { wm_sys_char_ = sys_char; }

  int wm_sys_dead_char() const { return wm_sys_dead_char_; }
  void set_wm_sys_dead_char(int sys_dead_char) {
    wm_sys_dead_char_ = sys_dead_char;
  }

  int wm_key_down_;
  int wm_key_up_;
  int wm_char_;
  int wm_dead_char_;
  int wm_sys_key_down_;
  int wm_sys_key_up_;
  int wm_sys_char_;
  int wm_sys_dead_char_;
#endif

  WebWidgetTestProxy* const web_widget_test_proxy_;

  bool force_layout_on_events_;

  // When set to true (the default value), we batch mouse move and mouse up
  // events so we can simulate drag & drop.
  bool is_drag_mode_;

  int touch_modifiers_;
  bool touch_cancelable_;
  std::vector<blink::WebTouchPoint> touch_points_;

  std::unique_ptr<blink::WebContextMenuData> last_context_menu_data_;

  base::Optional<blink::WebDragData> current_drag_data_;

  // Location of the touch point that initiated a gesture.
  gfx::PointF current_gesture_location_;

  // Mouse-like pointer properties.
  struct PointerState {
    // Last pressed button (Left/Right/Middle or None).
    blink::WebMouseEvent::Button pressed_button_;

    // A bitwise OR of the WebMouseEvent::*ButtonDown values corresponding to
    // currently pressed buttons of the pointer (e.g. pen or mouse).
    int current_buttons_;

    // Location of last mouseMoveTo event of this pointer.
    gfx::PointF last_pos_;

    int modifiers_;

    PointerState()
        : pressed_button_(blink::WebMouseEvent::Button::kNoButton),
          current_buttons_(0),
          modifiers_(0) {}
  };
  typedef std::unordered_map<int, PointerState> PointerStateMap;
  PointerStateMap current_pointer_state_;

  bool replaying_saved_events_;

  base::circular_deque<SavedEvent> mouse_event_queue_;

  blink::DragOperationsMask current_drag_effects_allowed_;

  // Time and place of the last mouse up event.
  base::TimeTicks last_click_time_;
  gfx::PointF last_click_pos_;

  // The last button number passed to mouseDown and mouseUp.
  // Used to determine whether the click count continues to increment or not.
  static blink::WebMouseEvent::Button last_button_type_;

  blink::DragOperation current_drag_effect_;

  base::TimeDelta time_offset_;
  int click_count_;
  // Timestamp of the last event that was dispatched
  base::TimeTicks last_event_timestamp_;

  bool pointer_lock_pending_;
  bool pointer_unlock_pending_;
  bool pointer_locked_;
  // Tests can control the behaviour of RequestPointerLock() by specifying what
  // the next action should be though this.
  NextPointerLockAction next_pointer_lock_action_;
  // Callback held until a pointer lock request completes/fails. It is run
  // before calling through to DidAcquirePointerLock() or
  // DidNotAcquirePointerLock().
  blink::WebWidgetClient::PointerLockCallback pointer_locked_callback_;

  base::WeakPtrFactory<EventSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EventSender);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_EVENT_SENDER_H_
