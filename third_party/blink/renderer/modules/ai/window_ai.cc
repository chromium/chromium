// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/window_ai.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

WindowAI::WindowAI(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void WindowAI::Trace(Visitor* visitor) const {
  visitor->Trace(ai_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char WindowAI::kSupplementName[] = "WindowAI";

// static
WindowAI& WindowAI::From(LocalDOMWindow& window) {
  WindowAI* supplement = Supplement<LocalDOMWindow>::From<WindowAI>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<WindowAI>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

// static
AI* WindowAI::ai(LocalDOMWindow& window) {
  return From(window).ai();
}

AI* WindowAI::ai() {
  if (!ai_) {
    ai_ = MakeGarbageCollected<AI>(GetSupplementable());
  }
  return ai_.Get();
}

}  // namespace blink
