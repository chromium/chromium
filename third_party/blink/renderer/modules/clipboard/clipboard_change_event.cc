// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_change_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_change_event_init.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {
ClipboardChangeEvent::ClipboardChangeEvent(const Vector<String>& types)
    : Event(event_type_names::kClipboardchange, Bubbles::kNo, Cancelable::kNo),
      types_(types) {}

ClipboardChangeEvent::ClipboardChangeEvent(
    const ClipboardChangeEventInit* initializer)
    : Event(event_type_names::kClipboardchange, initializer),
      types_(initializer->types()) {}

ClipboardChangeEvent::~ClipboardChangeEvent() = default;

Vector<String> ClipboardChangeEvent::types() const {
  return types_;
}

void ClipboardChangeEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
