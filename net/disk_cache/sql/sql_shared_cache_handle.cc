// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_handle.h"

#include "net/disk_cache/sql/sql_shared_cache.h"

namespace disk_cache {

SqlSharedCacheHandle::SqlSharedCacheHandle(
    base::WeakPtr<SqlSharedCache> weak_cache,
    base::PassKey<SqlSharedCache>)
    : weak_cache_(std::move(weak_cache)) {
  CHECK(weak_cache_);
  weak_cache_->IncrementHandleCount(base::PassKey<SqlSharedCacheHandle>());
}

SqlSharedCacheHandle::~SqlSharedCacheHandle() {
  if (weak_cache_) {
    weak_cache_->DecrementHandleCount(base::PassKey<SqlSharedCacheHandle>());
  }
}

SqlSharedCacheHandle::operator bool() const {
  return !!weak_cache_;
}

SqlSharedCache* SqlSharedCacheHandle::operator->() const {
  return weak_cache_.get();
}

SqlSharedCache* SqlSharedCacheHandle::get() const {
  return weak_cache_.get();
}

}  // namespace disk_cache
