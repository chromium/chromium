/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_cache.h"

#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

namespace blink {

// A helper method for coverting a MemoryCache::TypeStatistic to a
// WebCacheResourceTypeStat.
static void ToResourceTypeStat(const MemoryCache::TypeStatistic& from,
                               WebCacheResourceTypeStat& to) {
  to.count = from.count;
  to.size = from.size;
  to.decoded_size = from.decoded_size;
}

void WebCache::SetCapacity(size_t capacity) {
  MemoryCache* cache = GetMemoryCache();
  if (cache)
    cache->SetCapacity(static_cast<unsigned>(capacity));
}

void WebCache::Clear() {
  MemoryCache* cache = GetMemoryCache();
  if (cache)
    cache->EvictResources();
}

void WebCache::GetUsageStats(UsageStats* result) {
  DCHECK(result);

  MemoryCache* cache = GetMemoryCache();
  if (cache) {
    result->capacity = cache->Capacity();
    result->size = cache->size();
  } else {
    memset(result, 0, sizeof(UsageStats));
  }
}

void WebCache::GetResourceTypeStats(WebCacheResourceTypeStats* result) {
  MemoryCache* cache = GetMemoryCache();
  if (cache) {
    MemoryCache::Statistics stats = cache->GetStatistics();
    ToResourceTypeStat(stats.images, result->images);
    ToResourceTypeStat(stats.css_style_sheets, result->css_style_sheets);
    ToResourceTypeStat(stats.scripts, result->scripts);
    ToResourceTypeStat(stats.xsl_style_sheets, result->xsl_style_sheets);
    ToResourceTypeStat(stats.fonts, result->fonts);
    ToResourceTypeStat(stats.other, result->other);
  } else {
    memset(result, 0, sizeof(WebCacheResourceTypeStats));
  }
}

}  // namespace blink
