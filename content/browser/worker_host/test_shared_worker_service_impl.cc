// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/test_shared_worker_service_impl.h"

#include <utility>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"

namespace content {

TestSharedWorkerServiceImpl::TestSharedWorkerServiceImpl(
    StoragePartitionImpl* storage_partition,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<ChromeAppCacheService> appcache_service)
    : SharedWorkerServiceImpl(storage_partition,
                              std::move(service_worker_context),
                              std::move(appcache_service)) {}

TestSharedWorkerServiceImpl::~TestSharedWorkerServiceImpl() = default;

void TestSharedWorkerServiceImpl::TerminateAllWorkers(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!terminate_all_workers_callback_);

  // All workers may already be fully terminated.
  if (worker_hosts_.empty() && workers_awaiting_disconnection_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  SetWorkerTerminationCallback(std::move(callback));

  std::vector<SharedWorkerHost*> worker_hosts;
  worker_hosts.reserve(worker_hosts_.size());
  for (auto& host : worker_hosts_) {
    worker_hosts.push_back(host.get());
  }

  for (SharedWorkerHost* worker_host : worker_hosts)
    DestroyHost(worker_host);
}

void TestSharedWorkerServiceImpl::SetWorkerTerminationCallback(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  terminate_all_workers_callback_ = std::move(callback);
  workers_awaiting_disconnection_.set_disconnect_handler(base::BindRepeating(
      &TestSharedWorkerServiceImpl::OnRemoteSharedWorkerConnectionLost,
      base::Unretained(this)));
}

void TestSharedWorkerServiceImpl::DestroyHost(SharedWorkerHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  mojo::Remote<blink::mojom::SharedWorker> remote_shared_worker =
      host->TerminateRemoteWorkerForTesting();

  worker_hosts_.erase(worker_hosts_.find(host));

  if (remote_shared_worker && remote_shared_worker.is_connected()) {
    // The remote shared worker for this host is still connected. Track its
    // pending disconnection.
    workers_awaiting_disconnection_.Add(std::move(remote_shared_worker));
    return;
  }

  // Notify the termination callback if there aren't any running workers left.
  if (worker_hosts_.empty() && workers_awaiting_disconnection_.empty() &&
      terminate_all_workers_callback_) {
    std::move(terminate_all_workers_callback_).Run();
  }
}

void TestSharedWorkerServiceImpl::OnRemoteSharedWorkerConnectionLost(
    mojo::RemoteSetElementId id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (worker_hosts_.empty() && workers_awaiting_disconnection_.empty() &&
      terminate_all_workers_callback_) {
    std::move(terminate_all_workers_callback_).Run();
  }
}

}  // namespace content
