// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"

#include <memory>

#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/input/scroll_manager.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text_control_single_line.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/windows_keyboard_codes.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#import <Carbon/Carbon.h>
#endif

namespace blink {

namespace {

#if defined(OS_WIN)
static const unsigned short kHIGHBITMASKSHORT = 0x8000;
#endif

const int kVKeyProcessKey = 229;

WebFocusType FocusDirectionForKey(KeyboardEvent* event) {
  if (event->ctrlKey() || event->metaKey() || event->shiftKey())
    return kWebFocusTypeNone;

  WebFocusType ret_val = kWebFocusTypeNone;
  if (event->key() == "ArrowDown")
    ret_val = kWebFocusTypeDown;
  else if (event->key() == "ArrowUp")
    ret_val = kWebFocusTypeUp;
  else if (event->key() == "ArrowLeft")
    ret_val = kWebFocusTypeLeft;
  else if (event->key() == "ArrowRight")
    ret_val = kWebFocusTypeRight;
  return ret_val;
}

bool MapKeyCodeForScroll(int key_code,
                         WebInputEvent::Modifiers modifiers,
                         ScrollDirection* scroll_direction,
                         ScrollGranularity* scroll_granularity,
                         WebFeature* scroll_use_uma) {
  if (modifiers & WebInputEvent::kShiftKey ||
      modifiers & WebInputEvent::kMetaKey)
    return false;

  if (modifiers & WebInputEvent::kAltKey) {
    // Alt-Up/Down should behave like PageUp/Down on Mac.  (Note that Alt-keys
    // on other platforms are suppressed due to isSystemKey being set.)
    if (key_code == VKEY_UP)
      key_code = VKEY_PRIOR;
    else if (key_code == VKEY_DOWN)
      key_code = VKEY_NEXT;
    else
      return false;
  }

  if (modifiers & WebInputEvent::kControlKey) {
    // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
    // key combinations which affect scrolling.
    if (key_code != VKEY_HOME && key_code != VKEY_END)
      return false;
  }

  switch (key_code) {
    case VKEY_LEFT:
      *scroll_direction = kScrollLeftIgnoringWritingMode;
      *scroll_granularity = kScrollByLine;
      *scroll_use_uma = WebFeature::kScrollByKeyboardArrowKeys;
      break;
    case VKEY_RIGHT:
      *scroll_direction = kScrollRightIgnoringWritingMode;
      *scroll_granularity = kScrollByLine;
      *scroll_use_uma = WebFeature::kScrollByKeyboardArrowKeys;
      break;
    case VKEY_UP:
      *scroll_direction = kScrollUpIgnoringWritingMode;
      *scroll_granularity = kScrollByLine;
      *scroll_use_uma = WebFeature::kScrollByKeyboardArrowKeys;
      break;
    case VKEY_DOWN:
      *scroll_direction = kScrollDownIgnoringWritingMode;
      *scroll_granularity = kScrollByLine;
      *scroll_use_uma = WebFeature::kScrollByKeyboardArrowKeys;
      break;
    case VKEY_HOME:
      *scroll_direction = kScrollUpIgnoringWritingMode;
      *scroll_granularity = kScrollByDocument;
      *scroll_use_uma = WebFeature::kScrollByKeyboardHomeEndKeys;
      break;
    case VKEY_END:
      *scroll_direction = kScrollDownIgnoringWritingMode;
      *scroll_granularity = kScrollByDocument;
      *scroll_use_uma = WebFeature::kScrollByKeyboardHomeEndKeys;
      break;
    case VKEY_PRIOR:  // page up
      *scroll_direction = kScrollUpIgnoringWritingMode;
      *scroll_granularity = kScrollByPage;
      *scroll_use_uma = WebFeature::kScrollByKeyboardPageUpDownKeys;
      break;
    case VKEY_NEXT:  // page down
      *scroll_direction = kScrollDownIgnoringWritingMode;
      *scroll_granularity = kScrollByPage;
      *scroll_use_uma = WebFeature::kScrollByKeyboardPageUpDownKeys;
      break;
    default:
      return false;
  }

  return true;
}

}  // namespace

KeyboardEventManager::KeyboardEventManager(LocalFrame& frame,
                                           ScrollManager& scroll_manager)
    : frame_(frame), scroll_manager_(scroll_manager) {}

void KeyboardEventManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(scroll_manager_);
}

