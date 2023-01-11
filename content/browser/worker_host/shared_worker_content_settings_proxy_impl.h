// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONTENT_SETTINGS_PROXY_IMPL_H_
#define CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONTENT_SETTINGS_PROXY_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom.h"
#include "url/origin.h"

namespace content {

class SharedWorkerHost;

// SharedWorkerContentSettingsProxyImpl passes content settings to its renderer
// counterpart blink::SharedWorkerContentSettingsProxy.
// Created on SharedWorker::Start() and connects to the counterpart
// at the moment.
// SharedWorkerHost owns this class, so the lifetime of this class is strongly
// associated to it.
class SharedWorkerContentSettingsProxyImpl
    : public blink::mojom::WorkerContentSettingsProxy {
 public:
  SharedWorkerContentSettingsProxyImpl(
      const GURL& script_url,
      SharedWorkerHost* owner,
      mojo::PendingReceiver<blink::mojom::WorkerContentSettingsProxy> receiver);

  SharedWorkerContentSettingsProxyImpl(
      const SharedWorkerContentSettingsProxyImpl&) = delete;
  SharedWorkerContentSettingsProxyImpl& operator=(
      const SharedWorkerContentSettingsProxyImpl&) = delete;

  ~SharedWorkerContentSettingsProxyImpl() override;

  // blink::mojom::WorkerContentSettingsProxy implementation.
  void AllowIndexedDB(AllowIndexedDBCallback callback) override;
  void AllowCacheStorage(AllowCacheStorageCallback callback) override;
  void AllowWebLocks(AllowCacheStorageCallback callback) override;
  void RequestFileSystemAccessSync(
      RequestFileSystemAccessSyncCallback callback) override;

 private:
  const url::Origin origin_;
  raw_ptr<SharedWorkerHost> owner_;
  mojo::Receiver<blink::mojom::WorkerContentSettingsProxy> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_CONTENT_SETTINGS_PROXY_IMPL_H_
