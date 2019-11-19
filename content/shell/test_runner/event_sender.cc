// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/event_sender.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/input/web_mouse_wheel_event_traits.h"
#include "content/renderer/render_widget.h"
#include "content/shell/test_runner/mock_spell_check.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_pointer_properties.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_context_menu_data.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "v8/include/v8.h"

using blink::WebContextMenuData;
using blink::WebDragData;
using blink::WebDragOperationsMask;
using blink::WebFloatPoint;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebKeyboardEvent;
using blink::WebLocalFrame;
using blink::WebMenuItemInfo;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPagePopup;
using blink::WebPoint;
using blink::WebPointerEvent;
using blink::WebPointerProperties;
using blink::WebString;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebURL;
using blink::WebVector;
using blink::WebView;

namespace test_runner {

namespace {

const int kRawMousePointerId = -1;
const char* const kPointerTypeStringUnknown = "";
const char* const kPointerTypeStringMouse = "mouse";
const char* const kPointerTypeStringTouch = "touch";
const char* const kPointerTypeStringPen = "pen";
const char* const kPointerTypeStringEraser = "eraser";

// Assigns |pointerType| from the provided |args|. Returns false if there was
// any error.
bool GetPointerType(gin::Arguments* args,
                    bool isOnlyMouseAndPenAllowed,
                    WebPointerProperties::PointerType& pointerType) {
  if (args->PeekNext().IsEmpty())
    return true;
  std::string pointer_type_string;
  if (!args->GetNext(&pointer_type_string)) {
    args->ThrowError();
    return false;
  }
  if (isOnlyMouseAndPenAllowed &&
      (pointer_type_string == kPointerTypeStringUnknown ||
       pointer_type_string == kPointerTypeStringTouch)) {
    args->ThrowError();
    return false;
  }
  if (pointer_type_string == kPointerTypeStringUnknown) {
    pointerType = WebMouseEvent::PointerType::kUnknown;
  } else if (pointer_type_string == kPointerTypeStringMouse) {
    pointerType = WebMouseEvent::PointerType::kMouse;
  } else if (pointer_type_string == kPointerTypeStringTouch) {
    pointerType = WebMouseEvent::PointerType::kTouch;
  } else if (pointer_type_string == kPointerTypeStringPen) {
    pointerType = WebMouseEvent::PointerType::kPen;
  } else if (pointer_type_string == kPointerTypeStringEraser) {
    pointerType = WebMouseEvent::PointerType::kEraser;
  } else {
    args->ThrowError();
    return false;
  }
  return true;
}

WebInputEvent::Type PointerEventTypeForTouchPointState(
    WebTouchPoint::State state) {
  switch (state) {
    case WebTouchPoint::kStateReleased:
      return WebInputEvent::Type::kPointerUp;
    case WebTouchPoint::kStateCancelled:
      return WebInputEvent::Type::kPointerCancel;
    case WebTouchPoint::kStatePressed:
      return WebInputEvent::Type::kPointerDown;
    case WebTouchPoint::kStateMoved:
      return WebInputEvent::Type::kPointerMove;
    case WebTouchPoint::kStateStationary:
    default:
      NOTREACHED();
      return WebInputEvent::Type::kUndefined;
  }
}

// Parses |pointerType|, |rawPointerId|, |pressure|, |tiltX| and |tiltY| from
// the provided |args|. Returns false if there was any error, assuming the last
// 3 of the five parsed parameters are optional.
bool getMousePenPointerProperties(
    gin::Arguments* args,
    WebPointerProperties::PointerType& pointerType,
    int& rawPointerId,
    float& pressure,
    int& tiltX,
    int& tiltY) {
  pointerType = WebPointerProperties::PointerType::kMouse;
  rawPointerId = kRawMousePointerId;
  pressure = std::numeric_limits<float>::quiet_NaN();
  tiltX = 0;
  tiltY = 0;

  // Only allow pen or mouse through this API.
  if (!GetPointerType(args, false, pointerType))
    return false;
  if (!args->PeekNext().IsEmpty()) {
    if (!args->GetNext(&rawPointerId)) {
      args->ThrowError();
      return false;
    }

    // Parse optional params
    if (!args->PeekNext().IsEmpty()) {
      if (!args->GetNext(&pressure)) {
        args->ThrowError();
        return false;
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&tiltX)) {
          args->ThrowError();
          return false;
        }
        if (!args->PeekNext().IsEmpty()) {
          if (!args->GetNext(&tiltY)) {
            args->ThrowError();
            return false;
          }
        }
      }
    }
  }

  return true;
}

WebMouseEvent::Button GetButtonTypeFromButtonNumber(int button_code) {
  switch (button_code) {
    case -1:
      return WebMouseEvent::Button::kNoButton;
    case 0:
      return WebMouseEvent::Button::kLeft;
    case 1:
      return WebMouseEvent::Button::kMiddle;
    case 2:
      return WebMouseEvent::Button::kRight;
    case 3:
      return WebMouseEvent::Button::kBack;
    case 4:
      return WebMouseEvent::Button::kForward;
  }
  NOTREACHED();
  return WebMouseEvent::Button::kNoButton;
}

int GetWebMouseEventModifierForButton(WebMouseEvent::Button button) {
  switch (button) {
    case WebMouseEvent::Button::kNoButton:
      return 0;
    case WebMouseEvent::Button::kLeft:
      return WebMouseEvent::kLeftButtonDown;
    case WebMouseEvent::Button::kMiddle:
      return WebMouseEvent::kMiddleButtonDown;
    case WebMouseEvent::Button::kRight:
      return WebMouseEvent::kRightButtonDown;
    case WebPointerProperties::Button::kBack:
      return WebMouseEvent::kBackButtonDown;
    case WebPointerProperties::Button::kForward:
      return WebMouseEvent::kForwardButtonDown;
    case WebPointerProperties::Button::kEraser:
      return 0;  // Not implemented yet
  }
  NOTREACHED();
  return 0;
}

const int kButtonsInModifiers =
    WebMouseEvent::kLeftButtonDown | WebMouseEvent::kMiddleButtonDown |
    WebMouseEvent::kRightButtonDown | WebMouseEvent::kBackButtonDown |
    WebMouseEvent::kForwardButtonDown;

int modifiersWithButtons(int modifiers, int buttons) {
  return (modifiers & ~kButtonsInModifiers) | (buttons & kButtonsInModifiers);
}

void InitMouseEventGeneric(WebMouseEvent::Button b,
                           int current_buttons,
                           const WebPoint& pos,
                           int click_count,
                           WebPointerProperties::PointerType pointerType,
                           int pointerId,
                           float pressure,
                           int tiltX,
                           int tiltY,
                           WebMouseEvent* e) {
  e->button = b;
  e->SetPositionInWidget(pos.x, pos.y);
  e->SetPositionInScreen(pos.x, pos.y);
  e->pointer_type = pointerType;
  e->id = pointerId;
  e->force = pressure;
  e->tilt_x = tiltX;
  e->tilt_y = tiltY;
  e->click_count = click_count;
}

void InitMouseEvent(WebMouseEvent::Button b,
                    int current_buttons,
                    const WebPoint& pos,
                    int click_count,
                    WebMouseEvent* e) {
  InitMouseEventGeneric(b, current_buttons, pos, click_count,
                        WebPointerProperties::PointerType::kMouse, 0, 0.0, 0, 0,
                        e);
}

void InitGestureEventFromMouseWheel(const WebMouseWheelEvent& wheel_event,
                                    WebGestureEvent* gesture_event) {
  gesture_event->SetPositionInWidget(wheel_event.PositionInWidget());
  gesture_event->SetPositionInScreen(wheel_event.PositionInScreen());
}

int GetKeyModifier(const std::string& modifier_name) {
  const char* characters = modifier_name.c_str();
  if (!strcmp(characters, "ctrlKey")
#ifndef __APPLE__
      || !strcmp(characters, "addSelectionKey")
#endif
          ) {
    return WebInputEvent::kControlKey;
  } else if (!strcmp(characters, "shiftKey") ||
             !strcmp(characters, "rangeSelectionKey")) {
    return WebInputEvent::kShiftKey;
  } else if (!strcmp(characters, "altKey")) {
    return WebInputEvent::kAltKey;
#ifdef __APPLE__
  } else if (!strcmp(characters, "metaKey") ||
             !strcmp(characters, "addSelectionKey")) {
    return WebInputEvent::kMetaKey;
#else
  } else if (!strcmp(characters, "metaKey")) {
    return WebInputEvent::kMetaKey;
#endif
  } else if (!strcmp(characters, "autoRepeat")) {
    return WebInputEvent::kIsAutoRepeat;
  } else if (!strcmp(characters, "copyKey")) {
#ifdef __APPLE__
    return WebInputEvent::kAltKey;
#else
    return WebInputEvent::kControlKey;
#endif
  } else if (!strcmp(characters, "accessKey")) {
#ifdef __APPLE__
    return WebInputEvent::kAltKey | WebInputEvent::kControlKey;
#else
    return WebInputEvent::kAltKey;
#endif
  } else if (!strcmp(characters, "leftButton")) {
    return WebInputEvent::kLeftButtonDown;
  } else if (!strcmp(characters, "middleButton")) {
    return WebInputEvent::kMiddleButtonDown;
  } else if (!strcmp(characters, "rightButton")) {
    return WebInputEvent::kRightButtonDown;
  } else if (!strcmp(characters, "backButton")) {
    return WebInputEvent::kBackButtonDown;
  } else if (!strcmp(characters, "forwardButton")) {
    return WebInputEvent::kForwardButtonDown;
  } else if (!strcmp(characters, "capsLockOn")) {
    return WebInputEvent::kCapsLockOn;
  } else if (!strcmp(characters, "numLockOn")) {
    return WebInputEvent::kNumLockOn;
  } else if (!strcmp(characters, "locationLeft")) {
    return WebInputEvent::kIsLeft;
  } else if (!strcmp(characters, "locationRight")) {
    return WebInputEvent::kIsRight;
  } else if (!strcmp(characters, "locationNumpad")) {
    return WebInputEvent::kIsKeyPad;
  } else if (!strcmp(characters, "isComposing")) {
    return WebInputEvent::kIsComposing;
  } else if (!strcmp(characters, "altGraphKey")) {
    return WebInputEvent::kAltGrKey;
  } else if (!strcmp(characters, "fnKey")) {
    return WebInputEvent::kFnKey;
  } else if (!strcmp(characters, "symbolKey")) {
    return WebInputEvent::kSymbolKey;
  } else if (!strcmp(characters, "scrollLockOn")) {
    return WebInputEvent::kScrollLockOn;
  }

  return 0;
}

int GetKeyModifiers(const std::vector<std::string>& modifier_names) {
  int modifiers = 0;
  for (std::vector<std::string>::const_iterator it = modifier_names.begin();
       it != modifier_names.end(); ++it) {
    modifiers |= GetKeyModifier(*it);
  }
  return modifiers;
}

int GetKeyModifiersFromV8(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  std::vector<std::string> modifier_names;
  if (value->IsString()) {
    modifier_names.push_back(gin::V8ToString(isolate, value));
  } else if (value->IsArray()) {
    gin::Converter<std::vector<std::string>>::FromV8(isolate, value,
                                                     &modifier_names);
  }
  return GetKeyModifiers(modifier_names);
}

WebMouseWheelEvent::Phase GetMouseWheelEventPhase(
    const std::string& phase_name) {
  if (phase_name == "phaseNone") {
    return WebMouseWheelEvent::kPhaseNone;
  } else if (phase_name == "phaseBegan") {
    return WebMouseWheelEvent::kPhaseBegan;
  } else if (phase_name == "phaseStationary") {
    return WebMouseWheelEvent::kPhaseStationary;
  } else if (phase_name == "phaseChanged") {
    return WebMouseWheelEvent::kPhaseChanged;
  } else if (phase_name == "phaseEnded") {
    return WebMouseWheelEvent::kPhaseEnded;
  } else if (phase_name == "phaseCancelled") {
    return WebMouseWheelEvent::kPhaseCancelled;
  } else if (phase_name == "phaseMayBegin") {
    return WebMouseWheelEvent::kPhaseMayBegin;
  }

  return WebMouseWheelEvent::kPhaseNone;
}

