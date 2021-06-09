// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// static
bool CacheStorageManager::IsValidQuotaStorageKey(
    const blink::StorageKey& storage_key) {
  // Disallow opaque storage keys at the quota boundary because we DCHECK that
  // we don't get an opaque key in lower code layers.
  return !storage_key.origin().opaque();
}

}  // namespace content
