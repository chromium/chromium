// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/v8_wrapper/blink_gc_memory_dump_provider.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state.h"
#include "v8/include/cppgc/heap-statistics.h"

namespace blink {
namespace {

constexpr const char* HeapTypeString(
    BlinkGCMemoryDumpProvider::HeapType heap_type) {
  switch (heap_type) {
    case BlinkGCMemoryDumpProvider::HeapType::kBlinkMainThread:
      return "main";
    case BlinkGCMemoryDumpProvider::HeapType::kBlinkWorkerThread:
      return "workers";
  }
}

}  // namespace

BlinkGCMemoryDumpProvider::BlinkGCMemoryDumpProvider(
    ThreadState* thread_state,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    BlinkGCMemoryDumpProvider::HeapType heap_type)
    : thread_state_(thread_state),
      heap_type_(heap_type),
      dump_base_name_(
          "blink_gc/" + std::string(HeapTypeString(heap_type_)) + "/heap" +
          (heap_type_ == HeapType::kBlinkWorkerThread
               ? "/" + base::StringPrintf(
                           "worker_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(thread_state_))
               : "")) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "BlinkGC", task_runner);
}

BlinkGCMemoryDumpProvider::~BlinkGCMemoryDumpProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool BlinkGCMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  ::cppgc::HeapStatistics::DetailLevel detail_level =
      args.level_of_detail ==
              base::trace_event::MemoryDumpLevelOfDetail::DETAILED
          ? ::cppgc::HeapStatistics::kDetailed
          : ::cppgc::HeapStatistics::kBrief;

  ::cppgc::HeapStatistics stats =
      ThreadState::Current()->cpp_heap().CollectStatistics(detail_level);

  auto* heap_dump = process_memory_dump->CreateAllocatorDump(dump_base_name_);
  heap_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                       base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                       stats.physical_size_bytes);
  heap_dump->AddScalar("allocated_objects_size",
                       base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                       stats.used_size_bytes);

  if (detail_level == ::cppgc::HeapStatistics::kBrief) {
    return true;
  }

  // Detailed statistics.
  for (const ::cppgc::HeapStatistics::SpaceStatistics& space_stats :
       stats.space_stats) {
    std::string arena_dump_name = dump_base_name_ + "/" + space_stats.name;
    auto* arena_dump =
        process_memory_dump->CreateAllocatorDump(arena_dump_name);
    arena_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_stats.physical_size_bytes);
    arena_dump->AddScalar("allocated_objects_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_stats.used_size_bytes);

    size_t page_count = 0;
    for (const ::cppgc::HeapStatistics::PageStatistics& page_stats :
         space_stats.page_stats) {
      auto* page_dump = process_memory_dump->CreateAllocatorDump(
          arena_dump_name + "/pages/page_" +
          base::NumberToString(page_count++));
      page_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                           base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                           page_stats.physical_size_bytes);
      page_dump->AddScalar("allocated_objects_size",
                           base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                           page_stats.used_size_bytes);
    }

    const ::cppgc::HeapStatistics::FreeListStatistics& free_list_stats =
        space_stats.free_list_stats;
    for (wtf_size_t i = 0; i < free_list_stats.bucket_size.size(); ++i) {
      constexpr size_t kDigits = 8;
      std::string original_bucket_size =
          base::NumberToString(free_list_stats.bucket_size[i]);
      std::string padded_bucket_size =
          std::string(kDigits - original_bucket_size.length(), '0') +
          original_bucket_size;
      auto* free_list_bucket_dump = process_memory_dump->CreateAllocatorDump(
          arena_dump_name + "/freelist/bucket_" + padded_bucket_size);
      free_list_bucket_dump->AddScalar(
          "free_size", base::trace_event::MemoryAllocatorDump::kUnitsBytes,
          free_list_stats.free_size[i]);
    }

    const ::cppgc::HeapStatistics::ObjectStatistics& object_stats =
        space_stats.object_stats;
    for (wtf_size_t i = 1; i < object_stats.num_types; i++) {
      if (object_stats.type_name[i].empty())
        continue;

      auto* class_dump = process_memory_dump->CreateAllocatorDump(
          arena_dump_name + "/classes/" + object_stats.type_name[i]);
      class_dump->AddScalar(
          "object_count", base::trace_event::MemoryAllocatorDump::kUnitsObjects,
          object_stats.type_count[i]);
      class_dump->AddScalar("object_size",
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            object_stats.type_bytes[i]);
    }
  }
  return true;
}

}  // namespace blink