WebMouseWheelEvent::Phase GetMouseWheelEventPhaseFromV8(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value) {
  if (value->IsString())
    return GetMouseWheelEventPhase(gin::V8ToString(isolate, value));
  return WebMouseWheelEvent::kPhaseNone;
}

// Maximum distance (in space and time) for a mouse click to register as a
// double or triple click.
constexpr base::TimeDelta kMultipleClickTime = base::TimeDelta::FromSeconds(1);
const int kMultipleClickRadiusPixels = 5;
const char kSubMenuDepthIdentifier[] = "_";
const char kSubMenuIdentifier[] = " >";
const char kSeparatorIdentifier[] = "---------";
const char kDisabledIdentifier[] = "#";
const char kCheckedIdentifier[] = "*";

bool OutsideMultiClickRadius(const WebPoint& a, const WebPoint& b) {
  return ((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)) >
         kMultipleClickRadiusPixels * kMultipleClickRadiusPixels;
}

void PopulateCustomItems(const WebVector<WebMenuItemInfo>& customItems,
                         const std::string& prefix,
                         std::vector<std::string>* strings) {
  for (size_t i = 0; i < customItems.size(); ++i) {
    std::string prefixCopy = prefix;
    if (!customItems[i].enabled)
      prefixCopy = kDisabledIdentifier + prefix;
    if (customItems[i].checked)
      prefixCopy = kCheckedIdentifier + prefix;
    if (customItems[i].type == blink::WebMenuItemInfo::kSeparator) {
      strings->push_back(prefixCopy + kSeparatorIdentifier);
    } else if (customItems[i].type == blink::WebMenuItemInfo::kSubMenu) {
      strings->push_back(prefixCopy + customItems[i].label.Utf8() +
                         kSubMenuIdentifier);
      PopulateCustomItems(customItems[i].sub_menu_items,
                          prefixCopy + kSubMenuDepthIdentifier, strings);
    } else {
      strings->push_back(prefixCopy + customItems[i].label.Utf8());
    }
  }
}

// Because actual context menu is implemented by the browser side,
// this function does only what web_tests are expecting:
// - Many test checks the count of items. So returning non-zero value makes
// sense.
// - Some test compares the count before and after some action. So changing the
// count based on flags also makes sense. This function is doing such for some
// flags.
// - Some test even checks actual string content. So providing it would be also
// helpful.
std::vector<std::string> MakeMenuItemStringsFor(
    WebContextMenuData* context_menu,
    WebTestDelegate* delegate) {
  // These constants are based on Safari's context menu because tests are made
  // for it.
  static const char* kNonEditableMenuStrings[] = {
      "Back",        "Reload Page",     "Open in Dashbaord",
      "<separator>", "View Source",     "Save Page As",
      "Print Page",  "Inspect Element", nullptr};
  static const char* kEditableMenuStrings[] = {"Cut",
                                               "Copy",
                                               "<separator>",
                                               "Paste",
                                               "Spelling and Grammar",
                                               "Substitutions, Transformations",
                                               "Font",
                                               "Speech",
                                               "Paragraph Direction",
                                               "<separator>",
                                               nullptr};

  // This is possible because mouse events are cancelleable.
  if (!context_menu)
    return std::vector<std::string>();

  std::vector<std::string> strings;

  // Populate custom menu items if provided by blink.
  PopulateCustomItems(context_menu->custom_items, "", &strings);

  if (context_menu->is_editable) {
    for (const char** item = kEditableMenuStrings; *item; ++item) {
      strings.push_back(*item);
    }
    WebVector<WebString> suggestions;
    MockSpellCheck::FillSuggestionList(context_menu->misspelled_word,
                                       &suggestions);
    for (size_t i = 0; i < suggestions.size(); ++i) {
      strings.push_back(suggestions[i].Utf8());
    }
  } else {
    for (const char** item = kNonEditableMenuStrings; *item; ++item) {
      strings.push_back(*item);
    }
  }

  return strings;
}

// How much we should scroll per event - the value here is chosen to match the
// WebKit impl and web test results.
const float kScrollbarPixelsPerTick = 40.0f;

// Get the edit command corresponding to a keyboard event.
// Returns true if the specified event corresponds to an edit command, the name
// of the edit command will be stored in |*name|.
bool GetEditCommand(const WebKeyboardEvent& event, std::string* name) {
#if defined(OS_MACOSX)
  // We only cares about Left,Right,Up,Down keys with Command or Command+Shift
  // modifiers. These key events correspond to some special movement and
  // selection editor commands. These keys will be marked as system key, which
  // prevents them from being handled. Thus they must be handled specially.
  if ((event.GetModifiers() & ~WebKeyboardEvent::kShiftKey) !=
      WebKeyboardEvent::kMetaKey)
    return false;

  switch (event.windows_key_code) {
    case ui::VKEY_LEFT:
      *name = "MoveToBeginningOfLine";
      break;
    case ui::VKEY_RIGHT:
      *name = "MoveToEndOfLine";
      break;
    case ui::VKEY_UP:
      *name = "MoveToBeginningOfDocument";
      break;
    case ui::VKEY_DOWN:
      *name = "MoveToEndOfDocument";
      break;
    default:
      return false;
  }

  if (event.GetModifiers() & WebKeyboardEvent::kShiftKey)
    name->append("AndModifySelection");

  return true;
#else
  return false;
#endif
}

bool IsSystemKeyEvent(const WebKeyboardEvent& event) {
#if defined(OS_MACOSX)
  return event.GetModifiers() & WebInputEvent::kMetaKey &&
         event.windows_key_code != ui::VKEY_B &&
         event.windows_key_code != ui::VKEY_I;
#else
  return !!(event.GetModifiers() & WebInputEvent::kAltKey);
#endif
}

bool GetScrollUnits(gin::Arguments* args,
                    ui::input_types::ScrollGranularity* units) {
  std::string units_string;
  if (!args->PeekNext().IsEmpty()) {
    if (args->PeekNext()->IsString())
      args->GetNext(&units_string);
    if (units_string == "Page") {
      *units = ui::input_types::ScrollGranularity::kScrollByPage;
      return true;
    } else if (units_string == "Pixels") {
      *units = ui::input_types::ScrollGranularity::kScrollByPixel;
      return true;
    } else if (units_string == "PrecisePixels") {
      *units = ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
      return true;
    } else {
      args->ThrowError();
      return false;
    }
  } else {
    *units = ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
    return true;
  }
}

const char* kSourceDeviceStringTouchpad = "touchpad";
const char* kSourceDeviceStringTouchscreen = "touchscreen";

}  // namespace

class EventSenderBindings : public gin::Wrappable<EventSenderBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(base::WeakPtr<EventSender> sender,
                      blink::WebLocalFrame* frame);

 private:
  explicit EventSenderBindings(base::WeakPtr<EventSender> sender);
  ~EventSenderBindings() override;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Bound methods:
  void EnableDOMUIEventLogging();
  void FireKeyboardEventsToElement();
  void ClearKillRing();
  std::vector<std::string> ContextClick();
  void TextZoomIn();
  void TextZoomOut();
  void ZoomPageIn();
  void ZoomPageOut();
  void SetPageZoomFactor(double factor);
  void ClearTouchPoints();
  void ReleaseTouchPoint(unsigned index);
  void UpdateTouchPoint(unsigned index,
                        double x,
                        double y,
                        gin::Arguments* args);
  void CancelTouchPoint(unsigned index);
  void SetTouchModifier(const std::string& key_name, bool set_mask);
  void SetTouchCancelable(bool cancelable);
  void DumpFilenameBeingDragged();
  void GestureFlingCancel();
  void GestureFlingStart(float x,
                         float y,
                         float velocity_x,
                         float velocity_y,
                         gin::Arguments* args);
  bool IsFlinging();
  void GestureScrollFirstPoint(float x, float y);
  void TouchStart(gin::Arguments* args);
  void TouchMove(gin::Arguments* args);
  void TouchCancel(gin::Arguments* args);
  void TouchEnd(gin::Arguments* args);
  void NotifyStartOfTouchScroll();
  void LeapForward(int milliseconds);
  double LastEventTimestamp();
  void BeginDragWithFiles(const std::vector<std::string>& files);
  void BeginDragWithStringData(const std::string& data,
                               const std::string& mime_type);
  void AddTouchPoint(double x, double y, gin::Arguments* args);
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
  void ContinuousMouseScrollBy(gin::Arguments* args);
  void MouseMoveTo(gin::Arguments* args);
  void MouseLeave(gin::Arguments* args);
  void MouseScrollBy(gin::Arguments* args);
  void ScheduleAsynchronousClick(gin::Arguments* args);
  void ScheduleAsynchronousKeyDown(gin::Arguments* args);
  void ConsumeUserActivation();
  void MouseDown(gin::Arguments* args);
  void MouseUp(gin::Arguments* args);
  void SetMouseButtonState(gin::Arguments* args);
  void KeyDown(gin::Arguments* args);

  // Binding properties:
  bool ForceLayoutOnEvents() const;
  void SetForceLayoutOnEvents(bool force);
  bool IsDragMode() const;
  void SetIsDragMode(bool drag_mode);

#if defined(OS_WIN)
  int WmKeyDown() const;
  void SetWmKeyDown(int key_down);

  int WmKeyUp() const;
  void SetWmKeyUp(int key_up);

  int WmChar() const;
  void SetWmChar(int wm_char);

  int WmDeadChar() const;
  void SetWmDeadChar(int dead_char);

  int WmSysKeyDown() const;
  void SetWmSysKeyDown(int key_down);

  int WmSysKeyUp() const;
  void SetWmSysKeyUp(int key_up);

  int WmSysChar() const;
  void SetWmSysChar(int sys_char);

  int WmSysDeadChar() const;
  void SetWmSysDeadChar(int sys_dead_char);
#endif

  base::WeakPtr<EventSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(EventSenderBindings);
};

gin::WrapperInfo EventSenderBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

EventSenderBindings::EventSenderBindings(base::WeakPtr<EventSender> sender)
    : sender_(sender) {}

EventSenderBindings::~EventSenderBindings() {}

