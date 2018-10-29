// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/execution_context_worker_registry.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"

namespace blink {

const char ExecutionContextWorkerRegistry::kSupplementName[] =
    "ExecutionContextWorkerRegistry";

ExecutionContextWorkerRegistry::ExecutionContextWorkerRegistry(
    ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {}

ExecutionContextWorkerRegistry::~ExecutionContextWorkerRegistry() {
}

ExecutionContextWorkerRegistry* ExecutionContextWorkerRegistry::From(
    ExecutionContext& context) {
  DCHECK(context.IsContextThread());
  ExecutionContextWorkerRegistry* worker_registry =
      Supplement<ExecutionContext>::From<ExecutionContextWorkerRegistry>(
          context);
  if (!worker_registry) {
    worker_registry = new ExecutionContextWorkerRegistry(context);
    Supplement<ExecutionContext>::ProvideTo(context, worker_registry);
  }
  return worker_registry;
}

void ExecutionContextWorkerRegistry::AddWorkerInspectorProxy(
    WorkerInspectorProxy* proxy) {
  proxies_.insert(proxy);
}

void ExecutionContextWorkerRegistry::RemoveWorkerInspectorProxy(
    WorkerInspectorProxy* proxy) {
  proxies_.erase(proxy);
}

const HeapHashSet<Member<WorkerInspectorProxy>>&
ExecutionContextWorkerRegistry::GetWorkerInspectorProxies() {
  return proxies_;
}

void ExecutionContextWorkerRegistry::Trace(Visitor* visitor) {
  visitor->Trace(proxies_);
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
