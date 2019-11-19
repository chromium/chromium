// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker_content_settings_proxy.h"

#include <memory>
#include <utility>
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-blink.h"

namespace blink {

SharedWorkerContentSettingsProxy::SharedWorkerContentSettingsProxy(
    mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy> host_info)
    : host_info_(std::move(host_info)) {}
SharedWorkerContentSettingsProxy::~SharedWorkerContentSettingsProxy() = default;

bool SharedWorkerContentSettingsProxy::AllowIndexedDB() {
  bool result = false;
  GetService()->AllowIndexedDB(&result);
  return result;
}

bool SharedWorkerContentSettingsProxy::AllowCacheStorage() {
  bool result = false;
  GetService()->AllowCacheStorage(&result);
  return result;
}

bool SharedWorkerContentSettingsProxy::AllowWebLocks() {
  bool result = false;
  GetService()->AllowWebLocks(&result);
  return result;
}

bool SharedWorkerContentSettingsProxy::RequestFileSystemAccessSync() {
  bool result = false;
  GetService()->RequestFileSystemAccessSync(&result);
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