// static
void EventSenderBindings::Install(base::WeakPtr<EventSender> sender,
                                  WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<EventSenderBindings> bindings =
      gin::CreateHandle(isolate, new EventSenderBindings(sender));
  if (bindings.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global->Set(context, gin::StringToV8(isolate, "eventSender"), bindings.ToV8())
      .Check();
}

gin::ObjectTemplateBuilder EventSenderBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<EventSenderBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("enableDOMUIEventLogging",
                 &EventSenderBindings::EnableDOMUIEventLogging)
      .SetMethod("fireKeyboardEventsToElement",
                 &EventSenderBindings::FireKeyboardEventsToElement)
      .SetMethod("clearKillRing", &EventSenderBindings::ClearKillRing)
      .SetMethod("contextClick", &EventSenderBindings::ContextClick)
      .SetMethod("textZoomIn", &EventSenderBindings::TextZoomIn)
      .SetMethod("textZoomOut", &EventSenderBindings::TextZoomOut)
      .SetMethod("zoomPageIn", &EventSenderBindings::ZoomPageIn)
      .SetMethod("zoomPageOut", &EventSenderBindings::ZoomPageOut)
      .SetMethod("setPageZoomFactor", &EventSenderBindings::SetPageZoomFactor)
      .SetMethod("clearTouchPoints", &EventSenderBindings::ClearTouchPoints)
      .SetMethod("releaseTouchPoint", &EventSenderBindings::ReleaseTouchPoint)
      .SetMethod("updateTouchPoint", &EventSenderBindings::UpdateTouchPoint)
      .SetMethod("cancelTouchPoint", &EventSenderBindings::CancelTouchPoint)
      .SetMethod("setTouchModifier", &EventSenderBindings::SetTouchModifier)
      .SetMethod("setTouchCancelable", &EventSenderBindings::SetTouchCancelable)
      .SetMethod("dumpFilenameBeingDragged",
                 &EventSenderBindings::DumpFilenameBeingDragged)
      .SetMethod("gestureScrollFirstPoint",
                 &EventSenderBindings::GestureScrollFirstPoint)
      .SetMethod("touchStart", &EventSenderBindings::TouchStart)
      .SetMethod("touchMove", &EventSenderBindings::TouchMove)
      .SetMethod("touchCancel", &EventSenderBindings::TouchCancel)
      .SetMethod("touchEnd", &EventSenderBindings::TouchEnd)
      .SetMethod("notifyStartOfTouchScroll",
                 &EventSenderBindings::NotifyStartOfTouchScroll)
      .SetMethod("leapForward", &EventSenderBindings::LeapForward)
      .SetMethod("lastEventTimestamp", &EventSenderBindings::LastEventTimestamp)
      .SetMethod("beginDragWithFiles", &EventSenderBindings::BeginDragWithFiles)
      .SetMethod("beginDragWithStringData",
                 &EventSenderBindings::BeginDragWithStringData)
      .SetMethod("addTouchPoint", &EventSenderBindings::AddTouchPoint)
      .SetMethod("gestureScrollBegin", &EventSenderBindings::GestureScrollBegin)
      .SetMethod("gestureScrollEnd", &EventSenderBindings::GestureScrollEnd)
      .SetMethod("gestureScrollUpdate",
                 &EventSenderBindings::GestureScrollUpdate)
      .SetMethod("gestureTap", &EventSenderBindings::GestureTap)
      .SetMethod("gestureTapDown", &EventSenderBindings::GestureTapDown)
      .SetMethod("gestureShowPress", &EventSenderBindings::GestureShowPress)
      .SetMethod("gestureTapCancel", &EventSenderBindings::GestureTapCancel)
      .SetMethod("gestureLongPress", &EventSenderBindings::GestureLongPress)
      .SetMethod("gestureLongTap", &EventSenderBindings::GestureLongTap)
      .SetMethod("gestureTwoFingerTap",
                 &EventSenderBindings::GestureTwoFingerTap)
      .SetMethod("continuousMouseScrollBy",
                 &EventSenderBindings::ContinuousMouseScrollBy)
      .SetMethod("keyDown", &EventSenderBindings::KeyDown)
      .SetMethod("mouseDown", &EventSenderBindings::MouseDown)
      .SetMethod("mouseMoveTo", &EventSenderBindings::MouseMoveTo)
      .SetMethod("mouseLeave", &EventSenderBindings::MouseLeave)
      .SetMethod("mouseScrollBy", &EventSenderBindings::MouseScrollBy)
      .SetMethod("mouseUp", &EventSenderBindings::MouseUp)
      .SetMethod("setMouseButtonState",
                 &EventSenderBindings::SetMouseButtonState)
      .SetMethod("scheduleAsynchronousClick",
                 &EventSenderBindings::ScheduleAsynchronousClick)
      .SetMethod("scheduleAsynchronousKeyDown",
                 &EventSenderBindings::ScheduleAsynchronousKeyDown)
      .SetMethod("consumeUserActivation",
                 &EventSenderBindings::ConsumeUserActivation)
      .SetProperty("forceLayoutOnEvents",
                   &EventSenderBindings::ForceLayoutOnEvents,
                   &EventSenderBindings::SetForceLayoutOnEvents)
#if defined(OS_WIN)
      .SetProperty("WM_KEYDOWN", &EventSenderBindings::WmKeyDown,
                   &EventSenderBindings::SetWmKeyDown)
      .SetProperty("WM_KEYUP", &EventSenderBindings::WmKeyUp,
                   &EventSenderBindings::SetWmKeyUp)
      .SetProperty("WM_CHAR", &EventSenderBindings::WmChar,
                   &EventSenderBindings::SetWmChar)
      .SetProperty("WM_DEADCHAR", &EventSenderBindings::WmDeadChar,
                   &EventSenderBindings::SetWmDeadChar)
      .SetProperty("WM_SYSKEYDOWN", &EventSenderBindings::WmSysKeyDown,
                   &EventSenderBindings::SetWmSysKeyDown)
      .SetProperty("WM_SYSKEYUP", &EventSenderBindings::WmSysKeyUp,
                   &EventSenderBindings::SetWmSysKeyUp)
      .SetProperty("WM_SYSCHAR", &EventSenderBindings::WmSysChar,
                   &EventSenderBindings::SetWmSysChar)
      .SetProperty("WM_SYSDEADCHAR", &EventSenderBindings::WmSysDeadChar,
                   &EventSenderBindings::SetWmSysDeadChar)
#endif
      .SetProperty("dragMode", &EventSenderBindings::IsDragMode,
                   &EventSenderBindings::SetIsDragMode);
}

void EventSenderBindings::EnableDOMUIEventLogging() {
  if (sender_)
    sender_->EnableDOMUIEventLogging();
}

void EventSenderBindings::FireKeyboardEventsToElement() {
  if (sender_)
    sender_->FireKeyboardEventsToElement();
}

void EventSenderBindings::ClearKillRing() {
  if (sender_)
    sender_->ClearKillRing();
}

std::vector<std::string> EventSenderBindings::ContextClick() {
  if (sender_)
    return sender_->ContextClick();
  return std::vector<std::string>();
}

void EventSenderBindings::TextZoomIn() {
  if (sender_)
    sender_->TextZoomIn();
}

void EventSenderBindings::TextZoomOut() {
  if (sender_)
    sender_->TextZoomOut();
}

void EventSenderBindings::ZoomPageIn() {
  if (sender_)
    sender_->ZoomPageIn();
}

void EventSenderBindings::ZoomPageOut() {
  if (sender_)
    sender_->ZoomPageOut();
}

void EventSenderBindings::SetPageZoomFactor(double factor) {
  if (sender_)
    sender_->SetPageZoomFactor(factor);
}

void EventSenderBindings::ClearTouchPoints() {
  if (sender_)
    sender_->ClearTouchPoints();
}

void EventSenderBindings::ReleaseTouchPoint(unsigned index) {
  if (sender_)
    sender_->ReleaseTouchPoint(index);
}

void EventSenderBindings::UpdateTouchPoint(unsigned index,
                                           double x,
                                           double y,
                                           gin::Arguments* args) {
  if (sender_) {
    sender_->UpdateTouchPoint(index, static_cast<float>(x),
                              static_cast<float>(y), args);
  }
}

void EventSenderBindings::CancelTouchPoint(unsigned index) {
  if (sender_)
    sender_->CancelTouchPoint(index);
}

void EventSenderBindings::SetTouchModifier(const std::string& key_name,
                                           bool set_mask) {
  if (sender_)
    sender_->SetTouchModifier(key_name, set_mask);
}

void EventSenderBindings::SetTouchCancelable(bool cancelable) {
  if (sender_)
    sender_->SetTouchCancelable(cancelable);
}

void EventSenderBindings::DumpFilenameBeingDragged() {
  if (sender_)
    sender_->DumpFilenameBeingDragged();
}

void EventSenderBindings::GestureScrollFirstPoint(float x, float y) {
  if (sender_)
    sender_->GestureScrollFirstPoint(x, y);
}

void EventSenderBindings::TouchStart(gin::Arguments* args) {
  if (sender_)
    sender_->TouchStart(args);
}

void EventSenderBindings::TouchMove(gin::Arguments* args) {
  if (sender_)
    sender_->TouchMove(args);
}

void EventSenderBindings::TouchCancel(gin::Arguments* args) {
  if (sender_)
    sender_->TouchCancel(args);
}

void EventSenderBindings::TouchEnd(gin::Arguments* args) {
  if (sender_)
    sender_->TouchEnd(args);
}

void EventSenderBindings::NotifyStartOfTouchScroll() {
  if (sender_)
    sender_->NotifyStartOfTouchScroll();
}

void EventSenderBindings::LeapForward(int milliseconds) {
  if (sender_)
    sender_->LeapForward(milliseconds);
}

double EventSenderBindings::LastEventTimestamp() {
  if (sender_)
    return sender_->last_event_timestamp().since_origin().InSecondsF();
  return 0;
}

void EventSenderBindings::BeginDragWithFiles(
    const std::vector<std::string>& files) {
  if (sender_)
    sender_->BeginDragWithFiles(files);
}

void EventSenderBindings::BeginDragWithStringData(
    const std::string& data,
    const std::string& mime_type) {
  if (sender_)
    sender_->BeginDragWithStringData(data, mime_type);
}

void EventSenderBindings::AddTouchPoint(double x,
                                        double y,
                                        gin::Arguments* args) {
  if (sender_)
    sender_->AddTouchPoint(static_cast<float>(x), static_cast<float>(y), args);
}

void EventSenderBindings::GestureScrollBegin(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollBegin(args);
}

void EventSenderBindings::GestureScrollEnd(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollEnd(args);
}

void EventSenderBindings::GestureScrollUpdate(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollUpdate(args);
}

void EventSenderBindings::GestureTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTap(args);
}

void EventSenderBindings::GestureTapDown(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTapDown(args);
}

void EventSenderBindings::GestureShowPress(gin::Arguments* args) {
  if (sender_)
    sender_->GestureShowPress(args);
}

void EventSenderBindings::GestureTapCancel(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTapCancel(args);
}

void EventSenderBindings::GestureLongPress(gin::Arguments* args) {
  if (sender_)
    sender_->GestureLongPress(args);
}

void EventSenderBindings::GestureLongTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureLongTap(args);
}

void EventSenderBindings::GestureTwoFingerTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTwoFingerTap(args);
}

void EventSenderBindings::ContinuousMouseScrollBy(gin::Arguments* args) {
  if (sender_)
    sender_->MouseScrollBy(args, EventSender::MouseScrollType::PIXEL);
}

void EventSenderBindings::MouseMoveTo(gin::Arguments* args) {
  if (sender_)
    sender_->MouseMoveTo(args);
}

void EventSenderBindings::MouseLeave(gin::Arguments* args) {
  if (!sender_)
    return;

  WebPointerProperties::PointerType pointerType =
      WebPointerProperties::PointerType::kMouse;
  int pointerId = kRawMousePointerId;

  // Only allow pen or mouse through this API.
  if (!GetPointerType(args, false, pointerType))
    return;
  if (!args->PeekNext().IsEmpty()) {
    if (!args->GetNext(&pointerId)) {
      args->ThrowError();
      return;
    }
  }
  sender_->MouseLeave(pointerType, pointerId);
}

void EventSenderBindings::MouseScrollBy(gin::Arguments* args) {
  if (sender_)
    sender_->MouseScrollBy(args, EventSender::MouseScrollType::TICK);
}

void EventSenderBindings::ScheduleAsynchronousClick(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number = 0;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    args->GetNext(&button_number);
    if (!args->PeekNext().IsEmpty())
      modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
  }
  sender_->ScheduleAsynchronousClick(button_number, modifiers);
}

void EventSenderBindings::ScheduleAsynchronousKeyDown(gin::Arguments* args) {
  if (!sender_)
    return;

  std::string code_str;
  int modifiers = 0;
  int location = DOMKeyLocationStandard;
  args->GetNext(&code_str);
  if (!args->PeekNext().IsEmpty()) {
    v8::Local<v8::Value> value;
    args->GetNext(&value);
    modifiers = GetKeyModifiersFromV8(args->isolate(), value);
    if (!args->PeekNext().IsEmpty())
      args->GetNext(&location);
  }
  sender_->ScheduleAsynchronousKeyDown(code_str, modifiers,
                                       static_cast<KeyLocationCode>(location));
}

void EventSenderBindings::ConsumeUserActivation() {
  if (sender_)
    sender_->ConsumeUserActivation();
}

void EventSenderBindings::MouseDown(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number = 0;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    if (!args->GetNext(&button_number)) {
      args->ThrowError();
      return;
    }
    if (!args->PeekNext().IsEmpty()) {
      modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
      args->Skip();
    }
  }

  WebPointerProperties::PointerType pointerType =
      WebPointerProperties::PointerType::kMouse;
  int pointerId = 0;
  float pressure = 0;
  int tiltX = 0;
  int tiltY = 0;
  if (!getMousePenPointerProperties(args, pointerType, pointerId, pressure,
                                    tiltX, tiltY))
    return;

  sender_->PointerDown(button_number, modifiers, pointerType, pointerId,
                       pressure, tiltX, tiltY);
}