bool KeyboardEventManager::HandleAccessKey(const WebKeyboardEvent& evt) {
  // FIXME: Ignoring the state of Shift key is what neither IE nor Firefox do.
  // IE matches lower and upper case access keys regardless of Shift key state -
  // but if both upper and lower case variants are present in a document, the
  // correct element is matched based on Shift key state.  Firefox only matches
  // an access key if Shift is not pressed, and does that case-insensitively.
  DCHECK(!(kAccessKeyModifiers & WebInputEvent::kShiftKey));
  if ((evt.GetModifiers() & (WebKeyboardEvent::kKeyModifiers &
                             ~WebInputEvent::kShiftKey)) != kAccessKeyModifiers)
    return false;
  String key = String(evt.unmodified_text);
  Element* elem =
      frame_->GetDocument()->GetElementByAccessKey(key.DeprecatedLower());
  if (!elem)
    return false;
  elem->AccessKeyAction(false);
  return true;
}

WebInputEventResult KeyboardEventManager::KeyEvent(
    const WebKeyboardEvent& initial_key_event) {
  frame_->GetChromeClient().ClearToolTip(*frame_);

  if (initial_key_event.windows_key_code == VK_CAPITAL)
    CapsLockStateMayHaveChanged();

  if (scroll_manager_->MiddleClickAutoscrollInProgress()) {
    DCHECK(RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled());
    // If a key is pressed while the middleClickAutoscroll is in progress then
    // we want to stop.
    if (initial_key_event.GetType() == WebInputEvent::kKeyDown ||
        initial_key_event.GetType() == WebInputEvent::kRawKeyDown)
      scroll_manager_->StopMiddleClickAutoscroll();

    // If we were in panscroll mode, we swallow the key event
    return WebInputEventResult::kHandledSuppressed;
  }

  // Check for cases where we are too early for events -- possible unmatched key
  // up from pressing return in the location bar.
  Node* node = EventTargetNodeForDocument(frame_->GetDocument());
  if (!node)
    return WebInputEventResult::kNotHandled;

  // To be meaningful enough to indicate user intention, a keyboard event needs
  // - not to be a modifier event
  // https://crbug.com/709765
  bool is_modifier =
      Platform::Current()->IsDomKeyForModifier(initial_key_event.dom_key);

  std::unique_ptr<UserGestureIndicator> gesture_indicator;
  if (!is_modifier)
    gesture_indicator = LocalFrame::NotifyUserActivation(frame_);

  // In IE, access keys are special, they are handled after default keydown
  // processing, but cannot be canceled - this is hard to match.  On Mac OS X,
  // we process them before dispatching keydown, as the default keydown handler
  // implements Emacs key bindings, which may conflict with access keys. Then we
  // dispatch keydown, but suppress its default handling.
  // On Windows, WebKit explicitly calls handleAccessKey() instead of
  // dispatching a keypress event for WM_SYSCHAR messages.  Other platforms
  // currently match either Mac or Windows behavior, depending on whether they
  // send combined KeyDown events.
  bool matched_an_access_key = false;
  if (initial_key_event.GetType() == WebInputEvent::kKeyDown)
    matched_an_access_key = HandleAccessKey(initial_key_event);

  // FIXME: it would be fair to let an input method handle KeyUp events before
  // DOM dispatch.
  if (initial_key_event.GetType() == WebInputEvent::kKeyUp ||
      initial_key_event.GetType() == WebInputEvent::kChar) {
    KeyboardEvent* dom_event = KeyboardEvent::Create(
        initial_key_event, frame_->GetDocument()->domWindow());

    return event_handling_util::ToWebInputEventResult(
        node->DispatchEvent(*dom_event));
  }

  WebKeyboardEvent key_down_event = initial_key_event;
  if (key_down_event.GetType() != WebInputEvent::kRawKeyDown)
    key_down_event.SetType(WebInputEvent::kRawKeyDown);
  KeyboardEvent* keydown =
      KeyboardEvent::Create(key_down_event, frame_->GetDocument()->domWindow());
  if (matched_an_access_key)
    keydown->SetDefaultPrevented(true);
  keydown->SetTarget(node);

  DispatchEventResult dispatch_result = node->DispatchEvent(*keydown);
  if (dispatch_result != DispatchEventResult::kNotCanceled)
    return event_handling_util::ToWebInputEventResult(dispatch_result);
  // If frame changed as a result of keydown dispatch, then return early to
  // avoid sending a subsequent keypress message to the new frame.
  bool changed_focused_frame =
      frame_->GetPage() &&
      frame_ != frame_->GetPage()->GetFocusController().FocusedOrMainFrame();
  if (changed_focused_frame)
    return WebInputEventResult::kHandledSystem;

  if (initial_key_event.GetType() == WebInputEvent::kRawKeyDown)
    return WebInputEventResult::kNotHandled;

  // Focus may have changed during keydown handling, so refetch node.
  // But if we are dispatching a fake backward compatibility keypress, then we
  // pretend that the keypress happened on the original node.
  node = EventTargetNodeForDocument(frame_->GetDocument());
  if (!node)
    return WebInputEventResult::kNotHandled;

#if defined(OS_MACOSX)
  // According to NSEvents.h, OpenStep reserves the range 0xF700-0xF8FF for
  // function keys. However, some actual private use characters happen to be
  // in this range, e.g. the Apple logo (Option+Shift+K). 0xF7FF is an
  // arbitrary cut-off.
  if (initial_key_event.text[0U] >= 0xF700 &&
      initial_key_event.text[0U] <= 0xF7FF) {
    return WebInputEventResult::kNotHandled;
  }
#endif

  WebKeyboardEvent key_press_event = initial_key_event;
  key_press_event.SetType(WebInputEvent::kChar);
  if (key_press_event.text[0] == 0)
    return WebInputEventResult::kNotHandled;
  KeyboardEvent* keypress = KeyboardEvent::Create(
      key_press_event, frame_->GetDocument()->domWindow());
  keypress->SetTarget(node);
  return event_handling_util::ToWebInputEventResult(
      node->DispatchEvent(*keypress));
}

