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
WindowControlsOverlay& WindowControlsOverlay::From(Navigator& navigator) {
  WindowControlsOverlay* supplement = FromIfExists(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<WindowControlsOverlay>(navigator);
    navigator.SetWindowControlsOverlay(supplement);
  }
  return *supplement;
}

// static
WindowControlsOverlay* WindowControlsOverlay::FromIfExists(
    Navigator& navigator) {
  return navigator.GetWindowControlsOverlay();
}

// static
WindowControlsOverlay* WindowControlsOverlay::windowControlsOverlay(
    Navigator& navigator) {
  return &From(navigator);
}

WindowControlsOverlay::WindowControlsOverlay(Navigator& navigator)
    : WindowControlsOverlayChangedDelegate(
          navigator.DomWindow() ? navigator.DomWindow()->GetFrame() : nullptr),
      navigator_(navigator) {}

WindowControlsOverlay::~WindowControlsOverlay() = default;

ExecutionContext* WindowControlsOverlay::GetExecutionContext() const {
  return navigator_->DomWindow();
}

const AtomicString& WindowControlsOverlay::InterfaceName() const {
  return event_target_names::kWindowControlsOverlay;
}

bool WindowControlsOverlay::visible() const {
  if (!navigator_->DomWindow()) {
    return false;
  }

  return navigator_->DomWindow()->GetFrame()->IsWindowControlsOverlayVisible();
}

DOMRect* WindowControlsOverlay::getTitlebarAreaRect() const {
  if (!navigator_->DomWindow()) {
    return DOMRect::Create(0, 0, 0, 0);
  }

  const auto& rect =
      navigator_->DomWindow()->GetFrame()->GetWindowControlsOverlayRect();
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
  visitor->Trace(navigator_);
}

}  // namespace blink
