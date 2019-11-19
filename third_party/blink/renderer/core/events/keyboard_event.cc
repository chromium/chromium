/**
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/events/keyboard_event.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/windows_keyboard_codes.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace blink {

namespace {

const AtomicString& EventTypeForKeyboardEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::kKeyUp:
      return event_type_names::kKeyup;
    case WebInputEvent::kRawKeyDown:
      return event_type_names::kKeydown;
    case WebInputEvent::kChar:
      return event_type_names::kKeypress;
    case WebInputEvent::kKeyDown:
      // The caller should disambiguate the combined event into RawKeyDown or
      // Char events.
      break;
    default:
      break;
  }
  NOTREACHED();
  return event_type_names::kKeydown;
}

KeyboardEvent::KeyLocationCode GetKeyLocationCode(const WebInputEvent& key) {
  if (key.GetModifiers() & WebInputEvent::kIsKeyPad)
    return KeyboardEvent::kDomKeyLocationNumpad;
  if (key.GetModifiers() & WebInputEvent::kIsLeft)
    return KeyboardEvent::kDomKeyLocationLeft;
  if (key.GetModifiers() & WebInputEvent::kIsRight)
    return KeyboardEvent::kDomKeyLocationRight;
  return KeyboardEvent::kDomKeyLocationStandard;
}

bool HasCurrentComposition(LocalDOMWindow* dom_window) {
  if (!dom_window)
    return false;
  LocalFrame* local_frame = dom_window->GetFrame();
  if (!local_frame)
    return false;
  return local_frame->GetInputMethodController().HasComposition();
}

static String FromUTF8(const std::string& s) {
  return String::FromUTF8(s.data(), s.length());
}

}  // namespace

KeyboardEvent* KeyboardEvent::Create(ScriptState* script_state,
                                     const AtomicString& type,
                                     const KeyboardEventInit* initializer) {
  if (script_state->World().IsIsolatedWorld()) {
    UIEventWithKeyState::DidCreateEventInIsolatedWorld(
        initializer->ctrlKey(), initializer->altKey(), initializer->shiftKey(),
        initializer->metaKey());
  }
  return MakeGarbageCollected<KeyboardEvent>(type, initializer);
}

KeyboardEvent::KeyboardEvent() : location_(kDomKeyLocationStandard) {}

KeyboardEvent::KeyboardEvent(const WebKeyboardEvent& key,
                             LocalDOMWindow* dom_window,
                             bool cancellable)
    : UIEventWithKeyState(
          EventTypeForKeyboardEventType(key.GetType()),
          Bubbles::kYes,
          cancellable ? Cancelable::kYes : Cancelable::kNo,
          dom_window,
          0,
          static_cast<WebInputEvent::Modifiers>(key.GetModifiers()),
          key.TimeStamp(),
          dom_window
              ? dom_window->GetInputDeviceCapabilities()->FiresTouchEvents(
                    false)
              : nullptr),
      key_event_(std::make_unique<WebKeyboardEvent>(key)),
      // TODO(crbug.com/482880): Fix this initialization to lazy initialization.
      code_(FromUTF8(ui::KeycodeConverter::DomCodeToCodeString(
          static_cast<ui::DomCode>(key.dom_code)))),
      key_(FromUTF8(ui::KeycodeConverter::DomKeyToKeyString(
          static_cast<ui::DomKey>(key.dom_key)))),
      location_(GetKeyLocationCode(key)),
      is_composing_(HasCurrentComposition(dom_window)) {
  InitLocationModifiers(location_);

  // Firefox: 0 for keydown/keyup events, character code for keypress
  // We match Firefox
  if (type() == event_type_names::kKeypress)
    char_code_ = key.text[0];

  if (type() == event_type_names::kKeydown ||
      type() == event_type_names::kKeyup)
    key_code_ = key.windows_key_code;
  else
    key_code_ = char_code_;

#if defined(OS_ANDROID)
  // FIXME: Check to see if this applies to other OS.
  // If the key event belongs to IME composition then propagate to JS.
  if (key.native_key_code == 0xE5)  // VKEY_PROCESSKEY
    key_code_ = 0xE5;
#endif
}

KeyboardEvent::KeyboardEvent(const AtomicString& event_type,
                             const KeyboardEventInit* initializer)
    : UIEventWithKeyState(event_type, initializer),
      code_(initializer->code()),
      key_(initializer->key()),
      location_(initializer->location()),
      is_composing_(initializer->isComposing()),
      char_code_(initializer->charCode()),
      key_code_(initializer->keyCode()) {
  if (initializer->repeat())
    modifiers_ |= WebInputEvent::kIsAutoRepeat;
  InitLocationModifiers(initializer->location());
}

KeyboardEvent::~KeyboardEvent() = default;

void KeyboardEvent::initKeyboardEvent(ScriptState* script_state,
                                      const AtomicString& type,
                                      bool bubbles,
                                      bool cancelable,
                                      AbstractView* view,
                                      const String& key_identifier,
                                      unsigned location,
                                      bool ctrl_key,
                                      bool alt_key,
                                      bool shift_key,
                                      bool meta_key) {
  if (IsBeingDispatched())
    return;

  if (script_state->World().IsIsolatedWorld())
    UIEventWithKeyState::DidCreateEventInIsolatedWorld(ctrl_key, alt_key,
                                                       shift_key, meta_key);

  initUIEvent(type, bubbles, cancelable, view, 0);

  location_ = location;
  InitModifiers(ctrl_key, alt_key, shift_key, meta_key);
  InitLocationModifiers(location);
}

int KeyboardEvent::keyCode() const {
  return key_code_;
}

int KeyboardEvent::charCode() const {
  return char_code_;
}

const AtomicString& KeyboardEvent::InterfaceName() const {
  return event_interface_names::kKeyboardEvent;
}

bool KeyboardEvent::IsKeyboardEvent() const {
  return true;
}

unsigned KeyboardEvent::which() const {
  // Netscape's "which" returns a virtual key code for keydown and keyup, and a
  // character code for keypress.  That's exactly what IE's "keyCode" returns.
  // So they are the same for keyboard events.
  return (unsigned)keyCode();
}

void KeyboardEvent::InitLocationModifiers(unsigned location) {
  switch (location) {
    case KeyboardEvent::kDomKeyLocationNumpad:
      modifiers_ |= WebInputEvent::kIsKeyPad;
      break;
    case KeyboardEvent::kDomKeyLocationLeft:
      modifiers_ |= WebInputEvent::kIsLeft;
      break;
    case KeyboardEvent::kDomKeyLocationRight:
      modifiers_ |= WebInputEvent::kIsRight;
      break;
  }
}

void KeyboardEvent::Trace(blink::Visitor* visitor) {
  UIEventWithKeyState::Trace(visitor);
}

}  // namespace blink
