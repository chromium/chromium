// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"

#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

CanvasMemoryDumpProvider* CanvasMemoryDumpProvider::Instance() {
  DEFINE_STATIC_LOCAL(CanvasMemoryDumpProvider, instance, ());
  return &instance;
}

bool CanvasMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* memory_dump) {
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed) {
    base::AutoLock auto_lock(lock_);
    for (auto* it : clients_)
      it->OnMemoryDump(memory_dump);
    return true;
  }

  size_t total_size = 0;
  size_t clients_size = 0;
  {
    base::AutoLock auto_lock(lock_);
    for (auto* it : clients_)
      total_size += it->GetSize();
    clients_size = clients_.size();
  }

  auto* dump =
      memory_dump->CreateAllocatorDump("canvas/ResourceProvider/SkSurface");
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  total_size);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  clients_size);

  // SkiaMemoryDumpProvider reports only sk_glyph_cache and sk_resource_cache.
  // So the SkSurface is suballocation of malloc, not SkiaDumpProvider.
  if (const char* system_allocator_name =
          base::trace_event::MemoryDumpManager::GetInstance()
              ->system_allocator_pool_name()) {
    memory_dump->AddSuballocation(dump->guid(), system_allocator_name);
  }
  return true;
}

void CanvasMemoryDumpProvider::RegisterClient(CanvasMemoryDumpClient* client) {
  base::AutoLock auto_lock(lock_);
  clients_.insert(client);
}

void CanvasMemoryDumpProvider::UnregisterClient(
    CanvasMemoryDumpClient* client) {
  base::AutoLock auto_lock(lock_);
  DCHECK(clients_.Contains(client));
  clients_.erase(client);
}

}  // namespace blink
