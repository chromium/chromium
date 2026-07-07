// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_HANDLE_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_HANDLE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "net/base/net_export.h"

namespace disk_cache {

class SqlSharedCache;

// A ref-counted handle to a `SqlSharedCache` instance.
//
// Holding a `SqlSharedCacheHandle` keeps the underlying `SqlSharedCache` alive
// and prevents its lifetime from being cleaned up by `SqlSharedCacheManager`.
class NET_EXPORT_PRIVATE SqlSharedCacheHandle
    : public base::RefCounted<SqlSharedCacheHandle> {
 public:
  // Creates a handle associated with `weak_cache`. Can only be constructed
  // by `SqlSharedCache` using a passkey.
  SqlSharedCacheHandle(base::WeakPtr<SqlSharedCache> weak_cache,
                       base::PassKey<SqlSharedCache>);
  SqlSharedCacheHandle(const SqlSharedCacheHandle&) = delete;
  SqlSharedCacheHandle& operator=(const SqlSharedCacheHandle&) = delete;

  // Returns true if the underlying `SqlSharedCache` is still valid and alive.
  explicit operator bool() const;

  // Accesses the underlying `SqlSharedCache`. Returns nullptr if destroyed.
  SqlSharedCache* operator->() const;

  // Returns a raw pointer to the underlying `SqlSharedCache`. Returns nullptr
  // if the cache has been destroyed.
  SqlSharedCache* get() const;

 private:
  friend class base::RefCounted<SqlSharedCacheHandle>;
  ~SqlSharedCacheHandle();

  base::WeakPtr<SqlSharedCache> weak_cache_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_HANDLE_H_
