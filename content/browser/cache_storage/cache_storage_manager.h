// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_

#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/browser/cache_storage/cache_storage_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// Keeps track of a CacheStorage per StorageKey. There is one
// CacheStorageManager per CacheStorageOwner. Created and accessed from a single
// sequence.
// TODO(jkarlin): Remove CacheStorage from memory once they're no
// longer in active use.
class CONTENT_EXPORT CacheStorageManager
    : public base::RefCounted<CacheStorageManager> {
 public:
  // Open the CacheStorage for the given `storage_key` and `owner`.  A reference
  // counting handle is returned which can be stored and used similar to a weak
  // pointer.
  virtual CacheStorageHandle OpenCacheStorage(
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner) = 0;

  // QuotaClient and Browsing Data Deletion support.
  virtual void GetAllStorageKeysUsage(
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::CacheStorageControl::GetAllStorageKeysInfoCallback
          callback) = 0;
  virtual void GetStorageKeyUsage(
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetBucketUsageCallback callback) = 0;
  virtual void GetStorageKeys(
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetStorageKeysForTypeCallback callback) = 0;
  virtual void DeleteStorageKeyData(
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback) = 0;
  virtual void DeleteStorageKeyData(
      const blink::StorageKey& storage_key,
      storage::mojom::CacheStorageOwner owner) = 0;

  virtual void AddObserver(
      mojo::PendingRemote<storage::mojom::CacheStorageObserver> observer) = 0;

  static bool IsValidQuotaStorageKey(const blink::StorageKey& storage_key);

 protected:
  friend class base::RefCounted<CacheStorageManager>;

  CacheStorageManager() = default;
  virtual ~CacheStorageManager() = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_
