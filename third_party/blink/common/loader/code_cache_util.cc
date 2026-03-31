// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/code_cache_util.h"

#include <stdint.h>

#include <string>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace blink {

std::string UrlToCodeCacheKey(const GURL& url) {
  return base::StrCat(
      {// Add a prefix so that the key can't be parsed as a valid URL.
       kCodeCacheKeyPrefix,
       // Remove reference, username and password sections of the URL.
       net::SimplifyUrlForRequest(url).spec()});
}

base::HeapArray<uint8_t> ComposeSourceKeyedCacheKey(
    base::span<const uint8_t> source_hash) {
  auto prefix = base::byte_span_from_cstring(kSourceKeyedCodeCacheKeyPrefix);
  auto key =
      base::HeapArray<uint8_t>::Uninit(prefix.size() + source_hash.size());
  key.first(prefix.size()).copy_from_nonoverlapping(prefix);
  key.last(source_hash.size()).copy_from_nonoverlapping(source_hash);
  return key;
}

}  // namespace blink
