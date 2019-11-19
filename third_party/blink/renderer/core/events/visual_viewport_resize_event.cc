// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/visual_viewport_resize_event.h"

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

VisualViewportResizeEvent::~VisualViewportResizeEvent() = default;

VisualViewportResizeEvent::VisualViewportResizeEvent()
    : Event(event_type_names::kResize, Bubbles::kNo, Cancelable::kNo) {}

void VisualViewportResizeEvent::DoneDispatchingEventAtCurrentTarget() {
  UseCounter::Count(currentTarget()->GetExecutionContext(),
                    WebFeature::kVisualViewportResizeFired);
}

}  // namespace blink
