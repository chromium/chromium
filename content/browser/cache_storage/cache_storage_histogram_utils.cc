// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_histogram_utils.h"

namespace content {

blink::mojom::CacheStorageError MakeErrorStorage(ErrorStorageType type) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorkerCache.ErrorStorageType", type);
  return blink::mojom::CacheStorageError::kErrorStorage;
}

}  // namespace content