void EventSenderBindings::MouseUp(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number = 0;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    if (!args->GetNext(&button_number)) {
      args->ThrowError();
      return;
    }
    if (!args->PeekNext().IsEmpty()) {
      modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
      args->Skip();
    }
  }

  WebPointerProperties::PointerType pointerType =
      WebPointerProperties::PointerType::kMouse;
  int pointerId = 0;
  float pressure = 0;
  int tiltX = 0;
  int tiltY = 0;
  if (!getMousePenPointerProperties(args, pointerType, pointerId, pressure,
                                    tiltX, tiltY))
    return;

  sender_->PointerUp(button_number, modifiers, pointerType, pointerId, pressure,
                     tiltX, tiltY);
}

void EventSenderBindings::SetMouseButtonState(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number;
  if (!args->GetNext(&button_number)) {
    args->ThrowError();
    return;
  }

  int modifiers = -1;  // Default to the modifier implied by button_number
  if (!args->PeekNext().IsEmpty()) {
    modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
  }

  sender_->SetMouseButtonState(button_number, modifiers);
}

void EventSenderBindings::KeyDown(gin::Arguments* args) {
  if (!sender_)
    return;

  std::string code_str;
  int modifiers = 0;
  int location = DOMKeyLocationStandard;
  args->GetNext(&code_str);
  if (!args->PeekNext().IsEmpty()) {
    v8::Local<v8::Value> value;
    args->GetNext(&value);
    modifiers = GetKeyModifiersFromV8(args->isolate(), value);
    if (!args->PeekNext().IsEmpty())
      args->GetNext(&location);
  }
  sender_->KeyDown(code_str, modifiers, static_cast<KeyLocationCode>(location));
}

bool EventSenderBindings::ForceLayoutOnEvents() const {
  if (sender_)
    return sender_->force_layout_on_events();
  return false;
}

void EventSenderBindings::SetForceLayoutOnEvents(bool force) {
  if (sender_)
    sender_->set_force_layout_on_events(force);
}

bool EventSenderBindings::IsDragMode() const {
  if (sender_)
    return sender_->is_drag_mode();
  return true;
}

void EventSenderBindings::SetIsDragMode(bool drag_mode) {
  if (sender_)
    sender_->set_is_drag_mode(drag_mode);
}

#if defined(OS_WIN)
int EventSenderBindings::WmKeyDown() const {
  if (sender_)
    return sender_->wm_key_down();
  return 0;
}

void EventSenderBindings::SetWmKeyDown(int key_down) {
  if (sender_)
    sender_->set_wm_key_down(key_down);
}

int EventSenderBindings::WmKeyUp() const {
  if (sender_)
    return sender_->wm_key_up();
  return 0;
}

void EventSenderBindings::SetWmKeyUp(int key_up) {
  if (sender_)
    sender_->set_wm_key_up(key_up);
}

int EventSenderBindings::WmChar() const {
  if (sender_)
    return sender_->wm_char();
  return 0;
}

void EventSenderBindings::SetWmChar(int wm_char) {
  if (sender_)
    sender_->set_wm_char(wm_char);
}

int EventSenderBindings::WmDeadChar() const {
  if (sender_)
    return sender_->wm_dead_char();
  return 0;
}

void EventSenderBindings::SetWmDeadChar(int dead_char) {
  if (sender_)
    sender_->set_wm_dead_char(dead_char);
}

int EventSenderBindings::WmSysKeyDown() const {
  if (sender_)
    return sender_->wm_sys_key_down();
  return 0;
}

void EventSenderBindings::SetWmSysKeyDown(int key_down) {
  if (sender_)
    sender_->set_wm_sys_key_down(key_down);
}

int EventSenderBindings::WmSysKeyUp() const {
  if (sender_)
    return sender_->wm_sys_key_up();
  return 0;
}

void EventSenderBindings::SetWmSysKeyUp(int key_up) {
  if (sender_)
    sender_->set_wm_sys_key_up(key_up);
}

int EventSenderBindings::WmSysChar() const {
  if (sender_)
    return sender_->wm_sys_char();
  return 0;
}

void EventSenderBindings::SetWmSysChar(int sys_char) {
  if (sender_)
    sender_->set_wm_sys_char(sys_char);
}

int EventSenderBindings::WmSysDeadChar() const {
  if (sender_)
    return sender_->wm_sys_dead_char();
  return 0;
}

void EventSenderBindings::SetWmSysDeadChar(int sys_dead_char) {
  if (sender_)
    sender_->set_wm_sys_dead_char(sys_dead_char);
}
#endif

// EventSender -----------------------------------------------------------------

WebMouseEvent::Button EventSender::last_button_type_ =
    WebMouseEvent::Button::kNoButton;

EventSender::SavedEvent::SavedEvent()
    : type(TYPE_UNSPECIFIED),
      button_type(WebMouseEvent::Button::kNoButton),
      milliseconds(0),
      modifiers(0) {}

EventSender::EventSender(WebWidgetTestProxy* web_widget_test_proxy)
    : web_widget_test_proxy_(web_widget_test_proxy),
      replaying_saved_events_(false) {
  Reset();
}

EventSender::~EventSender() {}

void EventSender::Reset() {
  DCHECK(current_drag_data_.IsNull());
  current_drag_data_.Reset();
  current_drag_effect_ = blink::kWebDragOperationNone;
  current_drag_effects_allowed_ = blink::kWebDragOperationNone;
  if (widget() && current_pointer_state_[kRawMousePointerId].pressed_button_ !=
                      WebMouseEvent::Button::kNoButton)
    widget()->MouseCaptureLost();
  current_pointer_state_.clear();
  is_drag_mode_ = true;
  force_layout_on_events_ = true;

  // Disable the zoom level override. Reset() also happens during creation of
  // the RenderWidget, which we can detect by checking for the WebWidget.
  if (web_widget_test_proxy_->GetWebWidget())
    web_widget_test_proxy_->ResetZoomLevelForTesting();

#if defined(OS_WIN)
  wm_key_down_ = WM_KEYDOWN;
  wm_key_up_ = WM_KEYUP;
  wm_char_ = WM_CHAR;
  wm_dead_char_ = WM_DEADCHAR;
  wm_sys_key_down_ = WM_SYSKEYDOWN;
  wm_sys_key_up_ = WM_SYSKEYUP;
  wm_sys_char_ = WM_SYSCHAR;
  wm_sys_dead_char_ = WM_SYSDEADCHAR;
#endif

  last_click_time_ = base::TimeTicks();
  last_click_pos_ = WebPoint(0, 0);
  last_button_type_ = WebMouseEvent::Button::kNoButton;
  touch_points_.clear();
  last_context_menu_data_.reset();
  weak_factory_.InvalidateWeakPtrs();
  current_gesture_location_ = WebFloatPoint(0, 0);
  mouse_event_queue_.clear();

  time_offset_ = base::TimeDelta();
  click_count_ = 0;

  touch_modifiers_ = 0;
  touch_cancelable_ = true;
  touch_points_.clear();
}

void EventSender::Install(WebLocalFrame* frame) {
  EventSenderBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

void EventSender::SetContextMenuData(const WebContextMenuData& data) {
  last_context_menu_data_.reset(new WebContextMenuData(data));
}

int EventSender::ModifiersForPointer(int pointer_id) {
  return modifiersWithButtons(
      current_pointer_state_[pointer_id].modifiers_,
      current_pointer_state_[pointer_id].current_buttons_);
}

void EventSender::DoDragDrop(const WebDragData& drag_data,
                             WebDragOperationsMask mask) {
  if (!mainFrameWidget())
    return;

  WebMouseEvent raw_event(WebInputEvent::kMouseDown,
                          ModifiersForPointer(kRawMousePointerId),
                          GetCurrentEventTime());
  InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &raw_event);

  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebMouseEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;

  current_drag_data_ = drag_data;
  current_drag_effects_allowed_ = mask;
  current_drag_effect_ = mainFrameWidget()->DragTargetDragEnter(
      drag_data, event->PositionInWidget(), event->PositionInScreen(),
      current_drag_effects_allowed_,
      modifiersWithButtons(
          current_pointer_state_[kRawMousePointerId].modifiers_,
          current_pointer_state_[kRawMousePointerId].current_buttons_));

  // Finish processing events.
  ReplaySavedEvents();
}

void EventSender::MouseDown(int button_number, int modifiers) {
  PointerDown(button_number, modifiers,
              WebPointerProperties::PointerType::kMouse, kRawMousePointerId,
              0.0, 0, 0);
}

void EventSender::MouseUp(int button_number, int modifiers) {
  PointerUp(button_number, modifiers, WebPointerProperties::PointerType::kMouse,
            kRawMousePointerId, 0.0, 0, 0);
}

void EventSender::PointerDown(int button_number,
                              int modifiers,
                              WebPointerProperties::PointerType pointerType,
                              int pointerId,
                              float pressure,
                              int tiltX,
                              int tiltY) {
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();
  DCHECK_NE(-1, button_number);

  WebMouseEvent::Button button_type =
      GetButtonTypeFromButtonNumber(button_number);

  int click_count = 0;
  current_pointer_state_[pointerId].pressed_button_ = button_type;
  current_pointer_state_[pointerId].current_buttons_ |=
      GetWebMouseEventModifierForButton(button_type);
  current_pointer_state_[pointerId].modifiers_ = modifiers;

  if (pointerType == WebPointerProperties::PointerType::kMouse) {
    UpdateClickCountForButton(button_type);
    click_count = click_count_;
  }
  WebMouseEvent event(WebInputEvent::kMouseDown, ModifiersForPointer(pointerId),
                      GetCurrentEventTime());
  InitMouseEventGeneric(current_pointer_state_[pointerId].pressed_button_,
                        current_pointer_state_[pointerId].current_buttons_,
                        current_pointer_state_[pointerId].last_pos_,
                        click_count, pointerType, pointerId, pressure, tiltX,
                        tiltY, &event);

  HandleInputEventOnViewOrPopup(event);
}

void EventSender::PointerUp(int button_number,
                            int modifiers,
                            WebPointerProperties::PointerType pointerType,
                            int pointerId,
                            float pressure,
                            int tiltX,
                            int tiltY) {
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  DCHECK_NE(-1, button_number);

  WebMouseEvent::Button button_type =
      GetButtonTypeFromButtonNumber(button_number);

  if (pointerType == WebPointerProperties::PointerType::kMouse &&
      is_drag_mode_ && !replaying_saved_events_) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::TYPE_MOUSE_UP;
    saved_event.button_type = button_type;
    saved_event.modifiers = modifiers;
    mouse_event_queue_.push_back(saved_event);
    ReplaySavedEvents();
  } else {
    current_pointer_state_[pointerId].modifiers_ = modifiers;
    current_pointer_state_[pointerId].current_buttons_ &=
        ~GetWebMouseEventModifierForButton(button_type);
    current_pointer_state_[pointerId].pressed_button_ =
        WebMouseEvent::Button::kNoButton;

    WebMouseEvent event(WebInputEvent::kMouseUp, ModifiersForPointer(pointerId),
                        GetCurrentEventTime());
    int click_count = pointerType == WebPointerProperties::PointerType::kMouse
                          ? click_count_
                          : 0;
    InitMouseEventGeneric(
        button_type, current_pointer_state_[pointerId].current_buttons_,
        current_pointer_state_[pointerId].last_pos_, click_count, pointerType,
        pointerId, pressure, tiltX, tiltY, &event);
    HandleInputEventOnViewOrPopup(event);
    if (pointerType == WebPointerProperties::PointerType::kMouse)
      DoDragAfterMouseUp(event);
  }
}

void EventSender::SetMouseButtonState(int button_number, int modifiers) {
  current_pointer_state_[kRawMousePointerId].pressed_button_ =
      GetButtonTypeFromButtonNumber(button_number);
  current_pointer_state_[kRawMousePointerId].current_buttons_ =
      (modifiers == -1)
          ? GetWebMouseEventModifierForButton(
                current_pointer_state_[kRawMousePointerId].pressed_button_)
          : modifiers & kButtonsInModifiers;
}

