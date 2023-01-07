// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/visual_viewport_scroll_event.h"

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

VisualViewportScrollEvent::~VisualViewportScrollEvent() = default;

VisualViewportScrollEvent::VisualViewportScrollEvent()
    : Event(event_type_names::kScroll, Bubbles::kNo, Cancelable::kNo) {}

void VisualViewportScrollEvent::DoneDispatchingEventAtCurrentTarget() {
  UseCounter::Count(currentTarget()->GetExecutionContext(),
                    WebFeature::kVisualViewportScrollFired);
}

}  // namespace blink
