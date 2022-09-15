// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_HANDLE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_HANDLE_H_

#include "content/browser/cache_storage/cache_storage_ref.h"

namespace content {

class CacheStorageCache;
using CacheStorageCacheHandle = CacheStorageRef<CacheStorageCache>;

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_HANDLE_H_
