// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/ready_to_render_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ReadyToRenderEvent::ReadyToRenderEvent()
    : Event(event_type_names::kReadytorender, Bubbles::kNo, Cancelable::kNo) {
  CHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
}

ReadyToRenderEvent::~ReadyToRenderEvent() = default;

const AtomicString& ReadyToRenderEvent::InterfaceName() const {
  return event_interface_names::kReadyToRenderEvent;
}

void ReadyToRenderEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
