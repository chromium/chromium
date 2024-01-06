// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_thread.h"

#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

SharedStorageWorkletThread::SharedStorageWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy),
      worker_backing_thread_(std::make_unique<WorkerBackingThread>(
          ThreadCreationParams(GetThreadType()))) {}

SharedStorageWorkletThread::~SharedStorageWorkletThread() = default;

void SharedStorageWorkletThread::ClearWorkerBackingThread() {
  worker_backing_thread_ = nullptr;
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
