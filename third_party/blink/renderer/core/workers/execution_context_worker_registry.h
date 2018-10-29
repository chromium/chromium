// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXECUTION_CONTEXT_WORKER_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXECUTION_CONTEXT_WORKER_REGISTRY_H_

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_inspector_proxy.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

// Tracks the WorkerInspectorProxy objects created by a given ExecutionContext.
class ExecutionContextWorkerRegistry final
    : public GarbageCollectedFinalized<ExecutionContextWorkerRegistry>,
      public Supplement<ExecutionContext> {
  USING_GARBAGE_COLLECTED_MIXIN(ExecutionContextWorkerRegistry);

 public:
  static const char kSupplementName[];

  static ExecutionContextWorkerRegistry* From(ExecutionContext& context);

  ~ExecutionContextWorkerRegistry();

  void AddWorkerInspectorProxy(WorkerInspectorProxy* proxy);
  void RemoveWorkerInspectorProxy(WorkerInspectorProxy* proxy);
  const HeapHashSet<Member<WorkerInspectorProxy>>& GetWorkerInspectorProxies();

  void Trace(Visitor* visitor) override;

 private:
  explicit ExecutionContextWorkerRegistry(ExecutionContext& context);

  HeapHashSet<Member<WorkerInspectorProxy>> proxies_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXECUTION_CONTEXT_WORKER_REGISTRY_H_
