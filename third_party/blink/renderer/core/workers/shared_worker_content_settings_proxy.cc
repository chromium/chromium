// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker_content_settings_proxy.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

SharedWorkerContentSettingsProxy::SharedWorkerContentSettingsProxy(
    mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy> host_info)
    : host_info_(std::move(host_info)) {}
SharedWorkerContentSettingsProxy::~SharedWorkerContentSettingsProxy() = default;

bool SharedWorkerContentSettingsProxy::AllowStorageAccessSync(
    StorageType storage_type) {
  bool result = false;
  switch (storage_type) {
    case StorageType::kIndexedDB: {
      SCOPED_UMA_HISTOGRAM_TIMER("ServiceWorker.AllowIndexedDBTime");
      GetService()->AllowIndexedDB(&result);
      break;
    }
    case StorageType::kCacheStorage: {
      SCOPED_UMA_HISTOGRAM_TIMER("ServiceWorker.AllowCacheStorageTime");
      GetService()->AllowCacheStorage(&result);
      break;
    }
    case StorageType::kWebLocks: {
      SCOPED_UMA_HISTOGRAM_TIMER("ServiceWorker.AllowWebLocksTime");
      GetService()->AllowWebLocks(&result);
      break;
    }
    case StorageType::kFileSystem: {
      SCOPED_UMA_HISTOGRAM_TIMER("ServiceWorker.RequestFileSystemAccessTime");
      GetService()->RequestFileSystemAccessSync(&result);
      break;
    }
    default: {
      // TODO(shuagga@microsoft.com): Revisit this default in the future.
      return true;
    }
  }

  return result;
}

// Use ThreadSpecific to ensure that |content_settings_instance_host| is
// destructed on worker thread.
// Each worker has a dedicated thread so this is safe.
mojo::Remote<mojom::blink::WorkerContentSettingsProxy>&
SharedWorkerContentSettingsProxy::GetService() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<mojo::Remote<mojom::blink::WorkerContentSettingsProxy>>,
      content_settings_instance_host, ());
  if (!content_settings_instance_host.IsSet()) {
    DCHECK(host_info_.is_valid());
    content_settings_instance_host->Bind(std::move(host_info_));
  }
  return *content_settings_instance_host;
}

}  // namespace blink
