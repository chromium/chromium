// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/thread_pool_thread.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/experimental/task_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_object_proxy_base.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {

namespace {

class ThreadPoolWorkerGlobalScope final : public WorkerGlobalScope {
 public:
  ThreadPoolWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params,
      WorkerThread* thread)
      : WorkerGlobalScope(std::move(creation_params),
                          thread,
                          CurrentTimeTicks()) {}

  ~ThreadPoolWorkerGlobalScope() override = default;

  // EventTarget
  const AtomicString& InterfaceName() const override {
    // TODO(japhet): Replaces this with
    // EventTargetNames::ThreadPoolWorkerGlobalScope.
    return EventTargetNames::DedicatedWorkerGlobalScope;
  }

  // WorkerGlobalScope
  void ImportModuleScript(
      const KURL& module_url_record,
      FetchClientSettingsObjectSnapshot* outside_settings_object,
      network::mojom::FetchCredentialsMode) override {
    // TODO(japhet): Consider whether modules should be supported.
    NOTREACHED();
  }

  void ExceptionThrown(ErrorEvent*) override {}
};

}  // anonymous namespace

ThreadPoolThread::ThreadPoolThread(ExecutionContext* parent_execution_context,
                                   ThreadedObjectProxyBase& object_proxy,
                                   ThreadBackingPolicy backing_policy)
    : WorkerThread(object_proxy), backing_policy_(backing_policy) {
  DCHECK(parent_execution_context);
  worker_backing_thread_ = WorkerBackingThread::Create(
      ThreadCreationParams(GetThreadType())
          .SetFrameOrWorkerScheduler(parent_execution_context->GetScheduler()));
}

WorkerOrWorkletGlobalScope* ThreadPoolThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  if (backing_policy_ == kWorker)
    return new ThreadPoolWorkerGlobalScope(std::move(creation_params), this);
  return new TaskWorkletGlobalScope(std::move(creation_params), this);
}

}  // namespace blink
