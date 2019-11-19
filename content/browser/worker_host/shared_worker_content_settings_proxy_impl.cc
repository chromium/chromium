// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_content_settings_proxy_impl.h"

#include <utility>

#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "url/gurl.h"

namespace content {

SharedWorkerContentSettingsProxyImpl::SharedWorkerContentSettingsProxyImpl(
    const GURL& script_url,
    SharedWorkerHost* owner,
    mojo::PendingReceiver<blink::mojom::WorkerContentSettingsProxy> receiver)
    : origin_(url::Origin::Create(script_url)),
      owner_(owner),
      receiver_(this, std::move(receiver)) {}

SharedWorkerContentSettingsProxyImpl::~SharedWorkerContentSettingsProxyImpl() =
    default;

void SharedWorkerContentSettingsProxyImpl::AllowIndexedDB(
    AllowIndexedDBCallback callback) {
  if (!origin_.opaque()) {
    owner_->AllowIndexedDB(origin_.GetURL(), std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void SharedWorkerContentSettingsProxyImpl::AllowCacheStorage(
    AllowCacheStorageCallback callback) {
  if (!origin_.opaque()) {
    owner_->AllowCacheStorage(origin_.GetURL(), std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void SharedWorkerContentSettingsProxyImpl::AllowWebLocks(
    AllowCacheStorageCallback callback) {
  if (!origin_.opaque()) {
    owner_->AllowWebLocks(origin_.GetURL(), std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void SharedWorkerContentSettingsProxyImpl::RequestFileSystemAccessSync(
    RequestFileSystemAccessSyncCallback callback) {
  if (!origin_.opaque()) {
    owner_->AllowFileSystem(origin_.GetURL(), std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

}  // namespace content
