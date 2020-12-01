// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/window_controls_overlay.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// static
const char WindowControlsOverlay::kSupplementName[] = "WindowControlsOverlay";

// static
WindowControlsOverlay* WindowControlsOverlay::windowControlsOverlay(
    Navigator& navigator) {
  auto* supplement =
      Supplement<Navigator>::From<WindowControlsOverlay>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<WindowControlsOverlay>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

WindowControlsOverlay::WindowControlsOverlay(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

WindowControlsOverlay::~WindowControlsOverlay() = default;

bool WindowControlsOverlay::visible() const {
  if (!GetSupplementable()->DomWindow()->GetFrame())
    return false;
  // TODO(crbug.com/937121): Replace the hardcoded value in the next javascript
  // API CL.
  return false;
}

DOMRect* WindowControlsOverlay::getBoundingClientRect() const {
  if (!GetSupplementable()->DomWindow()->GetFrame())
    return DOMRect::Create(0, 0, 0, 0);
  // TODO(crbug.com/937121): Replace the hardcoded value in the next javascript
  // API CL.
  return DOMRect::Create(0, 0, 0, 0);
}

void WindowControlsOverlay::Trace(blink::Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
