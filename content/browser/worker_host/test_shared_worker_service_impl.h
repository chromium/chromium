// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_TEST_SHARED_WORKER_SERVICE_IMPL_H_
#define CONTENT_BROWSER_WORKER_HOST_TEST_SHARED_WORKER_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {

class ServiceWorkerContextWrapper;
class StoragePartitionImpl;

// This class allows tests to shutdown workers and wait for their termination.
class TestSharedWorkerServiceImpl : public SharedWorkerServiceImpl {
 public:
  TestSharedWorkerServiceImpl(
      StoragePartitionImpl* storage_partition,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  TestSharedWorkerServiceImpl(const TestSharedWorkerServiceImpl&) = delete;
  TestSharedWorkerServiceImpl& operator=(const TestSharedWorkerServiceImpl&) =
      delete;

  ~TestSharedWorkerServiceImpl() override;

  void TerminateAllWorkers(base::OnceClosure callback);
  void SetWorkerTerminationCallback(base::OnceClosure callback);

  // SharedWorkerServiceImpl:
  void DestroyHost(SharedWorkerHost* host) override;

 private:
  // Invoked when a remote shared worker disconnection handler is fired.
  void OnRemoteSharedWorkerConnectionLost(mojo::RemoteSetElementId id);

  // Invoked when all workers hosts are deleted and their respective remote
  // shared workers are disconnected.
  base::OnceClosure terminate_all_workers_callback_;

  // Holds all remote shared workers whose host have been deleted and are now
  // expected to soon cause the disconnection handler to fire.
  mojo::RemoteSet<blink::mojom::SharedWorker> workers_awaiting_disconnection_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_TEST_SHARED_WORKER_SERVICE_IMPL_H_
