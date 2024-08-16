// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/web_test/renderer/event_sender.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/render_frame_impl.h"
#include "content/web_test/renderer/test_runner.h"
#include "content/web_test/renderer/web_frame_test_proxy.h"
#include "content/web_test/renderer/web_test_spell_checker.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "net/base/filename_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "v8/include/v8.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

using blink::ContextMenuData;
using blink::DragOperationsMask;
using blink::MenuItemInfo;
using blink::WebDragData;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebKeyboardEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPagePopup;
using blink::WebPointerEvent;
using blink::WebPointerProperties;
using blink::WebString;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebURL;
using blink::WebVector;
using blink::WebView;

namespace content {

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
    case WebTouchPoint::State::kStateReleased:
      return WebInputEvent::Type::kPointerUp;
    case WebTouchPoint::State::kStateCancelled:
      return WebInputEvent::Type::kPointerCancel;
    case WebTouchPoint::State::kStatePressed:
      return WebInputEvent::Type::kPointerDown;
    case WebTouchPoint::State::kStateMoved:
      return WebInputEvent::Type::kPointerMove;
    case WebTouchPoint::State::kStateStationary:
    default:
      NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
  return 0;
}

const int kButtonsInModifiers =
    WebMouseEvent::kLeftButtonDown | WebMouseEvent::kMiddleButtonDown |
    WebMouseEvent::kRightButtonDown | WebMouseEvent::kBackButtonDown |
    WebMouseEvent::kForwardButtonDown;

int ModifiersWithButtons(int modifiers, int buttons) {
  return (modifiers & ~kButtonsInModifiers) | (buttons & kButtonsInModifiers);
}

void InitMouseEventGeneric(WebMouseEvent::Button b,
                           int current_buttons,
                           const gfx::PointF& pos,
                           int click_count,
                           WebPointerProperties::PointerType pointerType,
                           int pointerId,
                           float pressure,
                           int tiltX,
                           int tiltY,
                           WebMouseEvent* e) {
  e->button = b;
  e->SetPositionInWidget(pos);
  e->SetPositionInScreen(pos);
  e->pointer_type = pointerType;
  e->id = pointerId;
  e->force = pressure;
  e->tilt_x = tiltX;
  e->tilt_y = tiltY;
  e->click_count = click_count;
}

