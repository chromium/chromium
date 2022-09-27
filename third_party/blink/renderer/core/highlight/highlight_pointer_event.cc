// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_pointer_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_highlight_pointer_event_init.h"

namespace blink {

HighlightPointerEvent::HighlightPointerEvent(
    const AtomicString& type,
    const HighlightPointerEventInit* initializer,
    base::TimeTicks platform_time_stamp,
    MouseEvent::SyntheticEventType synthetic_event_type,
    WebMenuSourceType menu_source_type)
    : PointerEvent(type,
                   initializer,
                   platform_time_stamp,
                   synthetic_event_type,
                   menu_source_type) {}

bool HighlightPointerEvent::IsHighlightPointerEvent() const {
  return true;
}

void HighlightPointerEvent::Trace(blink::Visitor* visitor) const {
  visitor->Trace(range_);
  PointerEvent::Trace(visitor);
}

}  // namespace blink
