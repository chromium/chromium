// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"

#include <inttypes.h>
#include <ios>
#include <sstream>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "v8/include/cppgc/heap-statistics.h"
#include "v8/include/v8-isolate.h"

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

void RecordType(
    std::vector<cppgc::HeapStatistics::ObjectStatsEntry>& global_object_stats,
    const cppgc::HeapStatistics::ObjectStatsEntry& local_object_stats,
    size_t entry_index) {
  global_object_stats[entry_index].allocated_bytes +=
      local_object_stats.allocated_bytes;
  global_object_stats[entry_index].object_count +=
      local_object_stats.object_count;
}

// Use the id to generate a unique name as different types may provide the same
// string as typename. This happens in component builds when cppgc creates
// different internal types for the same C++ class when it is instantiated from
// different libraries.
std::string GetUniqueName(std::string name, size_t id) {
  std::stringstream stream;
  // Convert the id to hex to avoid it reading like an object count.
  stream << name << " (0x" << std::hex << id << ")";
  return stream.str();
}

}  // namespace

BlinkGCMemoryDumpProvider::BlinkGCMemoryDumpProvider(
    ThreadState* thread_state,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    BlinkGCMemoryDumpProvider::HeapType heap_type)
    : thread_state_(thread_state),
      heap_type_(heap_type),
      dump_base_name_(
          "blink_gc/" + std::string(HeapTypeString(heap_type_)) +
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

namespace {

template <typename Stats>
size_t GetFragmentation(const Stats& stats) {
  // Any memory that is not used by objects but part of the resident contributes
  // to fragmentation.
  return stats.resident_size_bytes == 0
             ? 0
             : 100 * (stats.resident_size_bytes - stats.used_size_bytes) /
                   stats.resident_size_bytes;
}

}  // namespace

bool BlinkGCMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  if ((args.determinism ==
       base::trace_event::MemoryDumpDeterminism::kForceGc) &&
      thread_state_->isolate_) {
    // Memory dumps are asynchronous and the MemoryDumpDeterminism::kForceGc
    // flag indicates that we want the dump to be precise and without garbage.
    // Trigger a unified heap GC in V8 (using the same API DevTools uses in
    // "collectGarbage") to eliminate as much garbage as possible.
    // It is not sufficient to rely on a GC from the V8 dump provider since the
    // order between the V8 dump provider and this one is unknown, and this
    // provider may run before the V8 one.
    thread_state_->isolate_->LowMemoryNotification();
  }

  ::cppgc::HeapStatistics::DetailLevel detail_level =
      args.level_of_detail ==
              base::trace_event::MemoryDumpLevelOfDetail::kDetailed
          ? ::cppgc::HeapStatistics::kDetailed
          : ::cppgc::HeapStatistics::kBrief;

  ::cppgc::HeapStatistics stats =
      ThreadState::Current()->cpp_heap().CollectStatistics(detail_level);

  auto* heap_dump =
      process_memory_dump->CreateAllocatorDump(dump_base_name_ + "/heap");
  heap_dump->AddScalar("committed_size",
                       base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                       stats.committed_size_bytes);
  heap_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                       base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                       stats.resident_size_bytes);
  heap_dump->AddScalar("allocated_objects_size",
                       base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                       stats.used_size_bytes);
  heap_dump->AddScalar("pooled_size",
                       base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                       stats.pooled_memory_size_bytes);
  heap_dump->AddScalar("fragmentation", "percent", GetFragmentation(stats));

  if (detail_level == ::cppgc::HeapStatistics::kBrief) {
    return true;
  }

  // Aggregate global object stats from per page statistics.
  std::vector<cppgc::HeapStatistics::ObjectStatsEntry> global_object_stats;
  global_object_stats.resize(stats.type_names.size());

  // Detailed statistics follow.
  for (const ::cppgc::HeapStatistics::SpaceStatistics& space_stats :
       stats.space_stats) {
    auto* space_dump = process_memory_dump->CreateAllocatorDump(
        heap_dump->absolute_name() + "/" + space_stats.name);
    space_dump->AddScalar("committed_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_stats.committed_size_bytes);
    space_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_stats.resident_size_bytes);
    space_dump->AddScalar("allocated_objects_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_stats.used_size_bytes);
    space_dump->AddScalar("fragmentation", "percent",
                          GetFragmentation(space_stats));

    size_t page_count = 0;
    for (const ::cppgc::HeapStatistics::PageStatistics& page_stats :
         space_stats.page_stats) {
      auto* page_dump = process_memory_dump->CreateAllocatorDump(
          space_dump->absolute_name() + "/pages/page_" +
          base::NumberToString(page_count++));
      page_dump->AddScalar("committed_size",
                           base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                           page_stats.committed_size_bytes);
      page_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                           base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                           page_stats.resident_size_bytes);
      page_dump->AddScalar("allocated_objects_size",
                           base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                           page_stats.used_size_bytes);
      page_dump->AddScalar("fragmentation", "percent",
                           GetFragmentation(page_stats));

      const auto& object_stats = page_stats.object_statistics;
      for (size_t i = 0; i < object_stats.size(); i++) {
        if (!object_stats[i].object_count)
          continue;

        auto* page_class_dump = process_memory_dump->CreateAllocatorDump(
            page_dump->absolute_name() + "/types/" +
            GetUniqueName(stats.type_names[i], i));
        page_class_dump->AddScalar(
            base::trace_event::MemoryAllocatorDump::kNameObjectCount,
            base::trace_event::MemoryAllocatorDump::kUnitsObjects,
            object_stats[i].object_count);
        page_class_dump->AddScalar(
            "allocated_objects_size",
            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
            object_stats[i].allocated_bytes);

        RecordType(global_object_stats, object_stats[i], i);
      }
    }

    const ::cppgc::HeapStatistics::FreeListStatistics& free_list_stats =
        space_stats.free_list_stats;
    for (size_t i = 0; i < free_list_stats.bucket_size.size(); ++i) {
      constexpr size_t kDigits = 8;
      std::string original_bucket_size =
          base::NumberToString(free_list_stats.bucket_size[i]);
      std::string padded_bucket_size =
          std::string(kDigits - original_bucket_size.length(), '0') +
          original_bucket_size;
      auto* free_list_bucket_dump = process_memory_dump->CreateAllocatorDump(
          space_dump->absolute_name() + "/freelist/bucket_" +
          padded_bucket_size);
      free_list_bucket_dump->AddScalar(
          "free_slot_count",
          base::trace_event::MemoryAllocatorDump::kUnitsObjects,
          free_list_stats.free_count[i]);
      free_list_bucket_dump->AddScalar(
          "free_usable_size",
          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
          free_list_stats.free_size[i]);
    }
  }

  // Populate "allocated_objects" and "blink_objects/blink_gc" dumps.
  const auto* allocated_objects_dump = process_memory_dump->CreateAllocatorDump(
      dump_base_name_ + "/allocated_objects");
  for (size_t i = 0; i < global_object_stats.size(); i++) {
    auto* details = process_memory_dump->CreateAllocatorDump(
        "blink_objects/" + dump_base_name_ + "/" +
        GetUniqueName(stats.type_names[i], i));
    details->AddScalar("allocated_objects_size",
                       base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                       global_object_stats[i].allocated_bytes);
    details->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                       base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                       global_object_stats[i].object_count);
    details->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                       base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                       global_object_stats[i].allocated_bytes);
    process_memory_dump->AddSuballocation(
        details->guid(), dump_base_name_ + "/allocated_objects");
  }
  process_memory_dump->AddOwnershipEdge(allocated_objects_dump->guid(),
                                        heap_dump->guid());

  return true;
}

}  // namespace blink
