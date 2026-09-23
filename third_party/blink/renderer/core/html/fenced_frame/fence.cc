// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fence.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_fenceevent_string.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

Fence::Fence(LocalDOMWindow& window) : ExecutionContextClient(&window) {}

void Fence::reportEvent(const V8UnionFenceEventOrString* event,
                        ExceptionState& exception_state) {}

void Fence::setReportEventDataForAutomaticBeacons(
    const FenceEvent* event,
    ExceptionState& exception_state) {}

HeapVector<Member<FencedFrameConfig>> Fence::getNestedConfigs(
    ExceptionState& exception_state) {
  return HeapVector<Member<FencedFrameConfig>>();
}

void Fence::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
