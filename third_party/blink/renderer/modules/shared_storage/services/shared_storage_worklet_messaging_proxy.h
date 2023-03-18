// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SERVICES_SHARED_STORAGE_WORKLET_MESSAGING_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SERVICES_SHARED_STORAGE_WORKLET_MESSAGING_PROXY_H_

#include <memory>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-forward.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class WorkerThread;

// Acts as a proxy for the shared storage worklet global scope. Upon creation on
// the main thread, this will post a task to the worklet thread to initialize
// the `SharedStorageWorkletGlobalScope`. When the service is disconnected on
// the worklet thread (triggering
// `OnSharedStorageWorkletServiceDisconnectedOnWorkletThread`), a task will be
// posted to the main thread, and the main thread will in turn invoke
// `ThreadedMessagingProxyBase::ParentObjectDestroyed` to terminate the worklet
// thread gracefully. Eventually, this proxy will be destroyed on the main
// thread.
class MODULES_EXPORT SharedStorageWorkletMessagingProxy final
    : public ThreadedWorkletMessagingProxy {
 public:
  SharedStorageWorkletMessagingProxy(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver,
      base::OnceClosure worklet_terminated_callback);

  void WorkerThreadTerminated() override;

  void Trace(Visitor*) const override;

 private:
  friend class SharedStorageWorkletTest;

  void InitializeSharedStorageWorkletServiceOnWorkletThread(
      WorkerThread* worker_thread,
      mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver);

  void OnSharedStorageWorkletServiceDisconnectedOnWorkletThread();

  std::unique_ptr<WorkerThread> CreateWorkerThread() override;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  base::OnceClosure worklet_terminated_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SERVICES_SHARED_STORAGE_WORKLET_MESSAGING_PROXY_H_
