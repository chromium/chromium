// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/window_controls_overlay/window_controls_overlay.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/window_controls_overlay/window_controls_overlay_geometry_change_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
const char WindowControlsOverlay::kSupplementName[] = "WindowControlsOverlay";

// static
WindowControlsOverlay& WindowControlsOverlay::From(Navigator& navigator) {
  WindowControlsOverlay* supplement = FromIfExists(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<WindowControlsOverlay>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
WindowControlsOverlay* WindowControlsOverlay::FromIfExists(
    Navigator& navigator) {
  return Supplement<Navigator>::From<WindowControlsOverlay>(navigator);
}

// static
WindowControlsOverlay* WindowControlsOverlay::windowControlsOverlay(
    Navigator& navigator) {
  return &From(navigator);
}

WindowControlsOverlay::WindowControlsOverlay(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      WindowControlsOverlayChangedDelegate(
          navigator.DomWindow() ? navigator.DomWindow()->GetFrame() : nullptr) {
}

WindowControlsOverlay::~WindowControlsOverlay() = default;

ExecutionContext* WindowControlsOverlay::GetExecutionContext() const {
  return GetSupplementable()->DomWindow();
}

const AtomicString& WindowControlsOverlay::InterfaceName() const {
  return event_target_names::kWindowControlsOverlay;
}

bool WindowControlsOverlay::visible() const {
  if (!GetSupplementable()->DomWindow())
    return false;

  return GetSupplementable()
      ->DomWindow()
      ->GetFrame()
      ->IsWindowControlsOverlayVisible();
}

DOMRect* WindowControlsOverlay::getTitlebarAreaRect() const {
  if (!GetSupplementable()->DomWindow())
    return DOMRect::Create(0, 0, 0, 0);

  const auto& rect = GetSupplementable()
                         ->DomWindow()
                         ->GetFrame()
                         ->GetWindowControlsOverlayRect();
  return DOMRect::Create(rect.x(), rect.y(), rect.width(), rect.height());
}

void WindowControlsOverlay::WindowControlsOverlayChanged(
    const gfx::Rect& rect) {
  DispatchEvent(
      *(MakeGarbageCollected<WindowControlsOverlayGeometryChangeEvent>(
          event_type_names::kGeometrychange,
          DOMRect::Create(rect.x(), rect.y(), rect.width(), rect.height()),
          !rect.IsEmpty())));
}

void WindowControlsOverlay::Trace(blink::Visitor* visitor) const {
  EventTarget::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