void EventSender::KeyDown(const std::string& code_str,
                          int modifiers,
                          KeyLocationCode location) {
  // FIXME: I'm not exactly sure how we should convert the string to a key
  // event. This seems to work in the cases I tested.
  // FIXME: Should we also generate a KEY_UP?

  bool generate_char = false;

  // Convert \n -> VK_RETURN. Some web tests use \n to mean "Enter", when
  // Windows uses \r for "Enter".
  int code = 0;
  int text = 0;
  bool needs_shift_key_modifier = false;
  std::string domKeyString;
  std::string domCodeString;

  if ("Enter" == code_str) {
    generate_char = true;
    text = code = ui::VKEY_RETURN;
    domKeyString.assign("Enter");
    domCodeString.assign("Enter");
  } else if ("ArrowRight" == code_str) {
    code = ui::VKEY_RIGHT;
    domKeyString.assign("ArrowRight");
    domCodeString.assign("ArrowRight");
  } else if ("ArrowDown" == code_str) {
    code = ui::VKEY_DOWN;
    domKeyString.assign("ArrowDown");
    domCodeString.assign("ArrowDown");
  } else if ("ArrowLeft" == code_str) {
    code = ui::VKEY_LEFT;
    domKeyString.assign("ArrowLeft");
    domCodeString.assign("ArrowLeft");
  } else if ("ArrowUp" == code_str) {
    code = ui::VKEY_UP;
    domKeyString.assign("ArrowUp");
    domCodeString.assign("ArrowUp");
  } else if ("Insert" == code_str) {
    code = ui::VKEY_INSERT;
    domKeyString.assign("Insert");
    domCodeString.assign("Insert");
  } else if ("Delete" == code_str) {
    code = ui::VKEY_DELETE;
    domKeyString.assign("Delete");
    domCodeString.assign("Delete");
  } else if ("PageUp" == code_str) {
    code = ui::VKEY_PRIOR;
    domKeyString.assign("PageUp");
    domCodeString.assign("PageUp");
  } else if ("PageDown" == code_str) {
    code = ui::VKEY_NEXT;
    domKeyString.assign("PageDown");
    domCodeString.assign("PageDown");
  } else if ("Home" == code_str) {
    code = ui::VKEY_HOME;
    domKeyString.assign("Home");
    domCodeString.assign("Home");
  } else if ("End" == code_str) {
    code = ui::VKEY_END;
    domKeyString.assign("End");
    domCodeString.assign("End");
  } else if ("PrintScreen" == code_str) {
    code = ui::VKEY_SNAPSHOT;
    domKeyString.assign("PrintScreen");
    domCodeString.assign("PrintScreen");
  } else if ("ContextMenu" == code_str) {
    code = ui::VKEY_APPS;
    domKeyString.assign("ContextMenu");
    domCodeString.assign("ContextMenu");
  } else if ("ControlLeft" == code_str) {
    code = ui::VKEY_CONTROL;
    domKeyString.assign("Control");
    domCodeString.assign("ControlLeft");
    location = DOMKeyLocationLeft;
  } else if ("ControlRight" == code_str) {
    code = ui::VKEY_CONTROL;
    domKeyString.assign("Control");
    domCodeString.assign("ControlRight");
    location = DOMKeyLocationRight;
  } else if ("ShiftLeft" == code_str) {
    code = ui::VKEY_SHIFT;
    domKeyString.assign("Shift");
    domCodeString.assign("ShiftLeft");
    location = DOMKeyLocationLeft;
  } else if ("ShiftRight" == code_str) {
    code = ui::VKEY_SHIFT;
    domKeyString.assign("Shift");
    domCodeString.assign("ShiftRight");
    location = DOMKeyLocationRight;
  } else if ("AltLeft" == code_str) {
    code = ui::VKEY_MENU;
    domKeyString.assign("Alt");
    domCodeString.assign("AltLeft");
    location = DOMKeyLocationLeft;
  } else if ("AltRight" == code_str) {
    code = ui::VKEY_MENU;
    domKeyString.assign("Alt");
    domCodeString.assign("AltRight");
    location = DOMKeyLocationRight;
  } else if ("NumLock" == code_str) {
    code = ui::VKEY_NUMLOCK;
    domKeyString.assign("NumLock");
    domCodeString.assign("NumLock");
  } else if ("Backspace" == code_str) {
    code = ui::VKEY_BACK;
    domKeyString.assign("Backspace");
    domCodeString.assign("Backspace");
  } else if ("Escape" == code_str) {
    code = ui::VKEY_ESCAPE;
    domKeyString.assign("Escape");
    domCodeString.assign("Escape");
  } else if ("Tab" == code_str) {
    code = ui::VKEY_TAB;
    domKeyString.assign("Tab");
    domCodeString.assign("Tab");
  } else if ("Cut" == code_str || "Copy" == code_str || "Paste" == code_str) {
    // No valid KeyboardCode for Cut/Copy/Paste.
    code = 0;
    domKeyString.assign(code_str);
    // It's OK to assign the same string as the DomCode strings happens to be
    // the same for these keys.
    domCodeString.assign(code_str);
  } else {
    // Compare the input string with the function-key names defined by the
    // DOM spec (i.e. "F1",...,"F24"). If the input string is a function-key
    // name, set its key code.
    for (int i = 1; i <= 24; ++i) {
      std::string function_key_name = base::StringPrintf("F%d", i);
      if (function_key_name == code_str) {
        code = ui::VKEY_F1 + (i - 1);
        domKeyString = function_key_name;
        domCodeString = function_key_name;
        break;
      }
    }
    if (!code) {
      base::string16 code_str16 = base::UTF8ToUTF16(code_str);
      if (code_str16.size() != 1u) {
        v8::Isolate* isolate = blink::MainThreadIsolate();
        isolate->ThrowException(v8::Exception::TypeError(
            gin::StringToV8(isolate, "Invalid web code.")));
        return;
      }
      text = code = code_str16[0];
      needs_shift_key_modifier = base::IsAsciiUpper(code & 0xFF);
      if (base::IsAsciiLower(code & 0xFF))
        code -= 'a' - 'A';
      if (base::IsAsciiAlpha(code)) {
        domKeyString.assign(code_str);
        domCodeString.assign("Key");
        domCodeString.push_back(
            base::ToUpperASCII(static_cast<base::char16>(code)));
      } else if (base::IsAsciiDigit(code)) {
        domKeyString.assign(code_str);
        domCodeString.assign("Digit");
        domCodeString.push_back(code);
      } else if (code == ' ') {
        domKeyString.assign(code_str);
        domCodeString.assign("Space");
      } else if (code == 9) {
        domKeyString.assign("Tab");
        domCodeString.assign("Tab");
      }
      generate_char = true;
    }

    if ("(" == code_str) {
      code = '9';
      needs_shift_key_modifier = true;
      domKeyString.assign("(");
      domCodeString.assign("Digit9");
    }
  }

  if (needs_shift_key_modifier)
    modifiers |= WebInputEvent::kShiftKey;

  // See if KeyLocation argument is given.
  switch (location) {
    case DOMKeyLocationStandard:
      break;
    case DOMKeyLocationLeft:
      modifiers |= WebInputEvent::kIsLeft;
      break;
    case DOMKeyLocationRight:
      modifiers |= WebInputEvent::kIsRight;
      break;
    case DOMKeyLocationNumpad:
      modifiers |= WebInputEvent::kIsKeyPad;
      break;
  }

  // For one generated keyboard event, we need to generate a keyDown/keyUp
  // pair;
  // On Windows, we might also need to generate a char event to mimic the
  // Windows event flow; on other platforms we create a merged event and test
  // the event flow that that platform provides.
  WebKeyboardEvent event_down(WebInputEvent::kRawKeyDown, modifiers,
                              GetCurrentEventTime());
  event_down.windows_key_code = code;
  event_down.dom_key =
      static_cast<int>(ui::KeycodeConverter::KeyStringToDomKey(domKeyString));
  event_down.dom_code = static_cast<int>(
      ui::KeycodeConverter::CodeStringToDomCode(domCodeString));

  if (generate_char) {
    event_down.text[0] = text;
    event_down.unmodified_text[0] = text;
  }

  if (event_down.GetModifiers() != 0)
    event_down.is_system_key = IsSystemKeyEvent(event_down);

  WebKeyboardEvent event_up = event_down;
  event_up.SetType(WebInputEvent::kKeyUp);
  // EventSender.m forces a layout here, with at least one
  // test (fast/forms/focus-control-to-page.html) relying on this.
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  // In the browser, if a keyboard event corresponds to an editor command,
  // the command will be dispatched to the renderer just before dispatching
  // the keyboard event, and then it will be executed in the
  // RenderView::handleCurrentKeyboardEvent() method.
  // We just simulate the same behavior here.
  std::string edit_command;
  if (GetEditCommand(event_down, &edit_command))
    delegate()->SetEditCommand(edit_command, "");

  HandleInputEventOnViewOrPopup(event_down);

  if (code == ui::VKEY_ESCAPE && !current_drag_data_.IsNull()) {
    WebMouseEvent event(WebInputEvent::kMouseDown,
                        ModifiersForPointer(kRawMousePointerId),
                        GetCurrentEventTime());
    InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                   current_pointer_state_[kRawMousePointerId].current_buttons_,
                   current_pointer_state_[kRawMousePointerId].last_pos_,
                   click_count_, &event);
    FinishDragAndDrop(event, blink::kWebDragOperationNone);
  }

  delegate()->ClearEditCommand();

  if (generate_char) {
    WebKeyboardEvent event_char = event_up;
    event_char.SetType(WebInputEvent::kChar);
    HandleInputEventOnViewOrPopup(event_char);
  }

  HandleInputEventOnViewOrPopup(event_up);
}

void EventSender::EnableDOMUIEventLogging() {}

void EventSender::FireKeyboardEventsToElement() {}

void EventSender::ClearKillRing() {}

std::vector<std::string> EventSender::ContextClick() {
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  UpdateClickCountForButton(WebMouseEvent::Button::kRight);

  // Clears last context menu data because we need to know if the context menu
  // be requested after following mouse events.
  last_context_menu_data_.reset();

  // Generate right mouse down and up.
  // This is a hack to work around only allowing a single pressed button since
  // we want to test the case where both the left and right mouse buttons are
  // pressed.
  // TODO(mustaq): This hack seems unused here! But do we need this hack at all
  //   after adding current_buttons_.
  if (current_pointer_state_[kRawMousePointerId].pressed_button_ ==
      WebMouseEvent::Button::kNoButton) {
    current_pointer_state_[kRawMousePointerId].pressed_button_ =
        WebMouseEvent::Button::kRight;
    current_pointer_state_[kRawMousePointerId].current_buttons_ |=
        GetWebMouseEventModifierForButton(
            current_pointer_state_[kRawMousePointerId].pressed_button_);
  }
  WebMouseEvent event(WebInputEvent::kMouseDown,
                      ModifiersForPointer(kRawMousePointerId),
                      GetCurrentEventTime());
  InitMouseEvent(WebMouseEvent::Button::kRight,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &event);
  HandleInputEventOnViewOrPopup(event);

#if defined(OS_WIN)
  current_pointer_state_[kRawMousePointerId].current_buttons_ &=
      ~GetWebMouseEventModifierForButton(WebMouseEvent::Button::kRight);
  current_pointer_state_[kRawMousePointerId].pressed_button_ =
      WebMouseEvent::Button::kNoButton;

  WebMouseEvent mouseUpEvent(WebInputEvent::kMouseUp,
                             ModifiersForPointer(kRawMousePointerId),
                             GetCurrentEventTime());
  InitMouseEvent(WebMouseEvent::Button::kRight,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &mouseUpEvent);
  HandleInputEventOnViewOrPopup(mouseUpEvent);
#endif

  std::vector<std::string> menu_items =
      MakeMenuItemStringsFor(last_context_menu_data_.get(), delegate());
  last_context_menu_data_.reset();
  return menu_items;
}

void EventSender::TextZoomIn() {
  view()->SetTextZoomFactor(view()->TextZoomFactor() * 1.2f);
}

