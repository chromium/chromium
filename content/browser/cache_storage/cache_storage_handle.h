// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HANDLE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HANDLE_H_

#include "content/browser/cache_storage/cache_storage_ref.h"

namespace content {

class CacheStorage;
using CacheStorageHandle = CacheStorageRef<CacheStorage>;

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HANDLE_H_
