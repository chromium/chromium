// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker_content_settings_proxy.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

SharedWorkerContentSettingsProxy::SharedWorkerContentSettingsProxy(
    mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy> host_info)
    : host_info_(std::move(host_info)) {
  DETACH_FROM_THREAD(worker_thread_checker_);
}

SharedWorkerContentSettingsProxy::~SharedWorkerContentSettingsProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
}

void SharedWorkerContentSettingsProxy::AllowStorageAccess(
    StorageType storage_type,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  switch (storage_type) {
    case StorageType::kIndexedDB:
      GetService()->AllowIndexedDB(std::move(callback));
      return;
    case StorageType::kCacheStorage:
      GetService()->AllowCacheStorage(std::move(callback));
      return;
    case StorageType::kWebLocks:
      GetService()->AllowWebLocks(std::move(callback));
      return;
    case StorageType::kFileSystem:
      GetService()->AllowFileSystem(std::move(callback));
      return;
    default:
      // TODO(crbug.com/40103756): Revisit this default in the future.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, blink::BindOnce(std::move(callback), true));
      return;
  }
}

bool SharedWorkerContentSettingsProxy::AllowStorageAccessSync(
    StorageType storage_type) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  bool result = false;
  switch (storage_type) {
    case StorageType::kIndexedDB:
      GetService()->AllowIndexedDB(&result);
      break;
    case StorageType::kCacheStorage:
      GetService()->AllowCacheStorage(&result);
      break;
    case StorageType::kWebLocks:
      GetService()->AllowWebLocks(&result);
      break;
    case StorageType::kFileSystem:
      GetService()->AllowFileSystem(&result);
      break;
    default:
      // TODO(crbug.com/40103756): Revisit this default in the future.
      return true;
  }

  return result;
}

mojo::Remote<mojom::blink::WorkerContentSettingsProxy>&
SharedWorkerContentSettingsProxy::GetService() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  if (!host_remote_.is_bound()) {
    DCHECK(host_info_.is_valid());
    host_remote_.Bind(std::move(host_info_));
  }
  return host_remote_;
}

}  // namespace blink