void InitMouseEvent(WebMouseEvent::Button b,
                    int current_buttons,
                    const gfx::PointF& pos,
                    int click_count,
                    WebMouseEvent* e) {
  InitMouseEventGeneric(b, current_buttons, pos, click_count,
                        WebPointerProperties::PointerType::kMouse, 0, 0.0, 0, 0,
                        e);
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
constexpr base::TimeDelta kMultipleClickTime = base::Seconds(1);
const int kMultipleClickRadiusPixels = 5;
const char kSubMenuDepthIdentifier[] = "_";
const char kSubMenuIdentifier[] = " >";
const char kSeparatorIdentifier[] = "---------";
const char kDisabledIdentifier[] = "#";
const char kCheckedIdentifier[] = "*";

bool OutsideRadius(const gfx::PointF& a, const gfx::PointF& b, float radius) {
  return ((a.x() - b.x()) * (a.x() - b.x()) +
          (a.y() - b.y()) * (a.y() - b.y())) > radius * radius;
}

void PopulateCustomItems(const WebVector<MenuItemInfo>& customItems,
                         const std::string& prefix,
                         std::vector<std::string>* strings) {
  for (size_t i = 0; i < customItems.size(); ++i) {
    std::string prefixCopy = prefix;
    if (!customItems[i].enabled)
      prefixCopy = kDisabledIdentifier + prefix;
    if (customItems[i].checked)
      prefixCopy = kCheckedIdentifier + prefix;
    if (customItems[i].type == blink::MenuItemInfo::kSeparator) {
      strings->push_back(prefixCopy + kSeparatorIdentifier);
    } else if (customItems[i].type == blink::MenuItemInfo::kSubMenu) {
      strings->push_back(prefixCopy + base::UTF16ToUTF8(customItems[i].label) +
                         kSubMenuIdentifier);
      PopulateCustomItems(customItems[i].sub_menu_items,
                          prefixCopy + kSubMenuDepthIdentifier, strings);
    } else {
      strings->push_back(prefixCopy + base::UTF16ToUTF8(customItems[i].label));
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
std::vector<std::string> MakeMenuItemStringsFor(ContextMenuData* context_menu) {
  // These constants are based on Safari's context menu because tests are made
  // for it.
  static const char* kNonEditableMenuStrings[] = {
      "Back",        "Reload Page",     "Open in Dashboard",
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
    WebTestSpellChecker::FillSuggestionList(
        WebString::FromUTF16(context_menu->misspelled_word), &suggestions);
    for (const WebString& suggestion : suggestions)
      strings.push_back(suggestion.Utf8());
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
#if BUILDFLAG(IS_MAC)
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
#if BUILDFLAG(IS_MAC)
  return event.GetModifiers() & WebInputEvent::kMetaKey &&
         event.windows_key_code != ui::VKEY_B &&
         event.windows_key_code != ui::VKEY_I;
#else
  return !!(event.GetModifiers() & WebInputEvent::kAltKey);
#endif
}

const char* kSourceDeviceStringTouchpad = "touchpad";
const char* kSourceDeviceStringTouchscreen = "touchscreen";

}  // namespace

class EventSenderBindings : public gin::Wrappable<EventSenderBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  EventSenderBindings(const EventSenderBindings&) = delete;
  EventSenderBindings& operator=(const EventSenderBindings&) = delete;

  static void Install(base::WeakPtr<EventSender> sender,
                      WebFrameTestProxy* frame);

 private:
  // Watches for the RenderFrame that the EventSenderBindings is attached to
  // being destroyed.
  class EventSenderBindingsRenderFrameObserver : public RenderFrameObserver {
   public:
    EventSenderBindingsRenderFrameObserver(EventSenderBindings* bindings,
                                           RenderFrame* frame)
        : RenderFrameObserver(frame), bindings_(bindings) {}

    // RenderFrameObserver implementation.
    void OnDestruct() override { bindings_->OnFrameDestroyed(); }

   private:
    const raw_ptr<EventSenderBindings> bindings_;
  };

  explicit EventSenderBindings(base::WeakPtr<EventSender> sender,
                               WebFrameTestProxy* frame);
  ~EventSenderBindings() override;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Bound methods:
  void EnableDOMUIEventLogging();
  void FireKeyboardEventsToElement();
  void ClearKillRing();
  std::vector<std::string> ContextClick();
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
  void GestureScrollPopup(gin::Arguments* args);
  void GestureTap(gin::Arguments* args);
  void GestureTapDown(gin::Arguments* args);
  void GestureShowPress(gin::Arguments* args);
  void GestureTapCancel(gin::Arguments* args);
  void GestureLongPress(gin::Arguments* args);
  void GestureLongTap(gin::Arguments* args);
  void GestureTwoFingerTap(gin::Arguments* args);
  void MouseMoveTo(gin::Arguments* args);
  void MouseLeave(gin::Arguments* args);
  void ScheduleAsynchronousClick(gin::Arguments* args);
  void ScheduleAsynchronousKeyDown(gin::Arguments* args);
  void ConsumeUserActivation();
  void MouseDown(gin::Arguments* args);
  void MouseUp(gin::Arguments* args);
  void SetMouseButtonState(gin::Arguments* args);
  void KeyDown(gin::Arguments* args);
  void KeyDownAsync(gin::Arguments* args);
  void KeyDownOnly(gin::Arguments* args);
  void KeyUp(gin::Arguments* args);

  void KeyEvent(EventSender::KeyEventType event_type,
                gin::Arguments* args,
                bool async);

  // Binding properties:
  bool ForceLayoutOnEvents() const;
  void SetForceLayoutOnEvents(bool force);
  bool IsDragMode() const;
  void SetIsDragMode(bool drag_mode);

#if BUILDFLAG(IS_WIN)
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

  // Is notified when the local root frame the EventSender is attached to is
  // destroyed.
  void OnFrameDestroyed() { sender_ = nullptr; }

  EventSenderBindingsRenderFrameObserver frame_observer_;

  base::WeakPtr<EventSender> sender_;
  const raw_ptr<blink::WebLocalFrame> frame_;
};

gin::WrapperInfo EventSenderBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

EventSenderBindings::EventSenderBindings(base::WeakPtr<EventSender> sender,
                                         WebFrameTestProxy* frame)
    : frame_observer_(this, frame),
      sender_(sender),
      frame_(frame->GetWebFrame()) {}

EventSenderBindings::~EventSenderBindings() = default;

// static
void EventSenderBindings::Install(base::WeakPtr<EventSender> sender,
                                  WebFrameTestProxy* frame) {
  v8::Isolate* isolate =
      frame->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<EventSenderBindings> bindings =
      gin::CreateHandle(isolate, new EventSenderBindings(sender, frame));
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
      .SetMethod("clearTouchPoints", &EventSenderBindings::ClearTouchPoints)
      .SetMethod("releaseTouchPoint", &EventSenderBindings::ReleaseTouchPoint)
      .SetMethod("updateTouchPoint", &EventSenderBindings::UpdateTouchPoint)
      .SetMethod("cancelTouchPoint", &EventSenderBindings::CancelTouchPoint)
      .SetMethod("setTouchModifier", &EventSenderBindings::SetTouchModifier)
      .SetMethod("setTouchCancelable", &EventSenderBindings::SetTouchCancelable)
      .SetMethod("dumpFilenameBeingDragged",
                 &EventSenderBindings::DumpFilenameBeingDragged)
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
      .SetMethod("gestureScrollPopup", &EventSenderBindings::GestureScrollPopup)
      .SetMethod("gestureTap", &EventSenderBindings::GestureTap)
      .SetMethod("gestureTapDown", &EventSenderBindings::GestureTapDown)
      .SetMethod("gestureShowPress", &EventSenderBindings::GestureShowPress)
      .SetMethod("gestureTapCancel", &EventSenderBindings::GestureTapCancel)
      .SetMethod("gestureLongPress", &EventSenderBindings::GestureLongPress)
      .SetMethod("gestureLongTap", &EventSenderBindings::GestureLongTap)
      .SetMethod("gestureTwoFingerTap",
                 &EventSenderBindings::GestureTwoFingerTap)
      .SetMethod("keyDown", &EventSenderBindings::KeyDown)
      .SetMethod("keyDownAsync", &EventSenderBindings::KeyDownAsync)
      .SetMethod("keyDownOnly", &EventSenderBindings::KeyDownOnly)
      .SetMethod("keyUp", &EventSenderBindings::KeyUp)
      .SetMethod("mouseDown", &EventSenderBindings::MouseDown)
      .SetMethod("mouseMoveTo", &EventSenderBindings::MouseMoveTo)
      .SetMethod("mouseLeave", &EventSenderBindings::MouseLeave)
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
#if BUILDFLAG(IS_WIN)
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
    sender_->DumpFilenameBeingDragged(frame_);
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
    sender_->BeginDragWithFiles(frame_, files);
}

void EventSenderBindings::BeginDragWithStringData(
    const std::string& data,
    const std::string& mime_type) {
  if (sender_)
    sender_->BeginDragWithStringData(frame_, data, mime_type);
}

void EventSenderBindings::AddTouchPoint(double x,
                                        double y,
                                        gin::Arguments* args) {
  if (sender_)
    sender_->AddTouchPoint(static_cast<float>(x), static_cast<float>(y), args);
}

void EventSenderBindings::GestureScrollPopup(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollPopup(frame_, args);
}

void EventSenderBindings::GestureTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTap(frame_, args);
}

void EventSenderBindings::GestureTapDown(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTapDown(frame_, args);
}

void EventSenderBindings::GestureShowPress(gin::Arguments* args) {
  if (sender_)
    sender_->GestureShowPress(frame_, args);
}

void EventSenderBindings::GestureTapCancel(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTapCancel(frame_, args);
}

void EventSenderBindings::GestureLongPress(gin::Arguments* args) {
  if (sender_)
    sender_->GestureLongPress(frame_, args);
}

void EventSenderBindings::GestureLongTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureLongTap(frame_, args);
}

void EventSenderBindings::GestureTwoFingerTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTwoFingerTap(frame_, args);
}

