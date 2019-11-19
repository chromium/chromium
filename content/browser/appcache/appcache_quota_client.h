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
#include "content/browser/appcache/appcache_storage.h"
#include "content/common/content_export.h"
#include "net/base/completion_repeating_callback.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_task.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {
class AppCacheQuotaClientTest;
class AppCacheServiceImpl;
class AppCacheStorageImpl;

// A QuotaClient implementation to integrate the appcache service
// with the quota management system. The QuotaClient interface is
// used on the IO thread by the quota manager. This class deletes
// itself when both the quota manager and the appcache service have
// been destroyed.
class AppCacheQuotaClient : public storage::QuotaClient,
                            public base::SupportsWeakPtr<AppCacheQuotaClient> {
 public:
  using RequestQueue = base::circular_deque<base::OnceClosure>;

  ~AppCacheQuotaClient() override;

  // QuotaClient method overrides
  ID id() const override;
  void OnQuotaManagerDestroyed() override;
  void GetOriginUsage(const url::Origin& origin,
                      blink::mojom::StorageType type,
                      GetUsageCallback callback) override;
  void GetOriginsForType(blink::mojom::StorageType type,
                         GetOriginsCallback callback) override;
  void GetOriginsForHost(blink::mojom::StorageType type,
                         const std::string& host,
                         GetOriginsCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        DeletionCallback callback) override;
  bool DoesSupport(blink::mojom::StorageType type) const override;

 private:
  friend class content::AppCacheQuotaClientTest;
  friend class AppCacheServiceImpl;  // for NotifyAppCacheIsDestroyed
  friend class AppCacheStorageImpl;  // for NotifyAppCacheIsReady

  CONTENT_EXPORT
  explicit AppCacheQuotaClient(base::WeakPtr<AppCacheServiceImpl> service);

  void DidDeleteAppCachesForOrigin(int rv);
  void GetOriginsHelper(blink::mojom::StorageType type,
                        const std::string& opt_host,
                        GetOriginsCallback callback);
  void ProcessPendingRequests();
  void DeletePendingRequests();
  net::CancelableCompletionRepeatingCallback* GetServiceDeleteCallback();

  // For use by appcache internals during initialization and shutdown.
  CONTENT_EXPORT void NotifyAppCacheReady();
  CONTENT_EXPORT void NotifyAppCacheDestroyed();

  // Prior to appcache service being ready, we have to queue
  // up reqeusts and defer acting on them until we're ready.
  RequestQueue pending_batch_requests_;
  RequestQueue pending_serial_requests_;

  // And once it's ready, we can only handle one delete request at a time,
  // so we queue up additional requests while one is in already in progress.
  DeletionCallback current_delete_request_callback_;
  std::unique_ptr<net::CancelableCompletionRepeatingCallback>
      service_delete_callback_;

  base::WeakPtr<AppCacheServiceImpl> service_;
  bool appcache_is_ready_ = false;
  bool service_is_destroyed_ = false;
  // This is used to prevent this object from being deleted in
  // OnQuotaManagerDestroyed() while NotifyAppCacheDestroyed() is still running.
  bool keep_alive_ = false;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AppCacheQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_QUOTA_CLIENT_H_
