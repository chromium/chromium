// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_isolate_memory_dump_provider.h"

#include <inttypes.h>
#include <stddef.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "gin/public/isolate_holder.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-locker.h"
#include "v8/include/v8-statistics.h"

namespace gin {

V8IsolateMemoryDumpProvider::V8IsolateMemoryDumpProvider(
    IsolateHolder* isolate_holder,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : isolate_holder_(isolate_holder) {
  DCHECK(task_runner);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "V8Isolate", task_runner);
}

V8IsolateMemoryDumpProvider::~V8IsolateMemoryDumpProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

// Called at trace dump point time. Creates a snapshot with the memory counters
// for the current isolate.
bool V8IsolateMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  // TODO(ssid): Use MemoryDumpArgs to create light dumps when requested
  // (crbug.com/499731).

  if (isolate_holder_->access_mode() == IsolateHolder::kUseLocker) {
    v8::Locker locked(isolate_holder_->isolate());
    DumpHeapStatistics(args, process_memory_dump);
  } else {
    DumpHeapStatistics(args, process_memory_dump);
  }
  return true;
}

namespace {

// Dump statistics related to code/bytecode when memory-infra.v8.code_stats is
// enabled.
void DumpCodeStatistics(base::trace_event::MemoryAllocatorDump* dump,
                        IsolateHolder* isolate_holder) {
  // Collecting code statistics is an expensive operation (~10 ms) when
  // compared to other v8 metrics (< 1 ms). So, dump them only when
  // memory-infra.v8.code_stats is enabled.
  // TODO(primiano): This information should be plumbed through TraceConfig.
  // See crbug.com/616441.
  bool dump_code_stats = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("memory-infra.v8.code_stats"),
      &dump_code_stats);
  if (!dump_code_stats)
    return;

  v8::HeapCodeStatistics code_statistics;
  if (!isolate_holder->isolate()->GetHeapCodeAndMetadataStatistics(
          &code_statistics)) {
    return;
  }

