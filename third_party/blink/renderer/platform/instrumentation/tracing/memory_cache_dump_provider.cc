// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/tracing/memory_cache_dump_provider.h"

namespace blink {

void MemoryCacheDumpClient::Trace(Visitor* visitor) const {}

MemoryCacheDumpProvider* MemoryCacheDumpProvider::Instance() {
  DEFINE_STATIC_LOCAL(MemoryCacheDumpProvider, instance, ());
  return &instance;
}

bool MemoryCacheDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* memory_dump) {
  DCHECK(IsMainThread());
  if (!client_)
    return false;

  WebMemoryDumpLevelOfDetail level;
  switch (args.level_of_detail) {
    case base::trace_event::MemoryDumpLevelOfDetail::kBackground:
      level = blink::WebMemoryDumpLevelOfDetail::kBackground;
      break;
    case base::trace_event::MemoryDumpLevelOfDetail::kLight:
      level = blink::WebMemoryDumpLevelOfDetail::kLight;
      break;
    case base::trace_event::MemoryDumpLevelOfDetail::kDetailed:
      level = blink::WebMemoryDumpLevelOfDetail::kDetailed;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  WebProcessMemoryDump dump(args.level_of_detail, memory_dump);
  return client_->OnMemoryDump(level, &dump);
}

MemoryCacheDumpProvider::MemoryCacheDumpProvider() = default;

MemoryCacheDumpProvider::~MemoryCacheDumpProvider() = default;

}  // namespace blink
