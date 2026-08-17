// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_CONTENT_SETTINGS_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_CONTENT_SETTINGS_PROXY_H_

#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// SharedWorkerContentSettingsProxy provides content settings information.
// This is created on the main thread, passed to the worker thread via
// GlobalScopeCreationParams, and then called and destroyed on the worker
// thread. Information is requested via a Mojo connection to the browser
// process.
class CORE_EXPORT SharedWorkerContentSettingsProxy
    : public WebContentSettingsClient {
 public:
  SharedWorkerContentSettingsProxy(
      mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy> host_info);
  ~SharedWorkerContentSettingsProxy() override;

  // WebContentSettingsClient overrides.
  void AllowStorageAccess(StorageType storage_type,
                          base::OnceCallback<void(bool)> callback) override;
  bool AllowStorageAccessSync(StorageType storage_type) override;

 private:
  mojo::Remote<mojom::blink::WorkerContentSettingsProxy>& GetService();

  mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy> host_info_;
  // Bound and destroyed on the worker thread.
  mojo::Remote<mojom::blink::WorkerContentSettingsProxy> host_remote_;

  THREAD_CHECKER(worker_thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_CONTENT_SETTINGS_PROXY_H_
