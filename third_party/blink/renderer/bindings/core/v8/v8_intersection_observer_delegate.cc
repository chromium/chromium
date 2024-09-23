// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_delegate.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_callback.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"

namespace blink {

V8IntersectionObserverDelegate::V8IntersectionObserverDelegate(
    V8IntersectionObserverCallback* callback,
    ScriptState* script_state)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      callback_(callback) {}

V8IntersectionObserverDelegate::~V8IntersectionObserverDelegate() = default;

void V8IntersectionObserverDelegate::Deliver(
    const HeapVector<Member<IntersectionObserverEntry>>& entries,
    IntersectionObserver& observer) {
  callback_->InvokeAndReportException(&observer, entries, &observer);
}

ExecutionContext* V8IntersectionObserverDelegate::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void V8IntersectionObserverDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(callback_);
  IntersectionObserverDelegate::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
