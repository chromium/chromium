// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia_memory_dump_provider.h"

#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "skia/ext/skia_trace_memory_dump_impl.h"
#include "third_party/skia/include/core/SkGraphics.h"

namespace skia {

// static
SkiaMemoryDumpProvider* SkiaMemoryDumpProvider::GetInstance() {
  return base::Singleton<
      SkiaMemoryDumpProvider,
      base::LeakySingletonTraits<SkiaMemoryDumpProvider>>::get();
}

SkiaMemoryDumpProvider::SkiaMemoryDumpProvider() = default;

SkiaMemoryDumpProvider::~SkiaMemoryDumpProvider() = default;

bool SkiaMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    auto* glyph_cache_dump =
        process_memory_dump->CreateAllocatorDump("skia/sk_glyph_cache");
    glyph_cache_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        SkGraphics::GetFontCacheUsed());
    auto* resource_cache_dump =
        process_memory_dump->CreateAllocatorDump("skia/sk_resource_cache");
    resource_cache_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        SkGraphics::GetResourceCacheTotalBytesUsed());
    return true;
  }
  SkiaTraceMemoryDumpImpl skia_dumper(args.level_of_detail,
                                      process_memory_dump);
  SkGraphics::DumpMemoryStatistics(&skia_dumper);

  return true;
}

}  // namespace skia