void EventSenderBindings::MouseMoveTo(gin::Arguments* args) {
  if (sender_)
    sender_->MouseMoveTo(frame_, args);
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
  sender_->ScheduleAsynchronousClick(frame_, button_number, modifiers);
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
  sender_->ScheduleAsynchronousKeyDown(frame_, code_str, modifiers,
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

// `KeyDown` sends both `KeyDown` and `KeyUp` events. It's similar to `KeyPress`
// in other APIs.
void EventSenderBindings::KeyDown(gin::Arguments* args) {
  KeyEvent(EventSender::kKeyPress, args, /*async=*/false);
}

// `KeyDownAsync` sends both `KeyDown` and `KeyUp` events. It's similar to
// `KeyPress` in other APIs. It sends those events asynchronously, outside of a
// JS task.
void EventSenderBindings::KeyDownAsync(gin::Arguments* args) {
  KeyEvent(EventSender::kKeyPress, args, /*async=*/true);
}

// `KeyDownOnly` sends `KeyDown` without `KeyUp`.
void EventSenderBindings::KeyDownOnly(gin::Arguments* args) {
  KeyEvent(EventSender::kKeyDown, args, /*async=*/false);
}

void EventSenderBindings::KeyUp(gin::Arguments* args) {
  KeyEvent(EventSender::kKeyUp, args, /*async=*/false);
}

void EventSenderBindings::KeyEvent(EventSender::KeyEventType event_type,
                                   gin::Arguments* args,
                                   bool async) {
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
  sender_->KeyEvent(event_type, code_str, modifiers,
                    static_cast<KeyLocationCode>(location), async);
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

#if BUILDFLAG(IS_WIN)
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

EventSender::EventSender(blink::WebFrameWidget* web_frame_widget,
                         content::TestRunner* test_runner)
    : web_frame_widget_(web_frame_widget),
      test_runner_(test_runner) {
  Reset();
}

EventSender::~EventSender() {}

void EventSender::Reset() {
  current_drag_data_ = std::nullopt;
  current_drag_effect_ = ui::mojom::DragOperation::kNone;
  current_drag_effects_allowed_ = blink::kDragOperationNone;
  current_pointer_state_.clear();
  is_drag_mode_ = true;
  force_layout_on_events_ = true;

#if BUILDFLAG(IS_WIN)
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
  last_click_pos_ = gfx::PointF();
  last_button_type_ = WebMouseEvent::Button::kNoButton;
  touch_points_.clear();
  last_context_menu_data_.reset();
  weak_factory_.InvalidateWeakPtrs();
  current_gesture_location_ = gfx::PointF();
  mouse_event_queue_.clear();

  time_offset_ = base::TimeDelta();
  click_count_ = 0;

  touch_modifiers_ = 0;
  touch_cancelable_ = true;
  touch_points_.clear();
}

void EventSender::Install(WebFrameTestProxy* frame) {
  EventSenderBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

void EventSender::SetContextMenuData(const ContextMenuData& data) {
  last_context_menu_data_ = std::make_unique<ContextMenuData>(data);
}

int EventSender::ModifiersForPointer(int pointer_id) {
  return ModifiersWithButtons(
      current_pointer_state_[pointer_id].modifiers_,
      current_pointer_state_[pointer_id].current_buttons_);
}

void EventSender::DoDragDrop(const WebDragData& drag_data,
                             DragOperationsMask mask) {
  if (!MainFrameWidget())
    return;

  WebMouseEvent event(WebInputEvent::Type::kMouseDown,
                      ModifiersForPointer(kRawMousePointerId),
                      GetCurrentEventTime());
  InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &event);

  current_drag_data_ = drag_data;
  current_drag_effects_allowed_ = mask;
  MainFrameWidget()->DragTargetDragEnter(
      drag_data, event.PositionInWidget(), event.PositionInScreen(),
      current_drag_effects_allowed_,
      ModifiersWithButtons(
          current_pointer_state_[kRawMousePointerId].modifiers_,
          current_pointer_state_[kRawMousePointerId].current_buttons_),
      base::BindOnce(
          [](base::WeakPtr<EventSender> sender, ui::mojom::DragOperation op,
             bool document_is_handling_drag) {
            if (sender)
              sender->current_drag_effect_ = op;
          },
          weak_factory_.GetWeakPtr()));

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
  WebMouseEvent event(WebInputEvent::Type::kMouseDown,
                      ModifiersForPointer(pointerId), GetCurrentEventTime());
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

    WebMouseEvent event(WebInputEvent::Type::kMouseUp,
                        ModifiersForPointer(pointerId), GetCurrentEventTime());
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

// `KeyDown` sends both `KeyDown` and `KeyUp` events. It's similar to `KeyPress`
// in other APIs.
void EventSender::KeyDown(const std::string& code_str,
                          int modifiers,
                          KeyLocationCode location) {
  KeyEvent(KeyEventType::kKeyPress, code_str, modifiers, location,
           /*async=*/false);
}

void EventSender::KeyEvent(KeyEventType event_type,
                           const std::string& code_str,
                           int modifiers,
                           KeyLocationCode location,
                           bool async) {
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
      std::u16string code_str16 = base::UTF8ToUTF16(code_str);
      if (code_str16.size() != 1u) {
        v8::Isolate* isolate =
            web_frame_widget_->LocalRoot()->GetAgentGroupScheduler()->Isolate();
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
            base::ToUpperASCII(static_cast<char16_t>(code)));
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

  // Update the currently pressed modifiers if `kKeyDown` or `kKeyUp`.
  switch (event_type) {
    case KeyEventType::kKeyDown:
      // Add the given `modifier` to the `key_modifiers_`. For example:
      // 1. Received `keyDown` of `kControlKey`. Keep it in `key_modifiers_`.
      // 2. Then received `keyDown` of `kShiftKey`. The given `modifier` is
      //    `kShiftKey`, but the current modifier state should become
      //    `kControlKey | kShiftKey`.
      key_modifiers_ |= modifiers;
      // `WebKeyboardEvent` should have all modifiers currently in the down
      // state. For example, if this event is `kShiftKey` while `kControlKey` is
      // currently down, the event should have `kControlKey | kShiftKey`. If
      // this is a non-modifier key (e.g., 'a') with `modifiers == 0` but
      // `kControlKey` is currently down (`key_modifiers_ == kControlKey`), then
      // the event should have `kControlKey`, meaning this is `Ctrl+A`.
      modifiers = key_modifiers_;
      break;
    case KeyEventType::kKeyUp:
      // Remove the released modifiers from the `key_modifiers_`.
      key_modifiers_ &= ~modifiers;
      // See `keyDown` above. For example, if this is `keyUp` for 'a' with no
      // modifiers (`modifiers == 0`) but `kControlKey` is currently down, this
      // should be `Ctrl+A`.
      modifiers |= key_modifiers_;
      break;
    case KeyEventType::kKeyPress:
      break;
  }

  // For one generated keyboard event, we need to generate a keyDown/keyUp
  // pair;
  // On Windows, we might also need to generate a char event to mimic the
  // Windows event flow; on other platforms we create a merged event and test
  // the event flow that that platform provides.
  WebKeyboardEvent event_down(WebInputEvent::Type::kRawKeyDown, modifiers,
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
  event_up.SetType(WebInputEvent::Type::kKeyUp);
  // EventSender.m forces a layout here, with at least one
  // test (fast/forms/focus-control-to-page.html) relying on this.
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  if (event_type & KeyEventType::kKeyDown) {
    // In the browser, if a keyboard event corresponds to an editor command,
    // the command will be dispatched to the renderer just before dispatching
    // the keyboard event, and stored in RenderWidget. We just simulate the same
    // behavior here.
    std::string edit_command;
    if (GetEditCommand(event_down, &edit_command)) {
      web_frame_widget_->AddEditCommandForNextKeyEvent(
          WebString::FromLatin1(edit_command), "");
    }

    HandleInputEventOnViewOrPopup(event_down, async);

    if (code == ui::VKEY_ESCAPE && current_drag_data_) {
      WebMouseEvent event(WebInputEvent::Type::kMouseDown,
                          ModifiersForPointer(kRawMousePointerId),
                          GetCurrentEventTime());
      InitMouseEvent(
          current_pointer_state_[kRawMousePointerId].pressed_button_,
          current_pointer_state_[kRawMousePointerId].current_buttons_,
          current_pointer_state_[kRawMousePointerId].last_pos_, click_count_,
          &event);
      FinishDragAndDrop(event, ui::mojom::DragOperation::kNone, false);
    }

    web_frame_widget_->ClearEditCommands();
  }

  if (event_type & KeyEventType::kKeyUp) {
    if (generate_char) {
      WebKeyboardEvent event_char = event_up;
      event_char.SetType(WebInputEvent::Type::kChar);
      HandleInputEventOnViewOrPopup(event_char, async);
    }

    HandleInputEventOnViewOrPopup(event_up, async);
  }
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
  WebMouseEvent event(WebInputEvent::Type::kMouseDown,
                      ModifiersForPointer(kRawMousePointerId),
                      GetCurrentEventTime());
  InitMouseEvent(WebMouseEvent::Button::kRight,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &event);
  HandleInputEventOnViewOrPopup(event);

#if BUILDFLAG(IS_WIN)
  current_pointer_state_[kRawMousePointerId].current_buttons_ &=
      ~GetWebMouseEventModifierForButton(WebMouseEvent::Button::kRight);
  current_pointer_state_[kRawMousePointerId].pressed_button_ =
      WebMouseEvent::Button::kNoButton;

  WebMouseEvent mouseUpEvent(WebInputEvent::Type::kMouseUp,
                             ModifiersForPointer(kRawMousePointerId),
                             GetCurrentEventTime());
  InitMouseEvent(WebMouseEvent::Button::kRight,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &mouseUpEvent);
  HandleInputEventOnViewOrPopup(mouseUpEvent);
#endif

  std::vector<std::string> menu_items =
      MakeMenuItemStringsFor(last_context_menu_data_.get());
  last_context_menu_data_.reset();
  return menu_items;
}

void EventSender::ClearTouchPoints() {
  touch_points_.clear();
}

void EventSender::ThrowTouchPointError() {
  v8::Isolate* isolate =
      web_frame_widget_->LocalRoot()->GetAgentGroupScheduler()->Isolate();
  isolate->ThrowException(v8::Exception::TypeError(
      gin::StringToV8(isolate, "Invalid touch point.")));
}

void EventSender::ReleaseTouchPoint(unsigned index) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::State::kStateReleased;
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
  touch_point->state = WebTouchPoint::State::kStateMoved;
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
  touch_point->state = WebTouchPoint::State::kStateCancelled;
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

void EventSender::DumpFilenameBeingDragged(blink::WebLocalFrame* frame) {
  if (!current_drag_data_)
    return;

  auto* frame_proxy =
      static_cast<WebFrameTestProxy*>(RenderFrame::FromWebFrame(frame));
  WebVector<WebDragData::Item> items = current_drag_data_->Items();
  for (const auto& item : items) {
    if (const auto* binary_data_item =
            absl::get_if<WebDragData::BinaryDataItem>(&item)) {
      WebURL url = binary_data_item->source_url;
      WebString filename_extension = binary_data_item->filename_extension;
      WebString content_disposition = binary_data_item->content_disposition;
      base::FilePath filename =
          net::GenerateFileName(url, content_disposition.Utf8(),
                                std::string(),   // referrer_charset
                                std::string(),   // suggested_name
                                std::string(),   // mime_type
                                std::string());  // default_name
#if BUILDFLAG(IS_WIN)
      filename = filename.ReplaceExtension(
          base::UTF8ToWide(filename_extension.Utf8()));
#else
      filename = filename.ReplaceExtension(filename_extension.Utf8());
#endif
      test_runner_->PrintMessage(std::string("Filename being dragged: ") +
                                     filename.AsUTF8Unsafe() + "\n",
                                 *frame_proxy);
      return;
    }
  }
}

void EventSender::TouchStart(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::Type::kTouchStart, args);
}

void EventSender::TouchMove(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::Type::kTouchMove, args);
}

void EventSender::TouchCancel(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::Type::kTouchCancel, args);
}

void EventSender::TouchEnd(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::Type::kTouchEnd, args);
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
    blink::WebLocalFrame* frame,
    const WebVector<WebDragData::Item>& items) {
  if (current_drag_data_) {
    // Nested dragging not supported, fuzzer code a likely culprit.
    // Cancel the current drag operation and throw an error.
    KeyDown("Escape", 0, DOMKeyLocationStandard);
    v8::Isolate* isolate =
        web_frame_widget_->LocalRoot()->GetAgentGroupScheduler()->Isolate();
    isolate->ThrowException(v8::Exception::Error(gin::StringToV8(
        isolate,
        "Nested beginDragWithFiles/beginDragWithStringData() not supported.")));
    return;
  }

  current_drag_data_ = blink::WebDragData();
  std::vector<base::FilePath> file_paths;
  for (const WebDragData::Item& item : items) {
    current_drag_data_->AddItem(item);
    if (const auto* filename_item =
            absl::get_if<WebDragData::FilenameItem>(&item)) {
      file_paths.push_back(blink::WebStringToFilePath(filename_item->filename));
    }
  }
  if (!file_paths.empty()) {
    auto* frame_proxy =
        static_cast<WebFrameTestProxy*>(RenderFrame::FromWebFrame(frame));
    current_drag_data_->SetFilesystemId(
        test_runner_->RegisterIsolatedFileSystem(file_paths, *frame_proxy));
  }
  current_drag_effects_allowed_ = blink::kDragOperationCopy;

  const gfx::PointF& last_pos =
      current_pointer_state_[kRawMousePointerId].last_pos_;

  // Provide a drag source.
  MainFrameWidget()->DragTargetDragEnter(
      *current_drag_data_, last_pos, last_pos, current_drag_effects_allowed_, 0,
      base::DoNothing());
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

void EventSender::BeginDragWithFiles(blink::WebLocalFrame* frame,
                                     const std::vector<std::string>& files) {
  WebVector<WebDragData::Item> items;

  for (const std::string& file_path : files) {
    WebDragData::FilenameItem item = {
        .filename = test_runner_->GetAbsoluteWebStringFromUTF8Path(file_path),
    };
    items.emplace_back(item);
  }

  BeginDragWithItems(frame, items);
}

void EventSender::BeginDragWithStringData(blink::WebLocalFrame* frame,
                                          const std::string& data,
                                          const std::string& mime_type) {
  WebVector<WebDragData::Item> items;
  WebDragData::StringItem item = {
      .type = WebString::FromUTF8(mime_type),
      .data = WebString::FromUTF8(data),
  };
  items.emplace_back(item);

  BeginDragWithItems(frame, items);
}

void EventSender::AddTouchPoint(float x, float y, gin::Arguments* args) {
  if (touch_points_.size() == WebTouchEvent::kTouchesLengthCap) {
    args->ThrowError();
    return;
  }

  // Web tests provide inputs in device-scale independent values, and need to be
  // adjusted to physical pixels when blink is working in physical pixels as
  // determined by UseZoomForDSF.
  float dsf = DeviceScaleFactorForEvents();
  x *= dsf;
  y *= dsf;

  WebTouchPoint touch_point;
  touch_point.pointer_type = WebPointerProperties::PointerType::kTouch;
  touch_point.state = WebTouchPoint::State::kStatePressed;
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

void EventSender::GestureScrollPopup(blink::WebLocalFrame* frame,
                                     gin::Arguments* args) {
  DCHECK(view()->GetPagePopup());
  double x;
  double y;
  double delta_x;
  double delta_y;
  if (!args->GetNext(&x) || !args->GetNext(&y) || !args->GetNext(&delta_x) ||
      !args->GetNext(&delta_y)) {
    args->ThrowError();
    return;
  }
  DCHECK(!std::isnan(x));
  DCHECK(!std::isnan(y));
  DCHECK(!std::isnan(delta_x));
  DCHECK(!std::isnan(delta_y));

  float dsf = DeviceScaleFactorForEvents();
  x *= dsf;
  y *= dsf;
  delta_x *= dsf;
  delta_y *= dsf;

  gfx::PointF gesture_location(x, y);

  // Send GestureScrollBegin.
  WebGestureEvent scroll_begin(
      WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      GetCurrentEventTime(), blink::WebGestureDevice::kTouchscreen);

  scroll_begin.SetPositionInWidget(gesture_location);
  scroll_begin.SetPositionInScreen(scroll_begin.PositionInWidget());

  HandleInputEventOnViewOrPopup(scroll_begin);

  // Send GestureScrollUpdate.
  WebGestureEvent scroll_update(
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      GetCurrentEventTime(), blink::WebGestureDevice::kTouchscreen);

  scroll_update.data.scroll_update.delta_x = delta_x;
  scroll_update.data.scroll_update.delta_y = delta_y;
  scroll_update.SetPositionInWidget(gesture_location);
  gesture_location.Offset(delta_x, delta_y);
  scroll_update.SetPositionInScreen(scroll_update.PositionInWidget());

  HandleInputEventOnViewOrPopup(scroll_update);

  // Send GestureScrollEnd.
  WebGestureEvent scroll_end(WebInputEvent::Type::kGestureScrollEnd,
                             WebInputEvent::kNoModifiers, GetCurrentEventTime(),
                             blink::WebGestureDevice::kTouchscreen);
  scroll_end.SetPositionInWidget(gesture_location);
  scroll_end.SetPositionInScreen(scroll_end.PositionInWidget());

  HandleInputEventOnViewOrPopup(scroll_end);
}

void EventSender::GestureTap(blink::WebLocalFrame* frame,
                             gin::Arguments* args) {
  GestureEvent(WebInputEvent::Type::kGestureTap, frame, args);
}

void EventSender::GestureTapDown(blink::WebLocalFrame* frame,
                                 gin::Arguments* args) {
  GestureEvent(WebInputEvent::Type::kGestureTapDown, frame, args);
}

void EventSender::GestureShowPress(blink::WebLocalFrame* frame,
                                   gin::Arguments* args) {
  GestureEvent(WebInputEvent::Type::kGestureShowPress, frame, args);
}

void EventSender::GestureTapCancel(blink::WebLocalFrame* frame,
                                   gin::Arguments* args) {
  GestureEvent(WebInputEvent::Type::kGestureTapCancel, frame, args);
}

void EventSender::GestureLongPress(blink::WebLocalFrame* frame,
                                   gin::Arguments* args) {
  GestureEvent(WebInputEvent::Type::kGestureLongPress, frame, args);
}

void EventSender::GestureLongTap(blink::WebLocalFrame* frame,
                                 gin::Arguments* args) {
  GestureEvent(WebInputEvent::Type::kGestureLongTap, frame, args);
}

void EventSender::GestureTwoFingerTap(blink::WebLocalFrame* frame,
                                      gin::Arguments* args) {
  GestureEvent(WebInputEvent::Type::kGestureTwoFingerTap, frame, args);
}

void EventSender::MouseMoveTo(blink::WebLocalFrame* frame,
                              gin::Arguments* args) {
  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  double x;
  double y;
  if (!args->GetNext(&x) || !args->GetNext(&y)) {
    args->ThrowError();
    return;
  }
  DCHECK(!std::isnan(x));
  DCHECK(!std::isnan(y));

  // Web tests provide inputs in device-scale independent values, and need to be
  // adjusted to physical pixels when blink is working in physical pixels as
  // determined by UseZoomForDSF.
  float dsf = DeviceScaleFactorForEvents();
  x *= dsf;
  y *= dsf;

  // The x and y coordinates here are relative to the |frame|, but will be
  // dispatched to the widget, so they need to be translated to find the same
  // position relative to the widget's origin.
  // The frame's viewport rect is already in physical pixels when UseZoomForDSF
  // is enabled.
  x += frame->GetPositionInViewportForTesting().x();
  y += frame->GetPositionInViewportForTesting().y();

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
    saved_event.pos = gfx::PointF(x, y);
    saved_event.modifiers = modifiers;
    mouse_event_queue_.push_back(saved_event);
  } else {
    current_pointer_state_[pointerId].last_pos_ = gfx::PointF(x, y);
    current_pointer_state_[pointerId].modifiers_ = modifiers;
    WebMouseEvent event(WebInputEvent::Type::kMouseMove,
                        ModifiersForPointer(pointerId), GetCurrentEventTime());
    int click_count = pointerType == WebPointerProperties::PointerType::kMouse
                          ? click_count_
                          : 0;
    InitMouseEventGeneric(
        current_pointer_state_[kRawMousePointerId].pressed_button_,
        current_pointer_state_[kRawMousePointerId].current_buttons_,
        gfx::PointF(x, y), click_count, pointerType, pointerId, pressure, tiltX,
        tiltY, &event);
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

  WebMouseEvent event(WebInputEvent::Type::kMouseLeave,
                      ModifiersForPointer(pointerId), GetCurrentEventTime());
  InitMouseEventGeneric(WebMouseEvent::Button::kNoButton, 0,
                        current_pointer_state_[kRawMousePointerId].last_pos_,
                        click_count_, pointerType, pointerId, 0.0, 0, 0,
                        &event);
  HandleInputEventOnViewOrPopup(event);
}

void EventSender::ScheduleAsynchronousClick(WebLocalFrame* frame,
                                            int button_number,
                                            int modifiers) {
  frame->GetTaskRunner(blink::TaskType::kInternalTest)
      ->PostTask(FROM_HERE, base::BindOnce(&EventSender::MouseDown,
                                           weak_factory_.GetWeakPtr(),
                                           button_number, modifiers));
  frame->GetTaskRunner(blink::TaskType::kInternalTest)
      ->PostTask(FROM_HERE, base::BindOnce(&EventSender::MouseUp,
                                           weak_factory_.GetWeakPtr(),
                                           button_number, modifiers));
}

void EventSender::ScheduleAsynchronousKeyDown(blink::WebLocalFrame* frame,
                                              const std::string& code_str,
                                              int modifiers,
                                              KeyLocationCode location) {
  frame->GetTaskRunner(blink::TaskType::kInternalTest)
      ->PostTask(FROM_HERE, base::BindOnce(&EventSender::KeyDown,
                                           weak_factory_.GetWeakPtr(), code_str,
                                           modifiers, location));
}

void EventSender::ConsumeUserActivation() {
  view()->MainFrame()->ToWebLocalFrame()->ConsumeTransientUserActivation();
}

base::TimeTicks EventSender::GetCurrentEventTime() const {
  return base::TimeTicks::Now() + time_offset_;
}

void EventSender::DoLeapForward(int milliseconds) {
  time_offset_ += base::Milliseconds(milliseconds);
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
      touch_cancelable_ ? WebInputEvent::DispatchType::kBlocking
                        : WebInputEvent::DispatchType::kEventNonBlocking;

  for (unsigned i = 0; i < touch_points_.size(); ++i) {
    const WebTouchPoint& touch_point = touch_points_[i];
    if (touch_point.state != blink::WebTouchPoint::State::kStateStationary) {
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
          (pointer_event.GetType() == WebInputEvent::Type::kPointerDown ||
           pointer_event.GetType() == WebInputEvent::Type::kPointerUp)
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
    if (touch_point->state == WebTouchPoint::State::kStateReleased ||
        touch_point->state == WebTouchPoint::State::kStateCancelled) {
      touch_points_.erase(touch_points_.begin() + i);
      --i;
    } else {
      touch_point->state = WebTouchPoint::State::kStateStationary;
    }
  }
}

void EventSender::GestureEvent(WebInputEvent::Type type,
                               blink::WebLocalFrame* frame,
                               gin::Arguments* args) {
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
  DCHECK(!std::isnan(x));
  DCHECK(!std::isnan(y));

  // Web tests provide inputs in device-scale independent values, and need to be
  // adjusted to physical pixels when blink is working in physical pixels as
  // determined by UseZoomForDSF.
  float dsf = DeviceScaleFactorForEvents();
  x *= dsf;
  y *= dsf;

  // The x and y coordinates here are relative to the |frame|, but will be
  // dispatched to the widget, so they need to be translated to find the same
  // position relative to the widget's origin.
  // The frame's viewport rect is already in physical pixels when UseZoomForDSF
  // is enabled.
  x += frame->GetPositionInViewportForTesting().x();
  y += frame->GetPositionInViewportForTesting().y();

  switch (type) {
    case WebInputEvent::Type::kGestureFlingStart:
    case WebInputEvent::Type::kGestureFlingCancel:
      // Flings are no longer handled on the main thread.
      NOTREACHED_IN_MIGRATION();
      return;
    case WebInputEvent::Type::kGestureTap: {
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
      event.SetPositionInWidget(gfx::PointF(x, y));
      break;
    }
    case WebInputEvent::Type::kGestureTapUnconfirmed:
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
      event.SetPositionInWidget(gfx::PointF(x, y));
      break;
    case WebInputEvent::Type::kGestureTapDown: {
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
      event.SetPositionInWidget(gfx::PointF(x, y));
      event.data.tap_down.width = width;
      event.data.tap_down.height = height;
      break;
    }
    case WebInputEvent::Type::kGestureShowPress: {
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
      event.SetPositionInWidget(gfx::PointF(x, y));
      event.data.show_press.width = width;
      event.data.show_press.height = height;
      break;
    }
    case WebInputEvent::Type::kGestureTapCancel:
      event.SetPositionInWidget(gfx::PointF(x, y));
      break;
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap:
      event.SetPositionInWidget(gfx::PointF(x, y));
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
    case WebInputEvent::Type::kGestureTwoFingerTap:
      event.SetPositionInWidget(gfx::PointF(x, y));
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
      NOTREACHED_IN_MIGRATION();
  }

  event.unique_touch_event_id = GetUniqueTouchEventId(args);
  if (!GetPointerType(args, false, event.primary_pointer_type))
    return;

  event.SetPositionInScreen(event.PositionInWidget());

  if (force_layout_on_events_)
    UpdateLifecycleToPrePaint();

  std::optional<WebInputEventResult> result =
      HandleInputEventOnViewOrPopup(event);
  // Async gestures are not currently supported.
  CHECK(result);

  // Long press might start a drag drop session. Complete it if so.
  if (type == WebInputEvent::Type::kGestureLongPress && current_drag_data_) {
    WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                              ModifiersForPointer(kRawMousePointerId),
                              GetCurrentEventTime());

    InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                   current_pointer_state_[kRawMousePointerId].current_buttons_,
                   gfx::PointF(x, y), click_count_, &mouse_event);

    FinishDragAndDrop(mouse_event, ui::mojom::DragOperation::kNone, false);
  }
  args->Return(*result != WebInputEventResult::kNotHandled);
}

void EventSender::UpdateClickCountForButton(WebMouseEvent::Button button_type) {
  // The radius constant is dsf-independent, but events are in physical pixels.
  // Convert the radius to physical pixels to compare to the event position.
  float radius = kMultipleClickRadiusPixels * DeviceScaleFactorForEvents();

  bool fast_enough =
      GetCurrentEventTime() - last_click_time_ < kMultipleClickTime;
  bool nearby_enough =
      !OutsideRadius(current_pointer_state_[kRawMousePointerId].last_pos_,
                     last_click_pos_, radius);
  bool same_button = button_type == last_button_type_;

  if (fast_enough && nearby_enough && same_button) {
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
  DCHECK(!std::isnan(horizontal));
  DCHECK(!std::isnan(vertical));

  // Web tests provide inputs in device-scale independent values, and need to be
  // adjusted to physical pixels.
  // While events have floating point positions, blink still expects them to be
  // integers (see MouseEvent::screenX() for example). If the web test provides
  // a non-whole number (including after device scale factor is applied) we drop
  // the fractional part.
  float dsf = DeviceScaleFactorForEvents();
  horizontal *= dsf;
  vertical *= dsf;

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
  WebMouseWheelEvent event(WebInputEvent::Type::kMouseWheel,
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
    event.delta_units = ui::ScrollGranularity::kScrollByPage;
  } else if (has_precise_scrolling_deltas) {
    event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  } else {
    event.delta_units = ui::ScrollGranularity::kScrollByPixel;
  }
  event.phase = phase;
  if (scroll_type == MouseScrollType::PIXEL) {
    event.wheel_ticks_x /= kScrollbarPixelsPerTick;
    event.wheel_ticks_y /= kScrollbarPixelsPerTick;
  } else {
    event.delta_x *= kScrollbarPixelsPerTick;
    event.delta_y *= kScrollbarPixelsPerTick;
  }
  event.event_action =
      blink::WebMouseWheelEvent::GetPlatformSpecificDefaultEventAction(event);
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

void EventSender::FinishDragAndDrop(const WebMouseEvent& event,
                                    ui::mojom::DragOperation drag_effect,
                                    bool document_is_handling_drag) {
  // Bail if cancelled.
  if (!current_drag_data_)
    return;

  current_drag_effect_ = drag_effect;
  if (current_drag_effect_ != ui::mojom::DragOperation::kNone) {
    // Specifically pass any keyboard modifiers to the drop method. This allows
    // tests to control the drop type (i.e. copy or move).
    MainFrameWidget()->DragTargetDrop(
        *current_drag_data_, event.PositionInWidget(), event.PositionInScreen(),
        event.GetModifiers(), base::DoNothing());
  } else {
    MainFrameWidget()->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
  }
  current_drag_data_ = std::nullopt;
  MainFrameWidget()->DragSourceEndedAt(event.PositionInWidget(),
                                       event.PositionInScreen(),
                                       current_drag_effect_, base::DoNothing());
  MainFrameWidget()->DragSourceSystemDragEnded();
}

void EventSender::DoDragAfterMouseUp(const WebMouseEvent& event) {
  last_click_time_ = event.TimeStamp();
  last_click_pos_ = current_pointer_state_[kRawMousePointerId].last_pos_;

  // If we're in a drag operation, complete it.
  if (!current_drag_data_)
    return;

  MainFrameWidget()->DragTargetDragOver(
      event.PositionInWidget(), event.PositionInScreen(),
      current_drag_effects_allowed_, event.GetModifiers(),
      base::BindOnce(&EventSender::FinishDragAndDrop,
                     weak_factory_.GetWeakPtr(), event));
}

void EventSender::DoDragAfterMouseMove(const WebMouseEvent& event) {
  if (current_pointer_state_[kRawMousePointerId].pressed_button_ ==
          WebMouseEvent::Button::kNoButton ||
      !current_drag_data_) {
    return;
  }

  MainFrameWidget()->DragTargetDragOver(
      event.PositionInWidget(), event.PositionInScreen(),
      current_drag_effects_allowed_, event.GetModifiers(),
      base::BindOnce(
          [](base::WeakPtr<EventSender> sender, ui::mojom::DragOperation op,
             bool document_is_handling_drag) {
            if (sender)
              sender->current_drag_effect_ = op;
          },
          weak_factory_.GetWeakPtr()));
}

void EventSender::ReplaySavedEvents() {
  replaying_saved_events_ = true;
  while (!mouse_event_queue_.empty()) {
    SavedEvent e = mouse_event_queue_.front();
    mouse_event_queue_.pop_front();

    switch (e.type) {
      case SavedEvent::TYPE_MOUSE_MOVE: {
        current_pointer_state_[kRawMousePointerId].modifiers_ = e.modifiers;
        WebMouseEvent event(WebInputEvent::Type::kMouseMove,
                            ModifiersForPointer(kRawMousePointerId),
                            GetCurrentEventTime());
        InitMouseEvent(
            current_pointer_state_[kRawMousePointerId].pressed_button_,
            current_pointer_state_[kRawMousePointerId].current_buttons_, e.pos,
            click_count_, &event);
        current_pointer_state_[kRawMousePointerId].last_pos_ =
            event.PositionInWidget();
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

        WebMouseEvent event(WebInputEvent::Type::kMouseUp,
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
        NOTREACHED_IN_MIGRATION();
    }
  }

  replaying_saved_events_ = false;
}
std::optional<blink::WebInputEventResult>
EventSender::HandleInputEventOnViewOrPopup(const WebInputEvent& event,
                                           bool async) {
  last_event_timestamp_ = event.TimeStamp();

  blink::WebWidget* target =
      view()->GetPagePopup() &&
              !WebInputEvent::IsKeyboardEventType(event.GetType())
          ? view()->GetPagePopup()
          : widget();
  if (async) {
    target->DispatchNonBlockingEventForTesting(
        std::make_unique<blink::WebCoalescedInputEvent>(event,
                                                        ui::LatencyInfo()));
    return std::nullopt;
  } else {
    return target->HandleInputEvent(
        blink::WebCoalescedInputEvent(event, ui::LatencyInfo()));
  }
}

const blink::WebView* EventSender::view() const {
  return web_frame_widget_->LocalRoot()->View();
}

blink::WebView* EventSender::view() {
  return web_frame_widget_->LocalRoot()->View();
}

blink::WebWidget* EventSender::widget() {
  return web_frame_widget_;
}

blink::WebFrameWidget* EventSender::MainFrameWidget() {
  if (!view() || !view()->MainFrame())
    return nullptr;

  DCHECK(view()->MainFrame()->IsWebLocalFrame())
      << "Event Sender doesn't support being run in a remote frame for this "
         "operation.";

  if (!view()->MainFrame()->ToWebLocalFrame())
    return nullptr;

  return view()->MainFrame()->ToWebLocalFrame()->FrameWidget();
}

void EventSender::UpdateLifecycleToPrePaint() {
  widget()->UpdateLifecycle(blink::WebLifecycleUpdate::kPrePaint,
                            blink::DocumentUpdateReason::kTest);
}

float EventSender::DeviceScaleFactorForEvents() {
  return web_frame_widget_->GetOriginalScreenInfo().device_scale_factor;
}

}  // namespace content
