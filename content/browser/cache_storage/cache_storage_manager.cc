// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include "url/origin.h"

namespace content {

// static
bool CacheStorageManager::IsValidQuotaOrigin(const url::Origin& origin) {
  // Disallow opaque origins at the quota boundary because we DCHECK that we
  // don't get an opaque origin in lower code layers.
  return !origin.opaque();
}

}  // namespace content
