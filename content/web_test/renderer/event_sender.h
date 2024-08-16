// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_EVENT_SENDER_H_
#define CONTENT_WEB_TEST_RENDERER_EVENT_SENDER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_point.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/gfx/geometry/point.h"

namespace blink {
class WebFrameWidget;
class WebLocalFrame;
class WebView;
class WebWidget;
struct ContextMenuData;
}  // namespace blink

namespace gin {
class Arguments;
}  // namespace gin

namespace content {
class TestRunner;
class WebFrameTestProxy;

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
  EventSender(blink::WebFrameWidget*, content::TestRunner* test_runner);

  EventSender(const EventSender&) = delete;
  EventSender& operator=(const EventSender&) = delete;

  virtual ~EventSender();

  void Reset();
  void Install(WebFrameTestProxy*);

  void SetContextMenuData(const blink::ContextMenuData&);

  void DoDragDrop(const blink::WebDragData&, blink::DragOperationsMask);

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
  enum KeyEventType { kKeyDown = 1, kKeyUp = 2, kKeyPress = kKeyDown | kKeyUp };
  void KeyEvent(KeyEventType event_type,
                const std::string& code_str,
                int modifiers,
                KeyLocationCode location,
                bool async);

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

  void DumpFilenameBeingDragged(blink::WebLocalFrame* frame);

  void TouchStart(gin::Arguments* args);
  void TouchMove(gin::Arguments* args);
  void TouchCancel(gin::Arguments* args);
  void TouchEnd(gin::Arguments* args);
  void NotifyStartOfTouchScroll();

  void LeapForward(int milliseconds);

  void BeginDragWithItems(
      blink::WebLocalFrame* frame,
      const blink::WebVector<blink::WebDragData::Item>& items);
  void BeginDragWithFiles(blink::WebLocalFrame* frame,
                          const std::vector<std::string>& files);
  void BeginDragWithStringData(blink::WebLocalFrame* frame,
                               const std::string& data,
                               const std::string& mime_type);

  void AddTouchPoint(float x, float y, gin::Arguments* args);

  void GestureScrollPopup(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureTap(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureTapDown(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureShowPress(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureTapCancel(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureLongPress(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureLongTap(blink::WebLocalFrame* frame, gin::Arguments* args);
  void GestureTwoFingerTap(blink::WebLocalFrame* frame, gin::Arguments* args);

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

  void FinishDragAndDrop(const blink::WebMouseEvent&,
                         ui::mojom::DragOperation,
                         bool);

  int ModifiersForPointer(int pointer_id);
  void DoDragAfterMouseUp(const blink::WebMouseEvent&);
  void DoDragAfterMouseMove(const blink::WebMouseEvent&);
  void ReplaySavedEvents();
  std::optional<blink::WebInputEventResult> HandleInputEventOnViewOrPopup(
      const blink::WebInputEvent& event,
      bool async = false);

  void SendGesturesForMouseWheelEvent(
      const blink::WebMouseWheelEvent wheel_event);

  void UpdateLifecycleToPrePaint();

  // Web tests are written to be dsf-independent. This scale should be applied
  // to coordinates provided from js, to convert them to physical pixels when
  // UseZoomForDSF is enabled.
  float DeviceScaleFactorForEvents();

  base::TimeTicks last_event_timestamp() const { return last_event_timestamp_; }

  bool force_layout_on_events() const { return force_layout_on_events_; }
  void set_force_layout_on_events(bool force) {
    force_layout_on_events_ = force;
  }

  bool is_drag_mode() const { return is_drag_mode_; }
  void set_is_drag_mode(bool drag_mode) { is_drag_mode_ = drag_mode; }

#if BUILDFLAG(IS_WIN)
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

  const raw_ptr<blink::WebFrameWidget> web_frame_widget_;
  const raw_ptr<TestRunner> test_runner_;

  bool force_layout_on_events_;

  // Currently pressed modifiers for key events.
  int key_modifiers_ = 0;

  // When set to true (the default value), we batch mouse move and mouse up
  // events so we can simulate drag & drop.
  bool is_drag_mode_;

  int touch_modifiers_;
  bool touch_cancelable_;
  std::vector<blink::WebTouchPoint> touch_points_;

  std::unique_ptr<blink::ContextMenuData> last_context_menu_data_;

  std::optional<blink::WebDragData> current_drag_data_;

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

  bool replaying_saved_events_ = false;

  base::circular_deque<SavedEvent> mouse_event_queue_;

  blink::DragOperationsMask current_drag_effects_allowed_;

  // Time and place of the last mouse up event.
  base::TimeTicks last_click_time_;
  gfx::PointF last_click_pos_;

  // The last button number passed to mouseDown and mouseUp.
  // Used to determine whether the click count continues to increment or not.
  static blink::WebMouseEvent::Button last_button_type_;

  ui::mojom::DragOperation current_drag_effect_;

  base::TimeDelta time_offset_;
  int click_count_;
  // Timestamp of the last event that was dispatched
  base::TimeTicks last_event_timestamp_;

  base::WeakPtrFactory<EventSender> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_EVENT_SENDER_H_
