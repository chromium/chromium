// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/eyedropper/color_select_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_color_select_event_init.h"

namespace blink {

ColorSelectEvent::ColorSelectEvent(const AtomicString& type,
                                   const ColorSelectEventInit* initializer,
                                   base::TimeTicks platform_time_stamp,
                                   SyntheticEventType synthetic_event_type,
                                   WebMenuSourceType menu_source_type)
    : PointerEvent(type,
                   initializer,
                   platform_time_stamp,
                   synthetic_event_type,
                   menu_source_type),
      value_(initializer->value()) {}

ColorSelectEvent* ColorSelectEvent::Create(
    const AtomicString& type,
    const ColorSelectEventInit* initializer,
    base::TimeTicks platform_time_stamp,
    SyntheticEventType synthetic_event_type,
    WebMenuSourceType menu_source_type) {
  return MakeGarbageCollected<ColorSelectEvent>(
      type, initializer, platform_time_stamp, synthetic_event_type,
      menu_source_type);
}

String ColorSelectEvent::value() const {
  return value_;
}

void ColorSelectEvent::Trace(Visitor* visitor) const {
  PointerEvent::Trace(visitor);
}

}  // namespace blink
