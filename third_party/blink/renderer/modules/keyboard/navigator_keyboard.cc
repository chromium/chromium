// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/keyboard/navigator_keyboard.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/keyboard/keyboard.h"

namespace blink {

// static
const char NavigatorKeyboard::kSupplementName[] = "NavigatorKeyboard";

NavigatorKeyboard::NavigatorKeyboard(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      keyboard_(
          MakeGarbageCollected<Keyboard>(GetSupplementable()->DomWindow())) {}

// static
Keyboard* NavigatorKeyboard::keyboard(Navigator& navigator) {
  NavigatorKeyboard* supplement =
      Supplement<Navigator>::From<NavigatorKeyboard>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorKeyboard>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement->keyboard_.Get();
}

void NavigatorKeyboard::Trace(Visitor* visitor) const {
  visitor->Trace(keyboard_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
