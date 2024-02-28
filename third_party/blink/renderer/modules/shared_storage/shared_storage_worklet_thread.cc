// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_thread.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {
class SharedStorageWorkletThreadSharedBackingThreadImpl;
}  // namespace

template class WorkletThreadHolder<
    SharedStorageWorkletThreadSharedBackingThreadImpl>;

namespace {

// Use for ref-counting of all SharedStorageWorkletThreadSharedBackingThreadImpl
// instances in a process. Incremented by the constructor and decremented by
// destructor.
int ref_count = 0;

// Owns the `WorkerBackingThread`.
class SharedStorageWorkletThreadOwningBackingThreadImpl final
    : public SharedStorageWorkletThread {
 public:
  explicit SharedStorageWorkletThreadOwningBackingThreadImpl(
      WorkerReportingProxy& worker_reporting_proxy)
      : SharedStorageWorkletThread(worker_reporting_proxy),
        worker_backing_thread_(std::make_unique<WorkerBackingThread>(
            ThreadCreationParams(GetThreadType()))) {
    CHECK(IsMainThread());

    CHECK(!base::FeatureList::IsEnabled(
        features::kSharedStorageWorkletSharedBackingThreadImplementation));
  }

  ~SharedStorageWorkletThreadOwningBackingThreadImpl() final {
    CHECK(IsMainThread());
  }

  WorkerBackingThread& GetWorkerBackingThread() final {
    return *worker_backing_thread_;
  }

 private:
  std::unique_ptr<WorkerBackingThread> worker_backing_thread_;
};

// Shares the `WorkerBackingThread` with other `SharedStorageWorkletThread`.
class SharedStorageWorkletThreadSharedBackingThreadImpl final
    : public SharedStorageWorkletThread {
 public:
  explicit SharedStorageWorkletThreadSharedBackingThreadImpl(
      WorkerReportingProxy& worker_reporting_proxy)
      : SharedStorageWorkletThread(worker_reporting_proxy) {
    CHECK(IsMainThread());

    CHECK(base::FeatureList::IsEnabled(
        features::kSharedStorageWorkletSharedBackingThreadImplementation));

    if (++ref_count == 1) {
      WorkletThreadHolder<SharedStorageWorkletThreadSharedBackingThreadImpl>::
          EnsureInstance(ThreadCreationParams(GetThreadType()));
    }
  }

  ~SharedStorageWorkletThreadSharedBackingThreadImpl() final {
    CHECK(IsMainThread());

    if (--ref_count == 0) {
      WorkletThreadHolder<
          SharedStorageWorkletThreadSharedBackingThreadImpl>::ClearInstance();
    }
  }

  WorkerBackingThread& GetWorkerBackingThread() final {
    return *WorkletThreadHolder<
                SharedStorageWorkletThreadSharedBackingThreadImpl>::
                GetInstance()
                    ->GetThread();
  }

 private:
  bool IsOwningBackingThread() const final { return false; }
};

}  // namespace

// static
std::unique_ptr<SharedStorageWorkletThread> SharedStorageWorkletThread::Create(
    WorkerReportingProxy& worker_reporting_proxy) {
  CHECK(IsMainThread());

  if (base::FeatureList::IsEnabled(
          features::kSharedStorageWorkletSharedBackingThreadImplementation)) {
    return std::make_unique<SharedStorageWorkletThreadSharedBackingThreadImpl>(
        worker_reporting_proxy);
  }

  return std::make_unique<SharedStorageWorkletThreadOwningBackingThreadImpl>(
      worker_reporting_proxy);
}

SharedStorageWorkletThread::~SharedStorageWorkletThread() = default;

void SharedStorageWorkletThread::InitializeSharedStorageWorkletService(
    mojo::PendingReceiver<mojom::blink::SharedStorageWorkletService> receiver,
    base::OnceClosure disconnect_handler) {
  SharedStorageWorkletGlobalScope* global_scope =
      To<SharedStorageWorkletGlobalScope>(GlobalScope());

  global_scope->BindSharedStorageWorkletService(std::move(receiver),
                                                std::move(disconnect_handler));
}

// static
std::optional<WorkerBackingThreadStartupData>
SharedStorageWorkletThread::CreateThreadStartupData() {
  if (base::FeatureList::IsEnabled(
          features::kSharedStorageWorkletSharedBackingThreadImplementation)) {
    return std::nullopt;
  }

  // The owning-backing-thread-implementation needs to provide a
  // `WorkerBackingThreadStartupData`.
  auto thread_startup_data = WorkerBackingThreadStartupData::CreateDefault();
  thread_startup_data.atomics_wait_mode =
      WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow;
  return thread_startup_data;
}

SharedStorageWorkletThread::SharedStorageWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy) {}

WorkerOrWorkletGlobalScope* SharedStorageWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  return MakeGarbageCollected<SharedStorageWorkletGlobalScope>(
      std::move(creation_params), this);
}

}  // namespace blink
