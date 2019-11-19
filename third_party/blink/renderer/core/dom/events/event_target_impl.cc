// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"

namespace blink {

const AtomicString& EventTargetImpl::InterfaceName() const {
  return event_target_names::kEventTargetImpl;
}

ExecutionContext* EventTargetImpl::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void EventTargetImpl::Trace(Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

EventTargetImpl::EventTargetImpl(ScriptState* script_state)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)) {}

}  // namespace blink
