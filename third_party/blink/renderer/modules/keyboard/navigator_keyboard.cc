// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/keyboard/navigator_keyboard.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/keyboard/keyboard.h"

namespace blink {

NavigatorKeyboard::NavigatorKeyboard(Navigator& navigator)
    : keyboard_(MakeGarbageCollected<Keyboard>(navigator.DomWindow())) {}

// static
Keyboard* NavigatorKeyboard::keyboard(Navigator& navigator) {
  NavigatorKeyboard* supplement = navigator.GetNavigatorKeyboard();
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorKeyboard>(navigator);
    navigator.SetNavigatorKeyboard(supplement);
  }
  return supplement->keyboard_.Get();
}

void NavigatorKeyboard::Trace(Visitor* visitor) const {
  visitor->Trace(keyboard_);
}

}  // namespace blink