void KeyboardEventManager::CapsLockStateMayHaveChanged() {
  if (Element* element = frame_->GetDocument()->FocusedElement()) {
    if (LayoutObject* r = element->GetLayoutObject()) {
      if (r->IsTextField())
        ToLayoutTextControlSingleLine(r)->CapsLockStateMayHaveChanged();
    }
  }
}

void KeyboardEventManager::DefaultKeyboardEventHandler(
    KeyboardEvent* event,
    Node* possible_focused_node) {
  if (event->type() == EventTypeNames::keydown) {
    frame_->GetEditor().HandleKeyboardEvent(event);
    if (event->DefaultHandled())
      return;

    // Do not perform the default action when inside a IME composition context.
    // TODO(dtapuska): Replace this with isComposing support. crbug.com/625686
    if (event->keyCode() == kVKeyProcessKey)
      return;
    if (event->key() == "Tab") {
      DefaultTabEventHandler(event);
    } else if (event->key() == "Escape") {
      DefaultEscapeEventHandler(event);
    } else {
      DefaultArrowEventHandler(event, possible_focused_node);
    }
  }
  if (event->type() == EventTypeNames::keypress) {
    frame_->GetEditor().HandleKeyboardEvent(event);
    if (event->DefaultHandled())
      return;
    if (event->charCode() == ' ')
      DefaultSpaceEventHandler(event, possible_focused_node);
  }
}

void KeyboardEventManager::DefaultSpaceEventHandler(
    KeyboardEvent* event,
    Node* possible_focused_node) {
  DCHECK_EQ(event->type(), EventTypeNames::keypress);

  if (event->ctrlKey() || event->metaKey() || event->altKey())
    return;

  ScrollDirection direction = event->shiftKey() ? kScrollBlockDirectionBackward
                                                : kScrollBlockDirectionForward;

  // FIXME: enable scroll customization in this case. See crbug.com/410974.
  if (scroll_manager_->LogicalScroll(direction, kScrollByPage, nullptr,
                                     possible_focused_node)) {
    UseCounter::Count(frame_->GetDocument(),
                      WebFeature::kScrollByKeyboardSpacebarKey);
    event->SetDefaultHandled();
    return;
  }
}

