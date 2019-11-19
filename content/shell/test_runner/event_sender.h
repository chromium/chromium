// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_EVENT_SENDER_H_
#define CONTENT_SHELL_TEST_RUNNER_EVENT_SENDER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_touch_point.h"

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

namespace test_runner {

class TestInterfaces;
class WebWidgetTestProxy;
class WebTestDelegate;

// Key event location code introduced in DOM Level 3.
// See also: http://www.w3.org/TR/DOM-Level-3-Events/#events-keyboardevents
enum KeyLocationCode {
  DOMKeyLocationStandard = 0x00,
  DOMKeyLocationLeft = 0x01,
  DOMKeyLocationRight = 0x02,
  DOMKeyLocationNumpad = 0x03
};

class TEST_RUNNER_EXPORT EventSender {
 public:
  explicit EventSender(WebWidgetTestProxy*);
  virtual ~EventSender();

  void Reset();
  void Install(blink::WebLocalFrame*);

  void SetContextMenuData(const blink::WebContextMenuData&);

  void DoDragDrop(const blink::WebDragData&, blink::WebDragOperationsMask);

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
    blink::WebPoint pos;                       // For MouseMove.
    int milliseconds;                          // For LeapForward.
    int modifiers;
  };

  enum class MouseScrollType { PIXEL, TICK };

  void EnableDOMUIEventLogging();
  void FireKeyboardEventsToElement();
  void ClearKillRing();

  std::vector<std::string> ContextClick();

  void TextZoomIn();
  void TextZoomOut();

  void ZoomPageIn();
  void ZoomPageOut();
  void SetPageZoomFactor(double zoom_factor);

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

  void GestureScrollBegin(gin::Arguments* args);
  void GestureScrollEnd(gin::Arguments* args);
  void GestureScrollUpdate(gin::Arguments* args);
  void GestureTap(gin::Arguments* args);
  void GestureTapDown(gin::Arguments* args);
  void GestureShowPress(gin::Arguments* args);
  void GestureTapCancel(gin::Arguments* args);
  void GestureLongPress(gin::Arguments* args);
  void GestureLongTap(gin::Arguments* args);
  void GestureTwoFingerTap(gin::Arguments* args);

  void MouseScrollBy(gin::Arguments* args, MouseScrollType scroll_type);
  void MouseMoveTo(gin::Arguments* args);
  void MouseLeave(blink::WebPointerProperties::PointerType, int pointerId);
  void ScheduleAsynchronousClick(int button_number, int modifiers);
  void ScheduleAsynchronousKeyDown(const std::string& code_str,
                                   int modifiers,
                                   KeyLocationCode location);
  // Consumes the transient user activation state for follow-up tests that don't
  // expect it.
  void ConsumeUserActivation();

  base::TimeTicks GetCurrentEventTime() const;

  void DoLeapForward(int milliseconds);

  uint32_t GetUniqueTouchEventId(gin::Arguments* args);
  void SendCurrentTouchEvent(blink::WebInputEvent::Type, gin::Arguments* args);

  void GestureEvent(blink::WebInputEvent::Type, gin::Arguments*);

  void UpdateClickCountForButton(blink::WebMouseEvent::Button);

  blink::WebMouseWheelEvent GetMouseWheelEvent(gin::Arguments* args,
                                               MouseScrollType scroll_type,
                                               bool* send_gestures);
  void InitPointerProperties(gin::Arguments* args,
                             blink::WebPointerProperties* e,
                             float* radius_x,
                             float* radius_y);

  void FinishDragAndDrop(const blink::WebMouseEvent&, blink::WebDragOperation);

  int ModifiersForPointer(int pointer_id);
  void DoDragAfterMouseUp(const blink::WebMouseEvent&);
  void DoDragAfterMouseMove(const blink::WebMouseEvent&);
  void ReplaySavedEvents();
  blink::WebInputEventResult HandleInputEventOnViewOrPopup(
      const blink::WebInputEvent& event);

  void SendGesturesForMouseWheelEvent(
      const blink::WebMouseWheelEvent wheel_event);

  std::unique_ptr<blink::WebInputEvent> TransformScreenToWidgetCoordinates(
      const blink::WebInputEvent& event);

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

  WebWidgetTestProxy* web_widget_test_proxy_;
  TestInterfaces* interfaces();
  WebTestDelegate* delegate();
  const blink::WebView* view() const;
  blink::WebView* view();
  blink::WebWidget* widget();
  blink::WebFrameWidget* mainFrameWidget();

  bool force_layout_on_events_;

  // When set to true (the default value), we batch mouse move and mouse up
  // events so we can simulate drag & drop.
  bool is_drag_mode_;

  int touch_modifiers_;
  bool touch_cancelable_;
  std::vector<blink::WebTouchPoint> touch_points_;

  std::unique_ptr<blink::WebContextMenuData> last_context_menu_data_;

  blink::WebDragData current_drag_data_;

  // Location of the touch point that initiated a gesture.
  blink::WebFloatPoint current_gesture_location_;

  // Mouse-like pointer properties.
  struct PointerState {
    // Last pressed button (Left/Right/Middle or None).
    blink::WebMouseEvent::Button pressed_button_;

    // A bitwise OR of the WebMouseEvent::*ButtonDown values corresponding to
    // currently pressed buttons of the pointer (e.g. pen or mouse).
    int current_buttons_;

    // Location of last mouseMoveTo event of this pointer.
    blink::WebPoint last_pos_;

    int modifiers_;

    PointerState()
        : pressed_button_(blink::WebMouseEvent::Button::kNoButton),
          current_buttons_(0),
          last_pos_(blink::WebPoint(0, 0)),
          modifiers_(0) {}
  };
  typedef std::unordered_map<int, PointerState> PointerStateMap;
  PointerStateMap current_pointer_state_;

  bool replaying_saved_events_;

  base::circular_deque<SavedEvent> mouse_event_queue_;

  blink::WebDragOperationsMask current_drag_effects_allowed_;

  // Time and place of the last mouse up event.
  base::TimeTicks last_click_time_;
  blink::WebPoint last_click_pos_;

  // The last button number passed to mouseDown and mouseUp.
  // Used to determine whether the click count continues to increment or not.
  static blink::WebMouseEvent::Button last_button_type_;

  blink::WebDragOperation current_drag_effect_;

  base::TimeDelta time_offset_;
  int click_count_;
  // Timestamp of the last event that was dispatched
  base::TimeTicks last_event_timestamp_;

  base::WeakPtrFactory<EventSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EventSender);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_EVENT_SENDER_H_
