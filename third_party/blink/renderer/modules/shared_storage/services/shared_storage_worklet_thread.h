// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SERVICES_SHARED_STORAGE_WORKLET_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SERVICES_SHARED_STORAGE_WORKLET_THREAD_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-forward.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class WorkerReportingProxy;
class SharedStorageWorkletServiceImpl;

// SharedStorageWorkletThread is a per-SharedStorageWorkletGlobalScope object
// that has a reference count to the backing thread that performs
// SharedStorageWorklet tasks.
class MODULES_EXPORT SharedStorageWorkletThread final : public WorkerThread {
 public:
  explicit SharedStorageWorkletThread(WorkerReportingProxy&);
  ~SharedStorageWorkletThread() final;

  void SharedStorageWorkletServiceConnectionError();

  WorkerBackingThread& GetWorkerBackingThread() final;
  void ClearWorkerBackingThread() final {}

  static void ClearSharedBackingThread();

  void InitializeSharedStorageWorkletService(
      mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver,
      base::OnceClosure disconnect_handler);

 private:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>) final;
  bool IsOwningBackingThread() const final { return false; }
  ThreadType GetThreadType() const final {
    // TODO(crbug.com/1414951): Specify a correct type.
    return ThreadType::kUnspecifiedWorkerThread;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SERVICES_SHARED_STORAGE_WORKLET_THREAD_H_
