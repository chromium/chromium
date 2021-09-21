// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/storage_key_quota_client.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class CacheStorageManager;

// CacheStorageQuotaClient is a self-owned receiver created by
// CacheStorageContextImpl.  The remote end is owned by QuotaManagerProxy.
// There is one CacheStorageQuotaClient per CacheStorageManager /
// CacheStorageOwner tuple.  Created and accessed on the cache storage task
// runner.
class CONTENT_EXPORT CacheStorageQuotaClient
    : public storage::StorageKeyQuotaClient {
 public:
  CacheStorageQuotaClient(scoped_refptr<CacheStorageManager> cache_manager,
                          storage::mojom::CacheStorageOwner owner);

  CacheStorageQuotaClient(const CacheStorageQuotaClient&) = delete;
  CacheStorageQuotaClient& operator=(const CacheStorageQuotaClient&) = delete;

  ~CacheStorageQuotaClient() override;

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

  static storage::QuotaClientType GetClientTypeFromOwner(
      storage::mojom::CacheStorageOwner owner);

 private:
  const scoped_refptr<CacheStorageManager> cache_manager_;
  const storage::mojom::CacheStorageOwner owner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
