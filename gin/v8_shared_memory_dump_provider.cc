// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_shared_memory_dump_provider.h"

#include <string>

#include "base/memory/singleton.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "v8/include/v8.h"

namespace gin {

V8SharedMemoryDumpProvider::V8SharedMemoryDumpProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "V8SharedMemory", nullptr);
}

// static
void V8SharedMemoryDumpProvider::Register() {
  // The provider is registered as part of the constructor, so all this does is
  // access the singleton causing it to be constructed on the first access.
  base::Singleton<
      V8SharedMemoryDumpProvider,
      base::LeakySingletonTraits<V8SharedMemoryDumpProvider>>::get();
}

// Called at trace dump point time. Creates a snapshot with the memory counters
// for the current isolate.
bool V8SharedMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  v8::SharedMemoryStatistics shared_memory_statistics;
  v8::V8::GetSharedMemoryStatistics(&shared_memory_statistics);

  std::string dump_base_name = "v8/shared";
  auto* shared_memory_dump = process_memory_dump->CreateAllocatorDump(
      dump_base_name + "/read_only_space");
  shared_memory_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameSize,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      shared_memory_statistics.read_only_space_physical_size());
  shared_memory_dump->AddScalar(
      "allocated_objects_size",
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      shared_memory_statistics.read_only_space_used_size());
  shared_memory_dump->AddScalar(
      "virtual_size", base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      shared_memory_statistics.read_only_space_size());

  return true;
}

}  // namespace gin
