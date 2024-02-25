// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/window_controls_overlay/window_controls_overlay_geometry_change_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_window_controls_overlay_geometry_change_event_init.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

WindowControlsOverlayGeometryChangeEvent*
WindowControlsOverlayGeometryChangeEvent::Create(
    const AtomicString& type,
    const WindowControlsOverlayGeometryChangeEventInit* initializer) {
  return MakeGarbageCollected<WindowControlsOverlayGeometryChangeEvent>(
      type, initializer);
}

WindowControlsOverlayGeometryChangeEvent::
    WindowControlsOverlayGeometryChangeEvent(
        const AtomicString& type,
        const WindowControlsOverlayGeometryChangeEventInit* initializer)
    : Event(type, initializer) {}

WindowControlsOverlayGeometryChangeEvent::
    WindowControlsOverlayGeometryChangeEvent(const AtomicString& type,
                                             DOMRect* rect,
                                             bool visible)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      bounding_rect_(rect),
      visible_(visible) {}

DOMRect* WindowControlsOverlayGeometryChangeEvent::titlebarAreaRect() const {
  return bounding_rect_.Get();
}

bool WindowControlsOverlayGeometryChangeEvent::visible() const {
  return visible_;
}

void WindowControlsOverlayGeometryChangeEvent::Trace(Visitor* visitor) const {
  visitor->Trace(bounding_rect_);
  Event::Trace(visitor);
}

}  // namespace blink
