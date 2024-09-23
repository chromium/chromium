// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/window_popin.h"

#include <optional>

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

const char WindowPopin::kSupplementName[] = "WindowPopin";

WindowPopin::WindowPopin(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void WindowPopin::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

WindowPopin& WindowPopin::From(LocalDOMWindow& window) {
  WindowPopin* supplement =
      Supplement<LocalDOMWindow>::From<WindowPopin>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<WindowPopin>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

Vector<V8PopinContextType> WindowPopin::popinContextTypesSupported(
    LocalDOMWindow& window) {
  return From(window).popinContextTypesSupported();
}

Vector<V8PopinContextType> WindowPopin::popinContextTypesSupported() {
  Vector<V8PopinContextType> out;
  LocalDOMWindow* const window = GetSupplementable();
  LocalFrame* const frame = window->GetFrame();
  if (!frame) {
    return out;
  }

  if (!frame->GetPage()->IsPartitionedPopin() &&
      frame->GetSecurityContext()->GetSecurityOrigin()->Protocol() == "https") {
    out.push_back(V8PopinContextType(V8PopinContextType::Enum::kPartitioned));
  }
  return out;
}

std::optional<V8PopinContextType> WindowPopin::popinContextType(
    LocalDOMWindow& window) {
  return From(window).popinContextType();
}

std::optional<V8PopinContextType> WindowPopin::popinContextType() {
  LocalDOMWindow* const window = GetSupplementable();
  LocalFrame* const frame = window->GetFrame();
  if (!frame) {
    return std::nullopt;
  }

  if (frame->GetPage()->IsPartitionedPopin()) {
    return V8PopinContextType(V8PopinContextType::Enum::kPartitioned);
  }
  return std::nullopt;
}

}  // namespace blink
