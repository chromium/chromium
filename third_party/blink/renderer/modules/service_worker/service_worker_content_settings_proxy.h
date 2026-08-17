// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CONTENT_SETTINGS_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CONTENT_SETTINGS_PROXY_H_

#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// Provides the content settings information from browser process. This proxy
// is created by WebEmbeddedWorkerImpl::StartWorkerContext() on a background
// ThreadPool thread, passed to the service worker thread via
// GlobalScopeCreationParams, and then called and destroyed on the service
// worker thread.
class MODULES_EXPORT ServiceWorkerContentSettingsProxy final
    : public blink::WebContentSettingsClient {
 public:
  explicit ServiceWorkerContentSettingsProxy(
      mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy> host_info);

  ServiceWorkerContentSettingsProxy(const ServiceWorkerContentSettingsProxy&) =
      delete;
  ServiceWorkerContentSettingsProxy& operator=(
      const ServiceWorkerContentSettingsProxy&) = delete;

  ~ServiceWorkerContentSettingsProxy() override;

  void SetSecurityOrigin(scoped_refptr<const blink::SecurityOrigin>);

  // WebContentSettingsClient overrides.
  // Asks the browser process about the settings.
  void AllowStorageAccess(StorageType storage_type,
                          base::OnceCallback<void(bool)> callback) override;

  // Asks the browser process about the settings.
  // Blocks until the response arrives.
  bool AllowStorageAccessSync(StorageType storage_type) override;

 private:
  mojo::Remote<mojom::blink::WorkerContentSettingsProxy>& GetService();

  mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy> host_info_;
  // Bound and destroyed on the service worker thread.
  mojo::Remote<mojom::blink::WorkerContentSettingsProxy> host_remote_;

  THREAD_CHECKER(worker_thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CONTENT_SETTINGS_PROXY_H_
