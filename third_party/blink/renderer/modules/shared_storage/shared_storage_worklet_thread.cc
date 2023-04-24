// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_thread.h"

#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// Use for ref-counting of all SharedStorageWorkletThread instances in a
// process. Incremented by the constructor and decremented by destructor.
int ref_count = 0;

void EnsureSharedBackingThread(const ThreadCreationParams& params) {
  DCHECK(IsMainThread());
  DCHECK_EQ(ref_count, 1);
  WorkletThreadHolder<SharedStorageWorkletThread>::EnsureInstance(params);
}

}  // namespace

template class WorkletThreadHolder<SharedStorageWorkletThread>;

SharedStorageWorkletThread::SharedStorageWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy) {
  DCHECK(IsMainThread());

  // TODO(crbug.com/1414951): Specify a correct type.
  ThreadCreationParams params =
      ThreadCreationParams(ThreadType::kUnspecifiedWorkerThread);

  if (++ref_count == 1) {
    EnsureSharedBackingThread(params);
  }
}

SharedStorageWorkletThread::~SharedStorageWorkletThread() {
  DCHECK(IsMainThread());
  if (--ref_count == 0) {
    ClearSharedBackingThread();
  }
}

WorkerBackingThread& SharedStorageWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<SharedStorageWorkletThread>::GetInstance()
              ->GetThread();
}

void SharedStorageWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  CHECK_EQ(ref_count, 0);
  WorkletThreadHolder<SharedStorageWorkletThread>::ClearInstance();
}

void SharedStorageWorkletThread::InitializeSharedStorageWorkletService(
    mojo::PendingReceiver<mojom::blink::SharedStorageWorkletService> receiver,
    base::OnceClosure disconnect_handler) {
  DCHECK(!IsMainThread());
  SharedStorageWorkletGlobalScope* global_scope =
      To<SharedStorageWorkletGlobalScope>(GlobalScope());

  global_scope->BindSharedStorageWorkletService(std::move(receiver),
                                                std::move(disconnect_handler));
}

WorkerOrWorkletGlobalScope* SharedStorageWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  return MakeGarbageCollected<SharedStorageWorkletGlobalScope>(
      std::move(creation_params), this);
}

}  // namespace blink
