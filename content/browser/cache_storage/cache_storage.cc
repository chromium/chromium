// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage.h"

namespace content {

constexpr int64_t CacheStorage::kSizeUnknown;
CacheStorage::CacheStorage(const url::Origin& origin) : origin_(origin) {}

}  // namespace content
