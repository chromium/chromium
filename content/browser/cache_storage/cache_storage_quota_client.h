// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {

class CacheStorageManager;

// CacheStorageQuotaClient is a self-owned receiver created by
// CacheStorageContextImpl.  The remote end is owned by QuotaManagerProxy.
// There is one CacheStorageQuotaClient per CacheStorageManager /
// CacheStorageOwner tuple.  Created and accessed on the cache storage task
// runner.
class CONTENT_EXPORT CacheStorageQuotaClient
    : public storage::mojom::QuotaClient {
 public:
  CacheStorageQuotaClient(scoped_refptr<CacheStorageManager> cache_manager,
                          storage::mojom::CacheStorageOwner owner);
  ~CacheStorageQuotaClient() override;

  // QuotaClient.
  void GetOriginUsage(const url::Origin& origin,
                      blink::mojom::StorageType type,
                      GetOriginUsageCallback callback) override;
  void GetOriginsForType(blink::mojom::StorageType type,
                         GetOriginsForTypeCallback callback) override;
  void GetOriginsForHost(blink::mojom::StorageType type,
                         const std::string& host,
                         GetOriginsForHostCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        DeleteOriginDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

  static storage::QuotaClientType GetClientTypeFromOwner(
      storage::mojom::CacheStorageOwner owner);

 private:
  const scoped_refptr<CacheStorageManager> cache_manager_;
  const storage::mojom::CacheStorageOwner owner_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CacheStorageQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
