// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache_memory_dump_provider.h"

#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

FontCacheMemoryDumpProvider* FontCacheMemoryDumpProvider::Instance() {
  DEFINE_STATIC_LOCAL(FontCacheMemoryDumpProvider, instance, ());
  return &instance;
}

bool FontCacheMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs&,
    base::trace_event::ProcessMemoryDump* memory_dump) {
  DCHECK(IsMainThread());
  FontCache* cache = FontCache::GetFontCache(kDoNotCreate);
  if (cache) {
    cache->DumpFontPlatformDataCache(memory_dump);
    cache->DumpShapeResultCache(memory_dump);
  }
  return true;
}

}  // namespace blink
