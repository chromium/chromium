// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_QUOTA_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/storage_key_quota_client.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/common/content_export.h"
#include "net/base/completion_repeating_callback.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_task.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {
class AppCacheQuotaClientTest;
class AppCacheServiceImpl;
class AppCacheStorageImpl;

// An StorageKeyQuotaClient implementation to integrate the appcache service
// with the quota management system. The StorageKeyQuotaClient interface is
// used on the IO thread by the quota manager. This class deletes
// itself when both the quota manager and the appcache service have
// been destroyed.
class AppCacheQuotaClient : public storage::StorageKeyQuotaClient {
 public:
  using RequestQueue = base::circular_deque<base::OnceClosure>;

  CONTENT_EXPORT
  explicit AppCacheQuotaClient(base::WeakPtr<AppCacheServiceImpl> service);

  ~AppCacheQuotaClient() override;

  // storage::StorageKeyQuotaClient method overrides.
  void GetStorageKeyUsage(const blink::StorageKey& storage_key,
                          blink::mojom::StorageType type,
                          GetStorageKeyUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void GetStorageKeysForHost(blink::mojom::StorageType type,
                             const std::string& host,
                             GetStorageKeysForHostCallback callback) override;
  void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            DeleteStorageKeyDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

  // The mojo pipe to this client was disconnected on the other side.
  CONTENT_EXPORT void OnMojoDisconnect();

 private:
  friend class content::AppCacheQuotaClientTest;
  friend class AppCacheServiceImpl;  // for NotifyServiceDestroyed
  friend class AppCacheStorageImpl;  // for NotifyStorageReady

  void DidDeleteAppCachesForStorageKey(int rv);
  void GetStorageKeysHelper(const std::string& opt_host,
                            GetStorageKeysForTypeCallback callback);
  void ProcessPendingRequests();
  void DeletePendingRequests();
  net::CancelableCompletionRepeatingCallback* GetServiceDeleteCallback();

  // For use by appcache internals during initialization and shutdown.

  // Called when the AppCacheStorageImpl's storage is fully initialized.
  //
  // After this point, calling AppCacheServiceImpl::storage() is guaranteed to
  // return a non-null value.
  CONTENT_EXPORT void NotifyStorageReady();

  // Called when the AppCacheServiceImpl passed to the constructor is destroyed.
  CONTENT_EXPORT void NotifyServiceDestroyed();

  // Prior to appcache service being ready, we have to queue
  // up requests and defer acting on them until we're ready.
  RequestQueue pending_batch_requests_;
  RequestQueue pending_serial_requests_;

  // And once it's ready, we can only handle one delete request at a time,
  // so we queue up additional requests while one is in already in progress.
  DeleteStorageKeyDataCallback current_delete_request_callback_;
  std::unique_ptr<net::CancelableCompletionRepeatingCallback>
      service_delete_callback_;

  base::WeakPtr<AppCacheServiceImpl> service_;
  bool appcache_is_ready_ = false;
  bool service_is_destroyed_ = false;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AppCacheQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_QUOTA_CLIENT_H_
