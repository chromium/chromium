// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTENT_SETTINGS_PROXY_IMPL_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTENT_SETTINGS_PROXY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerContextWrapper;

// ServiceWorkerContentSettingsProxyImpl passes content settings to its renderer
// counterpart blink::ServiceWorkerContentSettingsProxy
// Created on EmbeddedWorkerInstance::SendStartWorker() and connects to the
// counterpart at the moment.
// EmbeddedWorkerInstance owns this class, so the lifetime of this class is
// strongly associated to it. This class lives on the UI thread.
class ServiceWorkerContentSettingsProxyImpl final
    : public blink::mojom::WorkerContentSettingsProxy {
 public:
  ServiceWorkerContentSettingsProxyImpl(
      const GURL& script_url,
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      mojo::PendingReceiver<blink::mojom::WorkerContentSettingsProxy> receiver);

  ~ServiceWorkerContentSettingsProxyImpl() override;

  // blink::mojom::WorkerContentSettingsProxy implementation
  void AllowIndexedDB(AllowIndexedDBCallback callback) override;
  void AllowCacheStorage(AllowCacheStorageCallback callback) override;
  void AllowWebLocks(AllowCacheStorageCallback callback) override;
  void RequestFileSystemAccessSync(
      RequestFileSystemAccessSyncCallback callback) override;

 private:
  const url::Origin origin_;
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
  mojo::Receiver<blink::mojom::WorkerContentSettingsProxy> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTENT_SETTINGS_PROXY_IMPL_H_