void KeyboardEventManager::DefaultArrowEventHandler(
    KeyboardEvent* event,
    Node* possible_focused_node) {
  DCHECK_EQ(event->type(), EventTypeNames::keydown);

  Page* page = frame_->GetPage();
  if (!page)
    return;

  WebFocusType type = FocusDirectionForKey(event);
  if (type != kWebFocusTypeNone && IsSpatialNavigationEnabled(frame_) &&
      !frame_->GetDocument()->InDesignMode()) {
    if (page->GetFocusController().AdvanceFocus(type)) {
      event->SetDefaultHandled();
      return;
    }
  }

  if (event->KeyEvent() && event->KeyEvent()->is_system_key)
    return;

  ScrollDirection scroll_direction;
  ScrollGranularity scroll_granularity;
  WebFeature scroll_use_uma;
  if (!MapKeyCodeForScroll(event->keyCode(), event->GetModifiers(),
                           &scroll_direction, &scroll_granularity,
                           &scroll_use_uma))
    return;

  if (scroll_manager_->BubblingScroll(scroll_direction, scroll_granularity,
                                      nullptr, possible_focused_node)) {
    UseCounter::Count(frame_->GetDocument(), scroll_use_uma);
    event->SetDefaultHandled();
    return;
  }
}

void KeyboardEventManager::DefaultTabEventHandler(KeyboardEvent* event) {
  DCHECK_EQ(event->type(), EventTypeNames::keydown);

  // We should only advance focus on tabs if no special modifier keys are held
  // down.
  if (event->ctrlKey() || event->metaKey())
    return;

#if !defined(OS_MACOSX)
  // Option-Tab is a shortcut based on a system-wide preference on Mac but
  // should be ignored on all other platforms.
  if (event->altKey())
    return;
#endif

  Page* page = frame_->GetPage();
  if (!page)
    return;
  if (!page->TabKeyCyclesThroughElements())
    return;

  WebFocusType focus_type =
      event->shiftKey() ? kWebFocusTypeBackward : kWebFocusTypeForward;

  // Tabs can be used in design mode editing.
  if (frame_->GetDocument()->InDesignMode())
    return;

  if (page->GetFocusController().AdvanceFocus(focus_type,
                                              frame_->GetDocument()
                                                  ->domWindow()
                                                  ->GetInputDeviceCapabilities()
                                                  ->FiresTouchEvents(false)))
    event->SetDefaultHandled();
}

void KeyboardEventManager::DefaultEscapeEventHandler(KeyboardEvent* event) {
  if (HTMLDialogElement* dialog = frame_->GetDocument()->ActiveModalDialog())
    dialog->DispatchEvent(*Event::CreateCancelable(EventTypeNames::cancel));
}

static OverrideCapsLockState g_override_caps_lock_state;

void KeyboardEventManager::SetCurrentCapsLockState(
    OverrideCapsLockState state) {
  g_override_caps_lock_state = state;
}

bool KeyboardEventManager::CurrentCapsLockState() {
  switch (g_override_caps_lock_state) {
    case OverrideCapsLockState::kDefault:
#if defined(OS_WIN)
      // FIXME: Does this even work inside the sandbox?
      return GetKeyState(VK_CAPITAL) & 1;
#elif defined(OS_MACOSX)
      return GetCurrentKeyModifiers() & alphaLock;
#else
      // Caps lock state use is limited to Mac password input
      // fields, so just return false. See http://crbug.com/618739.
      return false;
#endif
    case OverrideCapsLockState::kOn:
      return true;
    case OverrideCapsLockState::kOff:
    default:
      return false;
  }
}

WebInputEvent::Modifiers KeyboardEventManager::GetCurrentModifierState() {
  unsigned modifiers = 0;
#if defined(OS_WIN)
  if (GetKeyState(VK_SHIFT) & kHIGHBITMASKSHORT)
    modifiers |= WebInputEvent::kShiftKey;
  if (GetKeyState(VK_CONTROL) & kHIGHBITMASKSHORT)
    modifiers |= WebInputEvent::kControlKey;
  if (GetKeyState(VK_MENU) & kHIGHBITMASKSHORT)
    modifiers |= WebInputEvent::kAltKey;
#elif defined(OS_MACOSX)
  UInt32 current_modifiers = GetCurrentKeyModifiers();
  if (current_modifiers & ::shiftKey)
    modifiers |= WebInputEvent::kShiftKey;
  if (current_modifiers & ::controlKey)
    modifiers |= WebInputEvent::kControlKey;
  if (current_modifiers & ::optionKey)
    modifiers |= WebInputEvent::kAltKey;
  if (current_modifiers & ::cmdKey)
    modifiers |= WebInputEvent::kMetaKey;
#else
  // TODO(crbug.com/538289): Implement on other platforms.
  return static_cast<WebInputEvent::Modifiers>(0);
#endif
  return static_cast<WebInputEvent::Modifiers>(modifiers);
}

}  // namespace blink
