// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"

#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

const AtomicString& EventTargetImpl::InterfaceName() const {
  return event_target_names::kEventTargetImpl;
}

ExecutionContext* EventTargetImpl::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void EventTargetImpl::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

EventTargetImpl::EventTargetImpl(ScriptState* script_state)
    : ExecutionContextClient(ExecutionContext::From(script_state)) {}

}  // namespace blink