  dump->AddScalar("code_and_metadata_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  code_statistics.code_and_metadata_size());
  dump->AddScalar("bytecode_and_metadata_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  code_statistics.bytecode_and_metadata_size());
  dump->AddScalar("external_script_source_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  code_statistics.external_script_source_size());
  dump->AddScalar("cpu_profiler_metadata_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  code_statistics.cpu_profiler_metadata_size());
}

// Dump the number of native and detached contexts.
// The result looks as follows in the Chrome trace viewer:
// ========================================
// Component                   object_count
// - v8
//   - main
//     - contexts
//       - detached_context  10
//       - native_context    20
//   - workers
//     - contexts
//       - detached_context
//         - isolate_0x1234  10
//       - native_context
//         - isolate_0x1234  20
// ========================================
void DumpContextStatistics(
    base::trace_event::ProcessMemoryDump* process_memory_dump,
    std::string dump_base_name,
    std::string dump_name_suffix,
    size_t number_of_detached_contexts,
    size_t number_of_native_contexts) {
  std::string dump_name_prefix = dump_base_name + "/contexts";
  std::string native_context_name =
      dump_name_prefix + "/native_context" + dump_name_suffix;
  auto* native_context_dump =
      process_memory_dump->CreateAllocatorDump(native_context_name);
  native_context_dump->AddScalar(
      "object_count", base::trace_event::MemoryAllocatorDump::kUnitsObjects,
      number_of_native_contexts);
  std::string detached_context_name =
      dump_name_prefix + "/detached_context" + dump_name_suffix;
  auto* detached_context_dump =
      process_memory_dump->CreateAllocatorDump(detached_context_name);
  detached_context_dump->AddScalar(
      "object_count", base::trace_event::MemoryAllocatorDump::kUnitsObjects,
      number_of_detached_contexts);
}

std::string IsolateTypeString(IsolateHolder::IsolateType isolate_type) {
  switch (isolate_type) {
    case IsolateHolder::IsolateType::kBlinkMainThread:
      return "main";
    case IsolateHolder::IsolateType::kBlinkWorkerThread:
      return "workers";
    case IsolateHolder::IsolateType::kTest:
      NOTREACHED();
    case IsolateHolder::IsolateType::kUtility:
      return "utility";
  }
  NOTREACHED();
}

bool CanHaveMultipleIsolates(IsolateHolder::IsolateType isolate_type) {
  switch (isolate_type) {
    case IsolateHolder::IsolateType::kBlinkMainThread:
      return false;
    case IsolateHolder::IsolateType::kBlinkWorkerThread:
      return true;
    case IsolateHolder::IsolateType::kTest:
      NOTREACHED();
    case IsolateHolder::IsolateType::kUtility:
      // PDFium and ProxyResolver create one isolate per process.
      return false;
  }
  NOTREACHED();
}

}  // namespace

void V8IsolateMemoryDumpProvider::DumpHeapStatistics(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  if (args.determinism == base::trace_event::MemoryDumpDeterminism::kForceGc) {
    // Force GC in V8 using the same API as DevTools uses in "collectGarbage".
    isolate_holder_->isolate()->LowMemoryNotification();
  }
  std::string isolate_name = base::StringPrintf(
      "isolate_0x%" PRIXPTR,
      reinterpret_cast<uintptr_t>(isolate_holder_->isolate()));

  // Dump statistics of the heap's spaces.
  v8::HeapStatistics heap_statistics;
  // The total heap sizes should be sampled before the individual space sizes
  // because of concurrent allocation. DCHECKs below rely on this order.
  isolate_holder_->isolate()->GetHeapStatistics(&heap_statistics);

  IsolateHolder::IsolateType isolate_type = isolate_holder_->isolate_type();
  std::string dump_base_name = "v8/" + IsolateTypeString(isolate_type);
  std::string dump_name_suffix =
      CanHaveMultipleIsolates(isolate_type) ? "/" + isolate_name : "";

  std::string space_name_prefix = dump_base_name + "/heap";

  size_t known_spaces_size = 0;
  size_t known_spaces_physical_size = 0;
  size_t number_of_spaces = isolate_holder_->isolate()->NumberOfHeapSpaces();
  for (size_t space = 0; space < number_of_spaces; space++) {
    v8::HeapSpaceStatistics space_statistics;
    isolate_holder_->isolate()->GetHeapSpaceStatistics(&space_statistics,
                                                       space);
    const size_t space_size = space_statistics.space_size();
    const size_t space_used_size = space_statistics.space_used_size();
    const size_t space_physical_size = space_statistics.physical_space_size();

    known_spaces_size += space_size;
    known_spaces_physical_size += space_physical_size;

    std::string space_dump_name = dump_base_name + "/heap/" +
                                  space_statistics.space_name() +
                                  dump_name_suffix;

    auto* space_dump =
        process_memory_dump->CreateAllocatorDump(space_dump_name);
    space_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_physical_size);
    space_dump->AddScalar("virtual_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_size);

    space_dump->AddScalar("allocated_objects_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_used_size);
  }

  // Sanity check that all spaces are accounted for in GetHeapSpaceStatistics.
  // Background threads may be running and allocating concurrently, so the sum
  // of space sizes may exceed the total heap size that was sampled earlier.
  DCHECK_LE(heap_statistics.total_heap_size(), known_spaces_size);

  // If V8 zaps garbage, all the memory mapped regions become resident,
  // so we add an extra dump to avoid mismatches w.r.t. the total
  // resident values.
  if (heap_statistics.does_zap_garbage()) {
    auto* zap_dump = process_memory_dump->CreateAllocatorDump(
        dump_base_name + "/zapped_for_debug" + dump_name_suffix);
    size_t zapped_size_for_debugging =
        known_spaces_size >= known_spaces_physical_size
            ? known_spaces_size - known_spaces_physical_size
            : 0;
    zap_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        zapped_size_for_debugging);
  }

  // Dump statistics about malloced memory.
  std::string malloc_name = dump_base_name + "/malloc" + dump_name_suffix;
  auto* malloc_dump = process_memory_dump->CreateAllocatorDump(malloc_name);
  malloc_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         heap_statistics.malloced_memory());
  malloc_dump->AddScalar("peak_size",
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         heap_statistics.peak_malloced_memory());
  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (system_allocator_name) {
    process_memory_dump->AddSuballocation(malloc_dump->guid(),
                                          system_allocator_name);
  }

  DumpContextStatistics(process_memory_dump, dump_base_name, dump_name_suffix,
                        heap_statistics.number_of_detached_contexts(),
                        heap_statistics.number_of_native_contexts());

  auto* code_stats_dump = process_memory_dump->CreateAllocatorDump(
      dump_base_name + "/code_stats" + dump_name_suffix);

  // Dump statistics related to code and bytecode if requested.
  DumpCodeStatistics(code_stats_dump, isolate_holder_);

  // Dump statistics for global handles.
  auto* global_handles_dump = process_memory_dump->CreateAllocatorDump(
      dump_base_name + "/global_handles" + dump_name_suffix);
  global_handles_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameSize,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      heap_statistics.total_global_handles_size());
  global_handles_dump->AddScalar(
      "allocated_objects_size",
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      heap_statistics.used_global_handles_size());
  if (system_allocator_name) {
    process_memory_dump->AddSuballocation(global_handles_dump->guid(),
                                          system_allocator_name);
  }

  // Dump object statistics only for detailed dumps.
  if (args.level_of_detail !=
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed) {
    return;
  }

  // Dump statistics of the heap's live objects from last GC.
  // TODO(primiano): these should not be tracked in the same trace event as they
  // report stats for the last GC (not the current state). See crbug.com/498779.
  std::string object_name_prefix =
      dump_base_name + "/heap_objects_at_last_gc" + dump_name_suffix;
  bool did_dump_object_stats = false;
  const size_t object_types =
      isolate_holder_->isolate()->NumberOfTrackedHeapObjectTypes();
  for (size_t type_index = 0; type_index < object_types; type_index++) {
    v8::HeapObjectStatistics object_statistics;
    if (!isolate_holder_->isolate()->GetHeapObjectStatisticsAtLastGC(
            &object_statistics, type_index))
      continue;

    std::string dump_name =
        object_name_prefix + "/" + object_statistics.object_type();
    if (object_statistics.object_sub_type()[0] != '\0')
      dump_name += std::string("/") + object_statistics.object_sub_type();
    auto* object_dump = process_memory_dump->CreateAllocatorDump(dump_name);

    object_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameObjectCount,
        base::trace_event::MemoryAllocatorDump::kUnitsObjects,
        object_statistics.object_count());
    object_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                           base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                           object_statistics.object_size());
    did_dump_object_stats = true;
  }

  if (process_memory_dump->GetAllocatorDump(object_name_prefix +
                                            "/CODE_TYPE")) {
    auto* code_kind_dump = process_memory_dump->CreateAllocatorDump(
        object_name_prefix + "/CODE_TYPE/CODE_KIND");
    auto* code_age_dump = process_memory_dump->CreateAllocatorDump(
        object_name_prefix + "/CODE_TYPE/CODE_AGE");
    process_memory_dump->AddOwnershipEdge(code_kind_dump->guid(),
                                          code_age_dump->guid());
  }

  if (did_dump_object_stats) {
    process_memory_dump->AddOwnershipEdge(
        process_memory_dump->CreateAllocatorDump(object_name_prefix)->guid(),
        process_memory_dump->GetOrCreateAllocatorDump(space_name_prefix)
            ->guid());
  }
}

}  // namespace gin
