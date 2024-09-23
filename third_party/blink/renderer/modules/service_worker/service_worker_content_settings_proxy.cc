// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_content_settings_proxy.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

ServiceWorkerContentSettingsProxy::ServiceWorkerContentSettingsProxy(
    mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy> host_info)
    : host_info_(std::move(host_info)) {}

ServiceWorkerContentSettingsProxy::~ServiceWorkerContentSettingsProxy() =
    default;

bool ServiceWorkerContentSettingsProxy::AllowStorageAccessSync(
    StorageType storage_type) {
  bool result = false;
  if (storage_type == StorageType::kIndexedDB) {
    SCOPED_UMA_HISTOGRAM_TIMER("ServiceWorker.AllowIndexedDBTime");
    GetService()->AllowIndexedDB(&result);
    return result;
  } else if (storage_type == StorageType::kFileSystem) {
    NOTREACHED_IN_MIGRATION();
    return false;
  } else {
    // TODO(shuagga@microsoft.com): Revisit this default in the future.
    return true;
  }
}

// Use ThreadSpecific to ensure that |content_settings_instance_host| is
// destructed on worker thread.
// Each worker has a dedicated thread so this is safe.
mojo::Remote<mojom::blink::WorkerContentSettingsProxy>&
ServiceWorkerContentSettingsProxy::GetService() {
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
