// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_signal_registry.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

namespace blink {

// static
const char AbortSignalRegistry::kSupplementName[] = "AbortSignalRegistry";

// static
AbortSignalRegistry* AbortSignalRegistry::From(ExecutionContext& context) {
  AbortSignalRegistry* registry =
      Supplement<ExecutionContext>::From<AbortSignalRegistry>(context);
  if (!registry) {
    registry = MakeGarbageCollected<AbortSignalRegistry>(context);
    Supplement<ExecutionContext>::ProvideTo(context, registry);
  }
  return registry;
}

AbortSignalRegistry::AbortSignalRegistry(ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      ExecutionContextLifecycleObserver(&context) {}

AbortSignalRegistry::~AbortSignalRegistry() = default;

void AbortSignalRegistry::Trace(Visitor* visitor) const {
  visitor->Trace(event_listener_signals_);
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void AbortSignalRegistry::ContextDestroyed() {
  event_listener_signals_.clear();
}

void AbortSignalRegistry::RegisterAbortAlgorithm(
    EventListener* listener,
    AbortSignal::AlgorithmHandle* handle) {
  if (!base::FeatureList::IsEnabled(features::kAbortSignalHandleBasedRemoval)) {
    return;
  }
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    return;
  }
  event_listener_signals_.Set(listener, handle);
}

}  // namespace blink
