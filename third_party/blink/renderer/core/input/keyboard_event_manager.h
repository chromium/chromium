// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_KEYBOARD_EVENT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_KEYBOARD_EVENT_MANAGER_H_

#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class KeyboardEvent;
class LocalFrame;
class ScrollManager;
class WebKeyboardEvent;

enum class OverrideCapsLockState { kDefault, kOn, kOff };

class CORE_EXPORT KeyboardEventManager final
    : public GarbageCollected<KeyboardEventManager> {
 public:
  static const int kAccessKeyModifiers =
// TODO(crbug.com/618397): Add a settings to control this behavior.
#if defined(OS_MAC)
      WebInputEvent::kControlKey | WebInputEvent::kAltKey;
#else
      WebInputEvent::kAltKey;
#endif

  KeyboardEventManager(LocalFrame&, ScrollManager&);
  KeyboardEventManager(const KeyboardEventManager&) = delete;
  KeyboardEventManager& operator=(const KeyboardEventManager&) = delete;
  void Trace(Visitor*) const;

  bool HandleAccessKey(const WebKeyboardEvent&);
  WebInputEventResult KeyEvent(const WebKeyboardEvent&);
  void DefaultKeyboardEventHandler(KeyboardEvent*, Node*);

  void CapsLockStateMayHaveChanged();
  static WebInputEvent::Modifiers GetCurrentModifierState();
  static bool CurrentCapsLockState();

 private:
  friend class Internals;
  // Allows overriding the current caps lock state for testing purposes.
  static void SetCurrentCapsLockState(OverrideCapsLockState);

  void DefaultSpaceEventHandler(KeyboardEvent*, Node*);
  void DefaultBackspaceEventHandler(KeyboardEvent*);
  void DefaultTabEventHandler(KeyboardEvent*);
  void DefaultEscapeEventHandler(KeyboardEvent*);
  void DefaultEnterEventHandler(KeyboardEvent*);
  void DefaultImeSubmitHandler(KeyboardEvent*);
  void DefaultArrowEventHandler(KeyboardEvent*, Node*);

  const Member<LocalFrame> frame_;

  Member<ScrollManager> scroll_manager_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_KEYBOARD_EVENT_MANAGER_H_