void EventSender::TextZoomOut() {
  view()->SetTextZoomFactor(view()->TextZoomFactor() / 1.2f);
}

void EventSender::ZoomPageIn() {
  for (WebViewTestProxy* view_proxy : interfaces()->GetWindowList()) {
    // Only set page zoom on main frames. Any RenderViews that exist for
    // a proxy main frame will hear about the change as a side effect of
    // changing the main frame.
    if (view_proxy->GetMainRenderFrame()) {
      view_proxy->GetWidget()->SetZoomLevelForTesting(
          view_proxy->webview()->ZoomLevel() + 1);
    }
  }
}

void EventSender::ZoomPageOut() {
  for (WebViewTestProxy* view_proxy : interfaces()->GetWindowList()) {
    // Only set page zoom on main frames. Any RenderViews that exist for
    // a proxy main frame will hear about the change as a side effect of
    // changing the main frame.
    if (view_proxy->GetMainRenderFrame()) {
      view_proxy->GetWidget()->SetZoomLevelForTesting(
          view_proxy->webview()->ZoomLevel() - 1);
    }
  }
}

void EventSender::SetPageZoomFactor(double zoom_factor) {
  for (WebViewTestProxy* view_proxy : interfaces()->GetWindowList()) {
    // Only set page zoom on main frames. Any RenderViews that exist for
    // a proxy main frame will hear about the change as a side effect of
    // changing the main frame.
    if (view_proxy->GetMainRenderFrame()) {
      view_proxy->GetWidget()->SetZoomLevelForTesting(std::log(zoom_factor) /
                                                      std::log(1.2));
    }
  }
}

void EventSender::ClearTouchPoints() {
  touch_points_.clear();
}

void EventSender::ThrowTouchPointError() {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  isolate->ThrowException(v8::Exception::TypeError(
      gin::StringToV8(isolate, "Invalid touch point.")));
}

void EventSender::ReleaseTouchPoint(unsigned index) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::kStateReleased;
}

void EventSender::UpdateTouchPoint(unsigned index,
                                   float x,
                                   float y,
                                   gin::Arguments* args) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::kStateMoved;
  touch_point->SetPositionInWidget(x, y);
  touch_point->SetPositionInScreen(x, y);

  InitPointerProperties(args, touch_point, &touch_point->radius_x,
                        &touch_point->radius_y);
}

void EventSender::CancelTouchPoint(unsigned index) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::kStateCancelled;
}

void EventSender::SetTouchModifier(const std::string& key_name, bool set_mask) {
  int mask = GetKeyModifier(key_name);

  if (set_mask)
    touch_modifiers_ |= mask;
  else
    touch_modifiers_ &= ~mask;
}

void EventSender::SetTouchCancelable(bool cancelable) {
  touch_cancelable_ = cancelable;
}

void EventSender::DumpFilenameBeingDragged() {
  if (current_drag_data_.IsNull())
    return;

  WebVector<WebDragData::Item> items = current_drag_data_.Items();
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].storage_type == WebDragData::Item::kStorageTypeBinaryData) {
      WebURL url = items[i].binary_data_source_url;
      WebString filename_extension = items[i].binary_data_filename_extension;
      WebString content_disposition = items[i].binary_data_content_disposition;
      base::FilePath filename =
          net::GenerateFileName(url, content_disposition.Utf8(),
                                std::string(),   // referrer_charset
                                std::string(),   // suggested_name
                                std::string(),   // mime_type
                                std::string());  // default_name
#if defined(OS_WIN)
      filename = filename.ReplaceExtension(filename_extension.Utf16());
#else
      filename = filename.ReplaceExtension(filename_extension.Utf8());
#endif
      delegate()->PrintMessage(std::string("Filename being dragged: ") +
                               filename.AsUTF8Unsafe() + "\n");
      return;
    }
  }
}

void EventSender::GestureScrollFirstPoint(float x, float y) {
  current_gesture_location_ = WebFloatPoint(x, y);
}

void EventSender::TouchStart(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::kTouchStart, args);
}

void EventSender::TouchMove(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::kTouchMove, args);
}

void EventSender::TouchCancel(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::kTouchCancel, args);
}

void EventSender::TouchEnd(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::kTouchEnd, args);
}

void EventSender::NotifyStartOfTouchScroll() {
  WebPointerEvent event = WebPointerEvent::CreatePointerCausesUaActionEvent(
      WebPointerProperties::PointerType::kUnknown, GetCurrentEventTime());
  HandleInputEventOnViewOrPopup(event);
}

void EventSender::LeapForward(int milliseconds) {
  if (is_drag_mode_ &&
      current_pointer_state_[kRawMousePointerId].pressed_button_ ==
          WebMouseEvent::Button::kLeft &&
      !replaying_saved_events_) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::TYPE_LEAP_FORWARD;
    saved_event.milliseconds = milliseconds;
    mouse_event_queue_.push_back(saved_event);
  } else {
    DoLeapForward(milliseconds);
  }
}

void EventSender::BeginDragWithItems(
    const WebVector<WebDragData::Item>& items) {
  if (!current_drag_data_.IsNull()) {
    // Nested dragging not supported, fuzzer code a likely culprit.
    // Cancel the current drag operation and throw an error.
    KeyDown("Escape", 0, DOMKeyLocationStandard);
    v8::Isolate* isolate = blink::MainThreadIsolate();
    isolate->ThrowException(v8::Exception::Error(gin::StringToV8(
        isolate,
        "Nested beginDragWithFiles/beginDragWithStringData() not supported.")));
    return;
  }

  current_drag_data_.Initialize();
  WebVector<WebString> absolute_filenames;
  for (size_t i = 0; i < items.size(); ++i) {
    current_drag_data_.AddItem(items[i]);
    if (items[i].storage_type == WebDragData::Item::kStorageTypeFilename)
      absolute_filenames.emplace_back(items[i].filename_data);
  }
  if (!absolute_filenames.empty()) {
    current_drag_data_.SetFilesystemId(
        delegate()->RegisterIsolatedFileSystem(absolute_filenames));
  }
  current_drag_effects_allowed_ = blink::kWebDragOperationCopy;

  const WebPoint& last_pos =
      current_pointer_state_[kRawMousePointerId].last_pos_;

  // Compute the scale from window (dsf-independent) to blink (dsf-dependent
  // under UseZoomForDSF).
  blink::WebFloatRect rect(0, 0, 1.0f, 0.0);
  web_widget_test_proxy_->ConvertWindowToViewport(&rect);
  float scale_to_blink_coords = rect.width;

  WebFloatPoint last_pos_for_blink(last_pos.x * scale_to_blink_coords,
                                   last_pos.y * scale_to_blink_coords);

  // Provide a drag source.
  mainFrameWidget()->DragTargetDragEnter(current_drag_data_, last_pos_for_blink,
                                         last_pos_for_blink,
                                         current_drag_effects_allowed_, 0);
  // |is_drag_mode_| saves events and then replays them later. We don't
  // need/want that.
  is_drag_mode_ = false;

  // Make the rest of eventSender think a drag is in progress.
  current_pointer_state_[kRawMousePointerId].pressed_button_ =
      WebMouseEvent::Button::kLeft;
  current_pointer_state_[kRawMousePointerId].current_buttons_ |=
      GetWebMouseEventModifierForButton(
          current_pointer_state_[kRawMousePointerId].pressed_button_);
}

void EventSender::BeginDragWithFiles(const std::vector<std::string>& files) {
  WebVector<WebDragData::Item> items;
  for (size_t i = 0; i < files.size(); ++i) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeFilename;
    item.filename_data = delegate()->GetAbsoluteWebStringFromUTF8Path(files[i]);
    items.emplace_back(item);
  }

  BeginDragWithItems(items);
}

void EventSender::BeginDragWithStringData(const std::string& data,
                                          const std::string& mime_type) {
  WebVector<WebDragData::Item> items;
  WebDragData::Item item;
  item.storage_type = WebDragData::Item::kStorageTypeString;
  item.string_data = WebString::FromUTF8(data);
  item.string_type = WebString::FromUTF8(mime_type);
  items.emplace_back(item);

  BeginDragWithItems(items);
}

void EventSender::AddTouchPoint(float x, float y, gin::Arguments* args) {
  if (touch_points_.size() == WebTouchEvent::kTouchesLengthCap) {
    args->ThrowError();
    return;
  }
  WebTouchPoint touch_point;
  touch_point.pointer_type = WebPointerProperties::PointerType::kTouch;
  touch_point.state = WebTouchPoint::kStatePressed;
  touch_point.SetPositionInWidget(x, y);
  touch_point.SetPositionInScreen(x, y);

  int highest_id = -1;
  for (size_t i = 0; i < touch_points_.size(); i++) {
    if (touch_points_[i].id > highest_id)
      highest_id = touch_points_[i].id;
  }
  touch_point.id = highest_id + 1;

  InitPointerProperties(args, &touch_point, &touch_point.radius_x,
                        &touch_point.radius_y);

  // Set the touch point pressure to zero if it was not set by the caller
  if (std::isnan(touch_point.force))
    touch_point.force = 0.0;

  touch_points_.push_back(touch_point);
}

void EventSender::GestureScrollBegin(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureScrollBegin, args);
}

void EventSender::GestureScrollEnd(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureScrollEnd, args);
}

void EventSender::GestureScrollUpdate(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureScrollUpdate, args);
}

void EventSender::GestureTap(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureTap, args);
}

void EventSender::GestureTapDown(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureTapDown, args);
}

void EventSender::GestureShowPress(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureShowPress, args);
}

void EventSender::GestureTapCancel(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureTapCancel, args);
}

void EventSender::GestureLongPress(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureLongPress, args);
}

void EventSender::GestureLongTap(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureLongTap, args);
}

void EventSender::GestureTwoFingerTap(gin::Arguments* args) {
  GestureEvent(WebInputEvent::kGestureTwoFingerTap, args);
}

void EventSender::MouseScrollBy(gin::Arguments* args,
                                MouseScrollType scroll_type) {
  // TODO(dtapuska): Gestures really should be sent by the MouseWheelEventQueue
  // class in the browser. But since the event doesn't propogate up into
  // the browser generate the events here. See crbug.com/596095.
  bool send_gestures = true;
  WebMouseWheelEvent wheel_event =
      GetMouseWheelEvent(args, scroll_type, &send_gestures);
  if (wheel_event.GetType() != WebInputEvent::kUndefined &&
      HandleInputEventOnViewOrPopup(wheel_event) ==
          WebInputEventResult::kNotHandled &&
      send_gestures) {
    SendGesturesForMouseWheelEvent(wheel_event);
  }
}

void EventSender::MouseMoveTo(gin::Arguments* args) {
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  double x;
  double y;
  if (!args->GetNext(&x) || !args->GetNext(&y)) {
    args->ThrowError();
    return;
  }
  WebPoint mouse_pos(static_cast<int>(x), static_cast<int>(y));

  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
    args->Skip();
  }

  WebPointerProperties::PointerType pointerType =
      WebPointerProperties::PointerType::kMouse;
  int pointerId = 0;
  float pressure = 0;
  int tiltX = 0;
  int tiltY = 0;
  if (!getMousePenPointerProperties(args, pointerType, pointerId, pressure,
                                    tiltX, tiltY))
    return;

  if (pointerType == WebPointerProperties::PointerType::kMouse &&
      is_drag_mode_ && !replaying_saved_events_ &&
      current_pointer_state_[kRawMousePointerId].pressed_button_ ==
          WebMouseEvent::Button::kLeft) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::TYPE_MOUSE_MOVE;
    saved_event.pos = mouse_pos;
    saved_event.modifiers = modifiers;
    mouse_event_queue_.push_back(saved_event);
  } else {
    current_pointer_state_[pointerId].last_pos_ = mouse_pos;
    current_pointer_state_[pointerId].modifiers_ = modifiers;
    WebMouseEvent event(WebInputEvent::kMouseMove,
                        ModifiersForPointer(pointerId), GetCurrentEventTime());
    int click_count = pointerType == WebPointerProperties::PointerType::kMouse
                          ? click_count_
                          : 0;
    InitMouseEventGeneric(
        current_pointer_state_[kRawMousePointerId].pressed_button_,
        current_pointer_state_[kRawMousePointerId].current_buttons_, mouse_pos,
        click_count, pointerType, pointerId, pressure, tiltX, tiltY, &event);
    HandleInputEventOnViewOrPopup(event);
    if (pointerType == WebPointerProperties::PointerType::kMouse)
      DoDragAfterMouseMove(event);
  }
}

