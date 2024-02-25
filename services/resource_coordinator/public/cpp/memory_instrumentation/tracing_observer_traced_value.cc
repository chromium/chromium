// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_traced_value.h"

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"

namespace memory_instrumentation {

using base::trace_event::ProcessMemoryDump;
using base::trace_event::TracedValue;

namespace {

void OsDumpAsValueInto(TracedValue* value, const mojom::OSMemDump& os_dump) {
  value->SetString(
      "private_footprint_bytes",
      base::StringPrintf(
          "%" PRIx64,
          static_cast<uint64_t>(os_dump.private_footprint_kb) * 1024));
  value->SetString(
      "peak_resident_set_size",
      base::StringPrintf(
          "%" PRIx64,
          static_cast<uint64_t>(os_dump.peak_resident_set_kb) * 1024));
  value->SetBoolean("is_peak_rss_resettable", os_dump.is_peak_rss_resettable);
}

}  // namespace

TracingObserverTracedValue::TracingObserverTracedValue(
    base::trace_event::TraceLog* trace_log,
    base::trace_event::MemoryDumpManager* memory_dump_manager) {}

TracingObserverTracedValue::~TracingObserverTracedValue() = default;

// static
void TracingObserverTracedValue::AddToTrace(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::ProcessId pid,
    std::unique_ptr<TracedValue> traced_value) {
  CHECK_NE(base::trace_event::MemoryDumpType::kSummaryOnly, args.dump_type);

  traced_value->SetString(
      "level_of_detail",
      base::trace_event::MemoryDumpLevelOfDetailToString(args.level_of_detail));
  const uint64_t dump_guid = args.dump_guid;
  const char* const event_name =
      base::trace_event::MemoryDumpTypeToString(args.dump_type);
  base::trace_event::TraceArguments trace_args("dumps",
                                               std::move(traced_value));
  TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
      TRACE_EVENT_PHASE_MEMORY_DUMP,
      base::trace_event::TraceLog::GetCategoryGroupEnabled(
          base::trace_event::MemoryDumpManager::kTraceCategory),
      event_name, trace_event_internal::kGlobalScope, dump_guid, pid,
      &trace_args, TRACE_EVENT_FLAG_HAS_ID);
}

bool TracingObserverTracedValue::AddChromeDumpToTraceIfEnabled(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::ProcessId pid,
    const ProcessMemoryDump* process_memory_dump,
    const base::TimeTicks& timastamp) {
  if (!ShouldAddToTrace(args))
    return false;

  std::unique_ptr<TracedValue> traced_value = std::make_unique<TracedValue>();
  process_memory_dump->SerializeAllocatorDumpsInto(traced_value.get());

  AddToTrace(args, pid, std::move(traced_value));

  return true;
}

bool TracingObserverTracedValue::AddOsDumpToTraceIfEnabled(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::ProcessId pid,
    const mojom::OSMemDump& os_dump,
    const std::vector<mojom::VmRegionPtr>& memory_maps,
    const base::TimeTicks& timestamp) {
  if (!ShouldAddToTrace(args))
    return false;

  std::unique_ptr<TracedValue> traced_value = std::make_unique<TracedValue>();

  traced_value->BeginDictionary("process_totals");
  OsDumpAsValueInto(traced_value.get(), os_dump);
  traced_value->EndDictionary();

  if (memory_maps.size()) {
    traced_value->BeginDictionary("process_mmaps");
    MemoryMapsAsValueInto(memory_maps, traced_value.get(), false);
    traced_value->EndDictionary();
  }

  AddToTrace(args, pid, std::move(traced_value));
  return true;
}

// static
void TracingObserverTracedValue::MemoryMapsAsValueInto(
    const std::vector<mojom::VmRegionPtr>& memory_maps,
    TracedValue* value,
    bool is_argument_filtering_enabled) {
  static const char kHexFmt[] = "%" PRIx64;

  // Refer to the design doc goo.gl/sxfFY8 for the semantics of these fields.
  value->BeginArray("vm_regions");
  for (const auto& region : memory_maps) {
    value->BeginDictionary();

    value->SetString("sa", base::StringPrintf(kHexFmt, region->start_address));
    value->SetString("sz", base::StringPrintf(kHexFmt, region->size_in_bytes));
    if (region->module_timestamp)
      value->SetString("ts",
                       base::StringPrintf(kHexFmt, region->module_timestamp));
    if (!region->module_debugid.empty())
      value->SetString("id", region->module_debugid);
    if (!region->module_debug_path.empty()) {
      value->SetString("df", ApplyPathFiltering(region->module_debug_path,
                                                is_argument_filtering_enabled));
    }
    value->SetInteger("pf", region->protection_flags);

    // The module path will be the basename when argument filtering is
    // activated. The whitelisting implemented for filtering string values
    // doesn't allow rewriting. Therefore, a different path is produced here
    // when argument filtering is activated.
    value->SetString("mf", ApplyPathFiltering(region->mapped_file,
                                              is_argument_filtering_enabled));

// The following stats are only well defined on Linux-derived OSes.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
    value->BeginDictionary("bs");  // byte stats
    value->SetString(
        "pss",
        base::StringPrintf(kHexFmt, region->byte_stats_proportional_resident));
    value->SetString(
        "pd",
        base::StringPrintf(kHexFmt, region->byte_stats_private_dirty_resident));
    value->SetString(
        "pc",
        base::StringPrintf(kHexFmt, region->byte_stats_private_clean_resident));
    value->SetString(
        "sd",
        base::StringPrintf(kHexFmt, region->byte_stats_shared_dirty_resident));
    value->SetString(
        "sc",
        base::StringPrintf(kHexFmt, region->byte_stats_shared_clean_resident));
    value->SetString("sw",
                     base::StringPrintf(kHexFmt, region->byte_stats_swapped));
    value->EndDictionary();
#endif

    value->EndDictionary();
  }
  value->EndArray();
}

}  // namespace memory_instrumentation
