// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/captured_mouse_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_captured_mouse_event_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

namespace {

// Coordinates are valid if they are non-negative, or if both of them are equal
// to -1 (meaning that the mouse cursor is not over the captured surface).
bool AreSurfaceCoordinatesValid(const CapturedMouseEventInit& initializer) {
  if (!initializer.hasSurfaceX() || !initializer.hasSurfaceY()) {
    return false;
  }
  return (initializer.surfaceX() >= 0 && initializer.surfaceY() >= 0) ||
         (initializer.surfaceX() == -1 && initializer.surfaceY() == -1);
}

}  // namespace

// static
CapturedMouseEvent* CapturedMouseEvent::Create(
    const AtomicString& type,
    const CapturedMouseEventInit* initializer,
    ExceptionState& exception_state) {
  CHECK(initializer);
  if (!AreSurfaceCoordinatesValid(*initializer)) {
    exception_state.ThrowRangeError(
        "surfaceX and surfaceY must both be non-negative, or both of them "
        "must be equal to -1.");
    return nullptr;
  }
  return MakeGarbageCollected<CapturedMouseEvent>(type, initializer);
}

const AtomicString& CapturedMouseEvent::InterfaceName() const {
  return event_interface_names::kCapturedMouseEvent;
}

CapturedMouseEvent::CapturedMouseEvent(
    const AtomicString& type,
    const CapturedMouseEventInit* initializer)
    : Event(type, initializer),
      surface_coordinates_(initializer->surfaceX(), initializer->surfaceY()) {
  CHECK(initializer);
  CHECK(AreSurfaceCoordinatesValid(*initializer));
}

}  // namespace blink