void EventSender::MouseLeave(
    blink::WebPointerProperties::PointerType pointerType,
    int pointerId) {
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  WebMouseEvent event(WebInputEvent::kMouseLeave,
                      ModifiersForPointer(pointerId), GetCurrentEventTime());
  InitMouseEventGeneric(WebMouseEvent::Button::kNoButton, 0,
                        current_pointer_state_[kRawMousePointerId].last_pos_,
                        click_count_, pointerType, pointerId, 0.0, 0, 0,
                        &event);
  HandleInputEventOnViewOrPopup(event);
}

void EventSender::ScheduleAsynchronousClick(int button_number, int modifiers) {
  delegate()->PostTask(base::BindOnce(&EventSender::MouseDown,
                                      weak_factory_.GetWeakPtr(), button_number,
                                      modifiers));
  delegate()->PostTask(base::BindOnce(&EventSender::MouseUp,
                                      weak_factory_.GetWeakPtr(), button_number,
                                      modifiers));
}

void EventSender::ScheduleAsynchronousKeyDown(const std::string& code_str,
                                              int modifiers,
                                              KeyLocationCode location) {
  delegate()->PostTask(base::BindOnce(&EventSender::KeyDown,
                                      weak_factory_.GetWeakPtr(), code_str,
                                      modifiers, location));
}

void EventSender::ConsumeUserActivation() {
  blink::WebUserGestureIndicator::ConsumeUserGesture(
      view()->MainFrame()->ToWebLocalFrame());
}

base::TimeTicks EventSender::GetCurrentEventTime() const {
  return base::TimeTicks::Now() + time_offset_;
}

void EventSender::DoLeapForward(int milliseconds) {
  time_offset_ += base::TimeDelta::FromMilliseconds(milliseconds);
}

uint32_t EventSender::GetUniqueTouchEventId(gin::Arguments* args) {
  uint32_t unique_touch_event_id;
  if (!args->PeekNext().IsEmpty() && args->GetNext(&unique_touch_event_id))
    return unique_touch_event_id;

  return 0;
}

void EventSender::SendCurrentTouchEvent(WebInputEvent::Type type,
                                        gin::Arguments* args) {
  uint32_t unique_touch_event_id = GetUniqueTouchEventId(args);

  DCHECK_LE(touch_points_.size(),
            static_cast<unsigned>(WebTouchEvent::kTouchesLengthCap));
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  base::TimeTicks time_stamp = GetCurrentEventTime();
  blink::WebInputEvent::DispatchType dispatch_type =
      touch_cancelable_ ? WebInputEvent::kBlocking
                        : WebInputEvent::kEventNonBlocking;

  for (unsigned i = 0; i < touch_points_.size(); ++i) {
    const WebTouchPoint& touch_point = touch_points_[i];
    if (touch_point.state != blink::WebTouchPoint::kStateStationary) {
      WebPointerEvent pointer_event = WebPointerEvent(
          PointerEventTypeForTouchPointState(touch_point.state), touch_point,
          touch_point.radius_x * 2, touch_point.radius_y * 2);
      pointer_event.hovering = false;
      pointer_event.dispatch_type = dispatch_type;
      pointer_event.moved_beyond_slop_region = true;
      pointer_event.unique_touch_event_id = unique_touch_event_id;
      pointer_event.SetTimeStamp(time_stamp);
      pointer_event.SetModifiers(touch_modifiers_);
      pointer_event.button =
          (pointer_event.GetType() == WebInputEvent::kPointerDown ||
           pointer_event.GetType() == WebInputEvent::kPointerUp)
              ? WebPointerProperties::Button::kLeft
              : WebPointerProperties::Button::kNoButton;

      HandleInputEventOnViewOrPopup(pointer_event);
    }
  }
  WebPagePopup* popup = view()->GetPagePopup();
  if (popup)
    popup->DispatchBufferedTouchEvents();
  else
    widget()->DispatchBufferedTouchEvents();

  for (size_t i = 0; i < touch_points_.size(); ++i) {
    WebTouchPoint* touch_point = &touch_points_[i];
    if (touch_point->state == WebTouchPoint::kStateReleased ||
        touch_point->state == WebTouchPoint::kStateCancelled) {
      touch_points_.erase(touch_points_.begin() + i);
      --i;
    } else {
      touch_point->state = WebTouchPoint::kStateStationary;
    }
  }
}

void EventSender::GestureEvent(WebInputEvent::Type type, gin::Arguments* args) {
  WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                        GetCurrentEventTime(),
                        blink::WebGestureDevice::kTouchscreen);

  // If the first argument is a string, it is to specify the device, otherwise
  // the device is assumed to be a touchscreen (since most tests were written
  // assuming this).
  if (!args->PeekNext().IsEmpty() && args->PeekNext()->IsString()) {
    std::string device_string;
    if (!args->GetNext(&device_string)) {
      args->ThrowError();
      return;
    }
    if (device_string == kSourceDeviceStringTouchpad) {
      event.SetSourceDevice(blink::WebGestureDevice::kTouchpad);
    } else if (device_string == kSourceDeviceStringTouchscreen) {
      event.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
    } else {
      args->ThrowError();
      return;
    }
  }

  double x;
  double y;
  if (!args->GetNext(&x) || !args->GetNext(&y)) {
    args->ThrowError();
    return;
  }

  switch (type) {
    case WebInputEvent::kGestureScrollUpdate: {
      if (!GetScrollUnits(args, &event.data.scroll_update.delta_units))
        return;

      event.data.scroll_update.delta_x = static_cast<float>(x);
      event.data.scroll_update.delta_y = static_cast<float>(y);
      event.SetPositionInWidget(current_gesture_location_);
      current_gesture_location_.x =
          current_gesture_location_.x + event.data.scroll_update.delta_x;
      current_gesture_location_.y =
          current_gesture_location_.y + event.data.scroll_update.delta_y;
      break;
    }
    case WebInputEvent::kGestureScrollBegin:
      current_gesture_location_ = WebFloatPoint(x, y);
      event.SetPositionInWidget(current_gesture_location_);
      break;
    case WebInputEvent::kGestureScrollEnd:
    case WebInputEvent::kGestureFlingStart:
      event.SetPositionInWidget(current_gesture_location_);
      break;
    case WebInputEvent::kGestureTap: {
      float tap_count = 1;
      float width = 30;
      float height = 30;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&tap_count)) {
          args->ThrowError();
          return;
        }
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&height)) {
          args->ThrowError();
          return;
        }
      }
      event.data.tap.tap_count = tap_count;
      event.data.tap.width = width;
      event.data.tap.height = height;
      event.SetPositionInWidget(WebFloatPoint(x, y));
      break;
    }
    case WebInputEvent::kGestureTapUnconfirmed:
      if (!args->PeekNext().IsEmpty()) {
        float tap_count;
        if (!args->GetNext(&tap_count)) {
          args->ThrowError();
          return;
        }
        event.data.tap.tap_count = tap_count;
      } else {
        event.data.tap.tap_count = 1;
      }
      event.SetPositionInWidget(WebFloatPoint(x, y));
      break;
    case WebInputEvent::kGestureTapDown: {
      float width = 30;
      float height = 30;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&height)) {
          args->ThrowError();
          return;
        }
      }
      event.SetPositionInWidget(WebFloatPoint(x, y));
      event.data.tap_down.width = width;
      event.data.tap_down.height = height;
      break;
    }
    case WebInputEvent::kGestureShowPress: {
      float width = 30;
      float height = 30;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
        if (!args->PeekNext().IsEmpty()) {
          if (!args->GetNext(&height)) {
            args->ThrowError();
            return;
          }
        }
      }
      event.SetPositionInWidget(WebFloatPoint(x, y));
      event.data.show_press.width = width;
      event.data.show_press.height = height;
      break;
    }
    case WebInputEvent::kGestureTapCancel:
      event.SetPositionInWidget(WebFloatPoint(x, y));
      break;
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureLongTap:
      event.SetPositionInWidget(WebFloatPoint(x, y));
      if (!args->PeekNext().IsEmpty()) {
        float width;
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
        event.data.long_press.width = width;
        if (!args->PeekNext().IsEmpty()) {
          float height;
          if (!args->GetNext(&height)) {
            args->ThrowError();
            return;
          }
          event.data.long_press.height = height;
        }
      }
      break;
    case WebInputEvent::kGestureTwoFingerTap:
      event.SetPositionInWidget(WebFloatPoint(x, y));
      if (!args->PeekNext().IsEmpty()) {
        float first_finger_width;
        if (!args->GetNext(&first_finger_width)) {
          args->ThrowError();
          return;
        }
        event.data.two_finger_tap.first_finger_width = first_finger_width;
        if (!args->PeekNext().IsEmpty()) {
          float first_finger_height;
          if (!args->GetNext(&first_finger_height)) {
            args->ThrowError();
            return;
          }
          event.data.two_finger_tap.first_finger_height = first_finger_height;
        }
      }
      break;
    default:
      NOTREACHED();
  }

  event.unique_touch_event_id = GetUniqueTouchEventId(args);
  if (!GetPointerType(args, false, event.primary_pointer_type))
    return;

  event.SetPositionInScreen(event.PositionInWidget());

  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  WebInputEventResult result = HandleInputEventOnViewOrPopup(event);

  // Long press might start a drag drop session. Complete it if so.
  if (type == WebInputEvent::kGestureLongPress &&
      !current_drag_data_.IsNull()) {
    WebMouseEvent mouse_event(WebInputEvent::kMouseDown,
                              ModifiersForPointer(kRawMousePointerId),
                              GetCurrentEventTime());

    InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                   current_pointer_state_[kRawMousePointerId].current_buttons_,
                   WebPoint(x, y), click_count_, &mouse_event);

    FinishDragAndDrop(mouse_event, blink::kWebDragOperationNone);
  }
  args->Return(result != WebInputEventResult::kNotHandled);
}

void EventSender::UpdateClickCountForButton(WebMouseEvent::Button button_type) {
  if ((GetCurrentEventTime() - last_click_time_ < kMultipleClickTime) &&
      (!OutsideMultiClickRadius(
          current_pointer_state_[kRawMousePointerId].last_pos_,
          last_click_pos_)) &&
      (button_type == last_button_type_)) {
    ++click_count_;
  } else {
    click_count_ = 1;
    last_button_type_ = button_type;
  }
}

WebMouseWheelEvent EventSender::GetMouseWheelEvent(gin::Arguments* args,
                                                   MouseScrollType scroll_type,
                                                   bool* send_gestures) {
  // Force a layout here just to make sure every position has been
  // determined before we send events (as well as all the other methods
  // that send an event do).
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  double horizontal;
  double vertical;
  if (!args->GetNext(&horizontal) || !args->GetNext(&vertical)) {
    args->ThrowError();
    return WebMouseWheelEvent();
  }

  bool paged = false;
  bool has_precise_scrolling_deltas = false;
  int modifiers = 0;
  WebMouseWheelEvent::Phase phase = WebMouseWheelEvent::kPhaseNone;
  if (!args->PeekNext().IsEmpty()) {
    args->GetNext(&paged);
    if (!args->PeekNext().IsEmpty()) {
      args->GetNext(&has_precise_scrolling_deltas);
      if (!args->PeekNext().IsEmpty()) {
        v8::Local<v8::Value> value;
        args->GetNext(&value);
        modifiers = GetKeyModifiersFromV8(args->isolate(), value);
        if (!args->PeekNext().IsEmpty()) {
          args->GetNext(send_gestures);
          if (!args->PeekNext().IsEmpty()) {
            v8::Local<v8::Value> phase_value;
            args->GetNext(&phase_value);
            phase = GetMouseWheelEventPhaseFromV8(args->isolate(), phase_value);
          }
        }
      }
    }
  }

  current_pointer_state_[kRawMousePointerId].modifiers_ = modifiers;
  WebMouseWheelEvent event(WebInputEvent::kMouseWheel,
                           ModifiersForPointer(kRawMousePointerId),
                           GetCurrentEventTime());
  InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &event);
  event.wheel_ticks_x = static_cast<float>(horizontal);
  event.wheel_ticks_y = static_cast<float>(vertical);
  event.delta_x = event.wheel_ticks_x;
  event.delta_y = event.wheel_ticks_y;
  if (paged) {
    event.delta_units = ui::input_types::ScrollGranularity::kScrollByPage;
  } else if (has_precise_scrolling_deltas) {
    event.delta_units =
        ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
  } else {
    event.delta_units = ui::input_types::ScrollGranularity::kScrollByPixel;
  }
  event.phase = phase;
  if (scroll_type == MouseScrollType::PIXEL) {
    event.wheel_ticks_x /= kScrollbarPixelsPerTick;
    event.wheel_ticks_y /= kScrollbarPixelsPerTick;
  } else {
    event.delta_x *= kScrollbarPixelsPerTick;
    event.delta_y *= kScrollbarPixelsPerTick;
  }
  event.event_action = content::WebMouseWheelEventTraits::GetEventAction(event);
  return event;
}

