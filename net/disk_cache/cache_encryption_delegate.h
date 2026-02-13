// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_CACHE_ENCRYPTION_DELEGATE_H_
#define NET_DISK_CACHE_CACHE_ENCRYPTION_DELEGATE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/disk_cache/cache_entry_hasher.h"
#include "net/disk_cache/disk_cache.h"

namespace net {

class NET_EXPORT CacheEncryptionDelegate {
 public:
  CacheEncryptionDelegate() = default;

  virtual ~CacheEncryptionDelegate() = default;

  // Async init. Don't call any other methods before |callback| is called. The
  // callback will be run with net::OK on success, or an error code on failure.
  // If already initialized, should run the callback immediately.
  virtual void Init(base::OnceCallback<void(Error)> callback) = 0;

  // Returns a factory for creating encrypted backend file operations,
  // wrapped around the given `file_operations_factory`.
  virtual disk_cache::BackendFileOperationsFactory*
  GetEncryptionFileOperationsFactory(
      scoped_refptr<disk_cache::BackendFileOperationsFactory>
          file_operations_factory) = 0;

  // Returns a CacheEntryHasher that uses the encryption key to get the salted
  // hash for the entry. Can fail and return nullptr if the encryption key is
  // not available.
  virtual std::unique_ptr<disk_cache::CacheEntryHasher>
  GetCacheEntryHasher() = 0;
};

}  // namespace net

#endif  // NET_DISK_CACHE_CACHE_ENCRYPTION_DELEGATE_H_
