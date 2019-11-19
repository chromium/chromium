// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {

class CacheStorageManager;
enum class CacheStorageOwner;

// CacheStorageQuotaClient is owned by the QuotaManager. There is one per
// CacheStorageManager/CacheStorageOwner tuple.  Created and accessed on
// the IO thread.
class CONTENT_EXPORT CacheStorageQuotaClient : public storage::QuotaClient {
 public:
  CacheStorageQuotaClient(scoped_refptr<CacheStorageManager> cache_manager,
                          CacheStorageOwner owner);
  ~CacheStorageQuotaClient() override;

  // QuotaClient overrides
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

  static ID GetIDFromOwner(CacheStorageOwner owner);

 private:
  scoped_refptr<CacheStorageManager> cache_manager_;
  CacheStorageOwner owner_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
