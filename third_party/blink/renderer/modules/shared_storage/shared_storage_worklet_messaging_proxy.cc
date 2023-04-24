// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_messaging_proxy.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

SharedStorageWorkletMessagingProxy::SharedStorageWorkletMessagingProxy(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    mojo::PendingReceiver<mojom::blink::SharedStorageWorkletService> receiver,
    base::OnceClosure worklet_terminated_callback)
    : ThreadedWorkletMessagingProxy(
          /*execution_context=*/nullptr,
          /*parent_agent_group_task_runner=*/main_thread_runner),
      main_thread_runner_(main_thread_runner),
      worklet_terminated_callback_(std::move(worklet_terminated_callback)) {
  DCHECK(IsMainThread());

  Initialize(/*worker_clients=*/nullptr, /*module_responses_map=*/nullptr);

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kMiscPlatformAPI), FROM_HERE,
      CrossThreadBindOnce(
          &SharedStorageWorkletMessagingProxy::
              InitializeSharedStorageWorkletServiceOnWorkletThread,
          WrapCrossThreadPersistent(this),
          CrossThreadUnretained(GetWorkerThread()), std::move(receiver)));
}

void SharedStorageWorkletMessagingProxy::
    InitializeSharedStorageWorkletServiceOnWorkletThread(
        WorkerThread* worker_thread,
        mojo::PendingReceiver<mojom::blink::SharedStorageWorkletService>
            receiver) {
  DCHECK(worker_thread->IsCurrentThread());

  auto disconnect_handler = WTF::BindOnce(
      &SharedStorageWorkletMessagingProxy::
          OnSharedStorageWorkletServiceDisconnectedOnWorkletThread,
      WrapCrossThreadPersistent(this));

  static_cast<SharedStorageWorkletThread*>(worker_thread)
      ->InitializeSharedStorageWorkletService(std::move(receiver),
                                              std::move(disconnect_handler));
}

void SharedStorageWorkletMessagingProxy::
    OnSharedStorageWorkletServiceDisconnectedOnWorkletThread() {
  // Initiate worklet termination from the main thread. This will eventually
  // trigger `WorkerThreadTerminated()`.
  PostCrossThreadTask(
      *main_thread_runner_, FROM_HERE,
      CrossThreadBindOnce(&ThreadedMessagingProxyBase::ParentObjectDestroyed,
                          WrapCrossThreadPersistent(this)));
}

void SharedStorageWorkletMessagingProxy::WorkerThreadTerminated() {
  DCHECK(IsMainThread());

  ThreadedWorkletMessagingProxy::WorkerThreadTerminated();

  // This will destroy the `WebSharedStorageWorkletThreadImpl` that owns `this`.
  std::move(worklet_terminated_callback_).Run();
}

void SharedStorageWorkletMessagingProxy::Trace(Visitor* visitor) const {
  ThreadedWorkletMessagingProxy::Trace(visitor);
}

std::unique_ptr<WorkerThread>
SharedStorageWorkletMessagingProxy::CreateWorkerThread() {
  DCHECK(IsMainThread());

  return std::make_unique<SharedStorageWorkletThread>(WorkletObjectProxy());
}

}  // namespace blink
