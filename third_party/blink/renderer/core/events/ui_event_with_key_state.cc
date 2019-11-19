/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 *
 */

#include "third_party/blink/renderer/core/events/ui_event_with_key_state.h"

#include "build/build_config.h"

namespace blink {

UIEventWithKeyState::UIEventWithKeyState(
    const AtomicString& type,
    Bubbles bubbles,
    Cancelable cancelable,
    AbstractView* view,
    int detail,
    WebInputEvent::Modifiers modifiers,
    base::TimeTicks platform_time_stamp,
    InputDeviceCapabilities* source_capabilities)
    : UIEvent(type,
              bubbles,
              cancelable,
              ComposedMode::kComposed,
              platform_time_stamp,
              view,
              detail,
              source_capabilities),
      modifiers_(modifiers) {}

UIEventWithKeyState::UIEventWithKeyState(const AtomicString& type,
                                         const EventModifierInit* initializer,
                                         base::TimeTicks platform_time_stamp)
    : UIEvent(type, initializer, platform_time_stamp), modifiers_(0) {
  if (initializer->ctrlKey())
    modifiers_ |= WebInputEvent::kControlKey;
  if (initializer->shiftKey())
    modifiers_ |= WebInputEvent::kShiftKey;
  if (initializer->altKey())
    modifiers_ |= WebInputEvent::kAltKey;
  if (initializer->metaKey())
    modifiers_ |= WebInputEvent::kMetaKey;
  if (initializer->modifierAltGraph())
    modifiers_ |= WebInputEvent::kAltGrKey;
  if (initializer->modifierFn())
    modifiers_ |= WebInputEvent::kFnKey;
  if (initializer->modifierCapsLock())
    modifiers_ |= WebInputEvent::kCapsLockOn;
  if (initializer->modifierScrollLock())
    modifiers_ |= WebInputEvent::kScrollLockOn;
  if (initializer->modifierNumLock())
    modifiers_ |= WebInputEvent::kNumLockOn;
  if (initializer->modifierSymbol())
    modifiers_ |= WebInputEvent::kSymbolKey;
}

bool UIEventWithKeyState::new_tab_modifier_set_from_isolated_world_ = false;

void UIEventWithKeyState::DidCreateEventInIsolatedWorld(bool ctrl_key,
                                                        bool shift_key,
                                                        bool alt_key,
                                                        bool meta_key) {
#if defined(OS_MACOSX)
  const bool new_tab_modifier_set = meta_key;
#else
  const bool new_tab_modifier_set = ctrl_key;
#endif
  new_tab_modifier_set_from_isolated_world_ |= new_tab_modifier_set;
}

void UIEventWithKeyState::SetFromWebInputEventModifiers(
    EventModifierInit* initializer,
    WebInputEvent::Modifiers modifiers) {
  if (modifiers & WebInputEvent::kControlKey)
    initializer->setCtrlKey(true);
  if (modifiers & WebInputEvent::kShiftKey)
    initializer->setShiftKey(true);
  if (modifiers & WebInputEvent::kAltKey)
    initializer->setAltKey(true);
  if (modifiers & WebInputEvent::kMetaKey)
    initializer->setMetaKey(true);
  if (modifiers & WebInputEvent::kAltGrKey)
    initializer->setModifierAltGraph(true);
  if (modifiers & WebInputEvent::kFnKey)
    initializer->setModifierFn(true);
  if (modifiers & WebInputEvent::kCapsLockOn)
    initializer->setModifierCapsLock(true);
  if (modifiers & WebInputEvent::kScrollLockOn)
    initializer->setModifierScrollLock(true);
  if (modifiers & WebInputEvent::kNumLockOn)
    initializer->setModifierNumLock(true);
  if (modifiers & WebInputEvent::kSymbolKey)
    initializer->setModifierSymbol(true);
}

bool UIEventWithKeyState::getModifierState(const String& key_identifier) const {
  struct Identifier {
    const char* identifier;
    WebInputEvent::Modifiers mask;
  };
  static const Identifier kIdentifiers[] = {
      {"Shift", WebInputEvent::kShiftKey},
      {"Control", WebInputEvent::kControlKey},
      {"Alt", WebInputEvent::kAltKey},
      {"Meta", WebInputEvent::kMetaKey},
      {"AltGraph", WebInputEvent::kAltGrKey},
      {"Accel",
#if defined(OS_MACOSX)
       WebInputEvent::kMetaKey
#else
       WebInputEvent::kControlKey
#endif
      },
      {"Fn", WebInputEvent::kFnKey},
      {"CapsLock", WebInputEvent::kCapsLockOn},
      {"ScrollLock", WebInputEvent::kScrollLockOn},
      {"NumLock", WebInputEvent::kNumLockOn},
      {"Symbol", WebInputEvent::kSymbolKey},
  };
  for (const auto& identifier : kIdentifiers) {
    if (key_identifier == identifier.identifier)
      return modifiers_ & identifier.mask;
  }
  return false;
}

void UIEventWithKeyState::InitModifiers(bool ctrl_key,
                                        bool alt_key,
                                        bool shift_key,
                                        bool meta_key) {
  modifiers_ = 0;
  if (ctrl_key)
    modifiers_ |= WebInputEvent::kControlKey;
  if (alt_key)
    modifiers_ |= WebInputEvent::kAltKey;
  if (shift_key)
    modifiers_ |= WebInputEvent::kShiftKey;
  if (meta_key)
    modifiers_ |= WebInputEvent::kMetaKey;
}

UIEventWithKeyState* FindEventWithKeyState(Event* event) {
  for (Event* e = event; e; e = e->UnderlyingEvent())
    if (e->IsKeyboardEvent() || e->IsMouseEvent() || e->IsPointerEvent())
      return static_cast<UIEventWithKeyState*>(e);
  return nullptr;
}

}  // namespace blink
