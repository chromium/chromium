// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard_geometry_change_event.h"

namespace blink {

VirtualKeyboardGeometryChangeEvent* VirtualKeyboardGeometryChangeEvent::Create(
    const AtomicString& type) {
  return MakeGarbageCollected<VirtualKeyboardGeometryChangeEvent>(type);
}

VirtualKeyboardGeometryChangeEvent::VirtualKeyboardGeometryChangeEvent(
    const AtomicString& type)
    : Event(type, Bubbles::kNo, Cancelable::kNo) {}

}  // namespace blink
