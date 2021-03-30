// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/browser/cache_storage/cache_storage_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace url {
class Origin;
}

namespace content {

// Keeps track of a CacheStorage per origin. There is one CacheStorageManager
// per CacheStorageOwner. Created and accessed from a single sequence.
// TODO(jkarlin): Remove CacheStorage from memory once they're no
// longer in active use.
class CONTENT_EXPORT CacheStorageManager
    : public base::RefCounted<CacheStorageManager> {
 public:
  // Open the CacheStorage for the given origin and owner.  A reference counting
  // handle is returned which can be stored and used similar to a weak pointer.
  virtual CacheStorageHandle OpenCacheStorage(
      const url::Origin& origin,
      storage::mojom::CacheStorageOwner owner) = 0;

  // QuotaClient and Browsing Data Deletion support.
  virtual void GetAllOriginsUsage(
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::CacheStorageControl::GetAllOriginsInfoCallback
          callback) = 0;
  virtual void GetOriginUsage(
      const url::Origin& origin_url,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetOriginUsageCallback callback) = 0;
  virtual void GetOrigins(
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetOriginsForTypeCallback callback) = 0;
  virtual void GetOriginsForHost(
      const std::string& host,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetOriginsForHostCallback callback) = 0;
  virtual void DeleteOriginData(
      const url::Origin& origin,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::DeleteOriginDataCallback callback) = 0;
  virtual void DeleteOriginData(const url::Origin& origin,
                                storage::mojom::CacheStorageOwner owner) = 0;

  virtual void AddObserver(
      mojo::PendingRemote<storage::mojom::CacheStorageObserver> observer) = 0;

  static bool IsValidQuotaOrigin(const url::Origin& origin);

 protected:
  friend class base::RefCounted<CacheStorageManager>;

  CacheStorageManager() = default;
  virtual ~CacheStorageManager() = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_