// Radius fields radius_x and radius_y should eventually be moved to
// WebPointerProperties.
// TODO(e_hakkinen): Drop radius_{x,y}_pointer parameters once that happens.
void EventSender::InitPointerProperties(gin::Arguments* args,
                                        WebPointerProperties* e,
                                        float* radius_x_pointer,
                                        float* radius_y_pointer) {
  if (!args->PeekNext().IsEmpty()) {
    double radius_x;
    if (!args->GetNext(&radius_x)) {
      args->ThrowError();
      return;
    }

    double radius_y = radius_x;
    if (!args->PeekNext().IsEmpty()) {
      if (!args->GetNext(&radius_y)) {
        args->ThrowError();
        return;
      }
    }

    *radius_x_pointer = static_cast<float>(radius_x);
    *radius_y_pointer = static_cast<float>(radius_y);
  }

  if (!args->PeekNext().IsEmpty()) {
    double force;
    if (!args->GetNext(&force)) {
      args->ThrowError();
      return;
    }
    e->force = static_cast<float>(force);
  }

  if (!args->PeekNext().IsEmpty()) {
    int tiltX, tiltY;
    if (!args->GetNext(&tiltX) || !args->GetNext(&tiltY)) {
      args->ThrowError();
      return;
    }
    e->tilt_x = tiltX;
    e->tilt_y = tiltY;
  }

  if (!GetPointerType(args, false, e->pointer_type))
    return;
}

void EventSender::FinishDragAndDrop(const WebMouseEvent& raw_event,
                                    blink::WebDragOperation drag_effect) {
  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebMouseEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;

  current_drag_effect_ = drag_effect;
  if (current_drag_effect_) {
    // Specifically pass any keyboard modifiers to the drop method. This allows
    // tests to control the drop type (i.e. copy or move).
    mainFrameWidget()->DragTargetDrop(
        current_drag_data_, event->PositionInWidget(),
        event->PositionInScreen(), event->GetModifiers());
  } else {
    mainFrameWidget()->DragTargetDragLeave(blink::WebFloatPoint(),
                                           blink::WebFloatPoint());
  }
  current_drag_data_.Reset();
  mainFrameWidget()->DragSourceEndedAt(event->PositionInWidget(),
                                       event->PositionInScreen(),
                                       current_drag_effect_);
  mainFrameWidget()->DragSourceSystemDragEnded();
}

void EventSender::DoDragAfterMouseUp(const WebMouseEvent& raw_event) {
  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebMouseEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;

  last_click_time_ = event->TimeStamp();
  last_click_pos_ = current_pointer_state_[kRawMousePointerId].last_pos_;

  // If we're in a drag operation, complete it.
  if (current_drag_data_.IsNull())
    return;

  blink::WebDragOperation drag_effect = mainFrameWidget()->DragTargetDragOver(
      event->PositionInWidget(), event->PositionInScreen(),
      current_drag_effects_allowed_, event->GetModifiers());

  // Bail if dragover caused cancellation.
  if (current_drag_data_.IsNull())
    return;

  FinishDragAndDrop(raw_event, drag_effect);
}

void EventSender::DoDragAfterMouseMove(const WebMouseEvent& raw_event) {
  if (current_pointer_state_[kRawMousePointerId].pressed_button_ ==
          WebMouseEvent::Button::kNoButton ||
      current_drag_data_.IsNull()) {
    return;
  }

  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebMouseEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;

  current_drag_effect_ = mainFrameWidget()->DragTargetDragOver(
      event->PositionInWidget(), event->PositionInScreen(),
      current_drag_effects_allowed_, event->GetModifiers());
}

void EventSender::ReplaySavedEvents() {
  replaying_saved_events_ = true;
  while (!mouse_event_queue_.empty()) {
    SavedEvent e = mouse_event_queue_.front();
    mouse_event_queue_.pop_front();

    switch (e.type) {
      case SavedEvent::TYPE_MOUSE_MOVE: {
        current_pointer_state_[kRawMousePointerId].modifiers_ = e.modifiers;
        WebMouseEvent event(WebInputEvent::kMouseMove,
                            ModifiersForPointer(kRawMousePointerId),
                            GetCurrentEventTime());
        InitMouseEvent(
            current_pointer_state_[kRawMousePointerId].pressed_button_,
            current_pointer_state_[kRawMousePointerId].current_buttons_, e.pos,
            click_count_, &event);
        current_pointer_state_[kRawMousePointerId].last_pos_ =
            WebPoint(event.PositionInWidget().x, event.PositionInWidget().y);
        HandleInputEventOnViewOrPopup(event);
        DoDragAfterMouseMove(event);
        break;
      }
      case SavedEvent::TYPE_LEAP_FORWARD:
        DoLeapForward(e.milliseconds);
        break;
      case SavedEvent::TYPE_MOUSE_UP: {
        current_pointer_state_[kRawMousePointerId].current_buttons_ &=
            ~GetWebMouseEventModifierForButton(e.button_type);
        current_pointer_state_[kRawMousePointerId].pressed_button_ =
            WebMouseEvent::Button::kNoButton;
        current_pointer_state_[kRawMousePointerId].modifiers_ = e.modifiers;

        WebMouseEvent event(WebInputEvent::kMouseUp,
                            ModifiersForPointer(kRawMousePointerId),
                            GetCurrentEventTime());
        InitMouseEvent(
            e.button_type,
            current_pointer_state_[kRawMousePointerId].current_buttons_,
            current_pointer_state_[kRawMousePointerId].last_pos_, click_count_,
            &event);
        HandleInputEventOnViewOrPopup(event);
        DoDragAfterMouseUp(event);
        break;
      }
      default:
        NOTREACHED();
    }
  }

  replaying_saved_events_ = false;
}

WebInputEventResult EventSender::HandleInputEventOnViewOrPopup(
    const WebInputEvent& raw_event) {
  last_event_timestamp_ = raw_event.TimeStamp();

  WebPagePopup* popup = view()->GetPagePopup();
  if (popup && !WebInputEvent::IsKeyboardEventType(raw_event.GetType())) {
    // Compute the scale from window (dsf-independent) to blink (dsf-dependent
    // under UseZoomForDSF).
    blink::WebFloatRect rect(0, 0, 1.0f, 0.0);
    web_widget_test_proxy_->ConvertWindowToViewport(&rect);
    float scale_to_blink_coords = rect.width;

    // ui::ScaleWebInputEvent returns nullptr when the scale is 1.0f as the
    // event does not have to be converted.
    std::unique_ptr<WebInputEvent> scaled_event =
        ui::ScaleWebInputEvent(raw_event, scale_to_blink_coords);
    const WebInputEvent* popup_friendly_event =
        scaled_event.get() ? scaled_event.get() : &raw_event;
    return popup->HandleInputEvent(
        blink::WebCoalescedInputEvent(*popup_friendly_event));
  }

  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebInputEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;
  return widget()->HandleInputEvent(blink::WebCoalescedInputEvent(*event));
}

void EventSender::SendGesturesForMouseWheelEvent(
    const WebMouseWheelEvent wheel_event) {
  WebGestureEvent begin_event(WebInputEvent::kGestureScrollBegin,
                              wheel_event.GetModifiers(), GetCurrentEventTime(),
                              blink::WebGestureDevice::kTouchpad);
  InitGestureEventFromMouseWheel(wheel_event, &begin_event);
  begin_event.data.scroll_begin.delta_x_hint = wheel_event.delta_x;
  begin_event.data.scroll_begin.delta_y_hint = wheel_event.delta_y;
  begin_event.data.scroll_begin.delta_hint_units = wheel_event.delta_units;
  if (wheel_event.delta_units ==
      ui::input_types::ScrollGranularity::kScrollByPage) {
    if (begin_event.data.scroll_begin.delta_x_hint) {
      begin_event.data.scroll_begin.delta_x_hint =
          begin_event.data.scroll_begin.delta_x_hint > 0 ? 1 : -1;
    }
    if (begin_event.data.scroll_begin.delta_y_hint) {
      begin_event.data.scroll_begin.delta_y_hint =
          begin_event.data.scroll_begin.delta_y_hint > 0 ? 1 : -1;
    }
  }

  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  HandleInputEventOnViewOrPopup(begin_event);

  WebGestureEvent update_event(
      WebInputEvent::kGestureScrollUpdate, wheel_event.GetModifiers(),
      GetCurrentEventTime(), blink::WebGestureDevice::kTouchpad);
  InitGestureEventFromMouseWheel(wheel_event, &update_event);
  update_event.data.scroll_update.delta_x =
      begin_event.data.scroll_begin.delta_x_hint;
  update_event.data.scroll_update.delta_y =
      begin_event.data.scroll_begin.delta_y_hint;
  update_event.data.scroll_update.delta_units =
      begin_event.data.scroll_begin.delta_hint_units;

  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();
  HandleInputEventOnViewOrPopup(update_event);

  WebGestureEvent end_event(WebInputEvent::kGestureScrollEnd,
                            wheel_event.GetModifiers(), GetCurrentEventTime(),
                            blink::WebGestureDevice::kTouchpad);
  InitGestureEventFromMouseWheel(wheel_event, &end_event);
  end_event.data.scroll_end.delta_units =
      begin_event.data.scroll_begin.delta_hint_units;

  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();
  HandleInputEventOnViewOrPopup(end_event);
}

TestInterfaces* EventSender::interfaces() {
  return web_widget_test_proxy_->GetWebViewTestProxy()->test_interfaces();
}

WebTestDelegate* EventSender::delegate() {
  return web_widget_test_proxy_->GetWebViewTestProxy()->delegate();
}

const blink::WebView* EventSender::view() const {
  return web_widget_test_proxy_->GetWebViewTestProxy()->webview();
}

blink::WebView* EventSender::view() {
  return web_widget_test_proxy_->GetWebViewTestProxy()->webview();
}

blink::WebWidget* EventSender::widget() {
  return web_widget_test_proxy_->GetWebWidget();
}

blink::WebFrameWidget* EventSender::mainFrameWidget() {
  if (!view() || !view()->MainFrame())
    return nullptr;
  DCHECK(view()->MainFrame()->IsWebLocalFrame())
      << "Event Sender doesn't support being run in a remote frame for this "
         "operation.";
  return view()->MainFrame()->ToWebLocalFrame()->FrameWidget();
}

std::unique_ptr<WebInputEvent> EventSender::TransformScreenToWidgetCoordinates(
    const WebInputEvent& event) {
  return delegate()->TransformScreenToWidgetCoordinates(web_widget_test_proxy_,
                                                        event);
}

void EventSender::UpdateLifecycleToPrePaint() {
  widget()->UpdateLifecycle(blink::WebWidget::LifecycleUpdate::kPrePaint,
                            blink::WebWidget::LifecycleUpdateReason::kTest);
}

}  // namespace test_runner
