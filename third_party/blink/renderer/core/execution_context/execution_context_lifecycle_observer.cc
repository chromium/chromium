// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

ExecutionContextClient::ExecutionContextClient(
    ExecutionContext* execution_context)
    : execution_context_(execution_context) {}

ExecutionContext* ExecutionContextClient::GetExecutionContext() const {
  return execution_context_ && !execution_context_->IsContextDestroyed()
             ? execution_context_.Get()
             : nullptr;
}

LocalDOMWindow* ExecutionContextClient::DomWindow() const {
  return DynamicTo<LocalDOMWindow>(GetExecutionContext());
}

void ExecutionContextClient::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
}

ExecutionContextLifecycleObserver::ExecutionContextLifecycleObserver(
    ExecutionContext* execution_context,
    Type type)
    : observer_type_(type) {
  SetExecutionContext(execution_context);
}

ExecutionContext* ExecutionContextLifecycleObserver::GetExecutionContext()
    const {
  return static_cast<ExecutionContext*>(GetContextLifecycleNotifier());
}

void ExecutionContextLifecycleObserver::SetExecutionContext(
    ExecutionContext* execution_context) {
  SetContextLifecycleNotifier(execution_context);
}

LocalDOMWindow* ExecutionContextLifecycleObserver::DomWindow() const {
  return DynamicTo<LocalDOMWindow>(GetExecutionContext());
}

void ExecutionContextLifecycleObserver::Trace(Visitor* visitor) const {
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
