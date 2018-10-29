// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/code_cache_loader_mock.h"

namespace blink {

void CodeCacheLoaderMock::FetchFromCodeCacheSynchronously(
    const GURL& url,
    base::Time* response_time_out,
    std::vector<uint8_t>* data_out) {
  *response_time_out = base::Time();
  *data_out = std::vector<uint8_t>();
}

void CodeCacheLoaderMock::FetchFromCodeCache(
    blink::mojom::CodeCacheType cache_type,
    const GURL& kurl,
    CodeCacheLoader::FetchCodeCacheCallback callback) {
  std::move(callback).Run(base::Time(), std::vector<uint8_t>());
}

}  // namespace blink
