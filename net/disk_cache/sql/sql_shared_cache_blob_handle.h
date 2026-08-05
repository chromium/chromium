// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_BLOB_HANDLE_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_BLOB_HANDLE_H_

#include "base/memory/ref_counted.h"

namespace disk_cache {

// A reference-counted handle that holds open a blob in the shared cache.
// While an instance of this class is held, the underlying SQLite blob handle
// remains cached in SqlSharedCacheIsolatedDatabase.
class SqlSharedCacheBlobHandle
    : public base::RefCountedThreadSafe<SqlSharedCacheBlobHandle> {
 protected:
  friend class base::RefCountedThreadSafe<SqlSharedCacheBlobHandle>;
  virtual ~SqlSharedCacheBlobHandle() = default;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_BLOB_HANDLE_H_
