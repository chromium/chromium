// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_proto.h"

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/trace_time.h"
#include "third_party/perfetto/protos/perfetto/trace/memory_graph.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/smaps.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/ps/process_stats.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace memory_instrumentation {

using base::trace_event::ProcessMemoryDump;

namespace {

void OsDumpAsProtoInto(perfetto::protos::pbzero::ProcessStats::Process* process,
                       const mojom::OSMemDump& os_dump) {
  process->set_chrome_private_footprint_kb(os_dump.private_footprint_kb);
  process->set_chrome_peak_resident_set_kb(os_dump.peak_resident_set_kb);
  process->set_is_peak_rss_resettable(os_dump.is_peak_rss_resettable);
}

perfetto::protos::pbzero::MemoryTrackerSnapshot::LevelOfDetail
MemoryDumpLevelOfDetailToProto(
    const base::trace_event::MemoryDumpLevelOfDetail& level_of_detail) {
  switch (level_of_detail) {
    case base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND:
      return perfetto::protos::pbzero::MemoryTrackerSnapshot::DETAIL_BACKGROUND;
    case base::trace_event::MemoryDumpLevelOfDetail::LIGHT:
      return perfetto::protos::pbzero::MemoryTrackerSnapshot::DETAIL_LIGHT;
    case base::trace_event::MemoryDumpLevelOfDetail::DETAILED:
      return perfetto::protos::pbzero::MemoryTrackerSnapshot::DETAIL_FULL;
  }
  NOTREACHED();
  return perfetto::protos::pbzero::MemoryTrackerSnapshot::DETAIL_BACKGROUND;
}

}  // namespace

TracingObserverProto::TracingObserverProto(
    base::trace_event::TraceLog* trace_log,
    base::trace_event::MemoryDumpManager* memory_dump_manager)
    : TracingObserver(trace_log, memory_dump_manager),
      tracing::PerfettoTracedProcess::DataSourceBase(
          tracing::mojom::kMemoryInstrumentationDataSourceName) {
  tracing::PerfettoTracedProcess::Get()->AddDataSource(this);
}

TracingObserverProto::~TracingObserverProto() = default;

bool TracingObserverProto::AddChromeDumpToTraceIfEnabled(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::ProcessId pid,
    const ProcessMemoryDump* process_memory_dump,
    const base::TimeTicks& timestamp) {
  if (!ShouldAddToTrace(args))
    return false;

  base::AutoLock lock(producer_lock_);

  if (!trace_writer_)
    return false;

  perfetto::TraceWriter::TracePacketHandle handle =
      trace_writer_->NewTracePacket();
  handle->set_timestamp(timestamp.since_origin().InNanoseconds());
  handle->set_timestamp_clock_id(tracing::kTraceClockId);
  perfetto::protos::pbzero::MemoryTrackerSnapshot* memory_snapshot =
      handle->set_memory_tracker_snapshot();
  memory_snapshot->set_level_of_detail(
      MemoryDumpLevelOfDetailToProto(args.level_of_detail));
  process_memory_dump->SerializeAllocatorDumpsInto(memory_snapshot, pid);

  return true;
}

bool TracingObserverProto::AddOsDumpToTraceIfEnabled(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::ProcessId pid,
    const mojom::OSMemDump& os_dump,
    const std::vector<mojom::VmRegionPtr>& memory_maps,
    const base::TimeTicks& timestamp) {
  if (!ShouldAddToTrace(args))
    return false;

  base::AutoLock lock(producer_lock_);

  if (!trace_writer_)
    return false;

  perfetto::TraceWriter::TracePacketHandle process_stats_packet =
      trace_writer_->NewTracePacket();
  process_stats_packet->set_timestamp(timestamp.since_origin().InNanoseconds());
  process_stats_packet->set_timestamp_clock_id(tracing::kTraceClockId);
  perfetto::protos::pbzero::ProcessStats* process_stats =
      process_stats_packet->set_process_stats();
  perfetto::protos::pbzero::ProcessStats::Process* process =
      process_stats->add_processes();
  process->set_pid(static_cast<int>(pid));

  OsDumpAsProtoInto(process, os_dump);

  process_stats_packet->Finalize();

  if (memory_maps.size()) {
    perfetto::TraceWriter::TracePacketHandle smaps_packet =
        trace_writer_->NewTracePacket();
    smaps_packet->set_timestamp(timestamp.since_origin().InNanoseconds());
    smaps_packet->set_timestamp_clock_id(tracing::kTraceClockId);
    perfetto::protos::pbzero::SmapsPacket* smaps =
        smaps_packet->set_smaps_packet();
    smaps->set_pid(static_cast<uint32_t>(pid));

    MemoryMapsAsProtoInto(memory_maps, smaps, false);

    smaps_packet->Finalize();
  }

  return true;
}

void TracingObserverProto::StartTracing(
    tracing::PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  base::AutoLock lock(producer_lock_);
  producer_ = producer;
  // We rely on concurrent setup of TraceLog categories by the
  // TraceEventDataSource so don't look at the trace config ourselves.
  trace_writer_ =
      producer->CreateTraceWriter(data_source_config.target_buffer());
}

void TracingObserverProto::StopTracing(
    base::OnceClosure stop_complete_callback) {
  // Scope to avoid reentrancy in case from the stop callback.
  {
    base::AutoLock lock(producer_lock_);
    trace_writer_.reset();
    producer_ = nullptr;
  }

  if (stop_complete_callback) {
    std::move(stop_complete_callback).Run();
  }
}

void TracingObserverProto::Flush(
    base::RepeatingClosure flush_complete_callback) {
  base::AutoLock lock(producer_lock_);
  if (trace_writer_)
    trace_writer_->Flush();
}

void TracingObserverProto::MemoryMapsAsProtoInto(
    const std::vector<mojom::VmRegionPtr>& memory_maps,
    perfetto::protos::pbzero::SmapsPacket* smaps,
    bool is_argument_filtering_enabled) {
  for (const auto& region : memory_maps) {
    perfetto::protos::pbzero::SmapsEntry* entry = smaps->add_entries();

    entry->set_start_address(region->start_address);
    entry->set_size_kb(region->size_in_bytes / 1024);

    if (region->module_timestamp)
      entry->set_module_timestamp(region->module_timestamp);
    if (!region->module_debugid.empty())
      entry->set_module_debugid(region->module_debugid);
    if (!region->module_debug_path.empty()) {
      entry->set_module_debug_path(ApplyPathFiltering(
          region->module_debug_path, is_argument_filtering_enabled));
    }
    entry->set_protection_flags(region->protection_flags);

    entry->set_file_name(
        ApplyPathFiltering(region->mapped_file, is_argument_filtering_enabled));

// The following stats are only well defined on Linux-derived OSes.
#if !defined(OS_MAC) && !defined(OS_WIN)
    entry->set_proportional_resident_kb(
        region->byte_stats_proportional_resident / 1024);
    entry->set_private_dirty_kb(region->byte_stats_private_dirty_resident /
                                1024);
    entry->set_private_clean_resident_kb(
        region->byte_stats_private_clean_resident / 1024);
    entry->set_shared_dirty_resident_kb(
        region->byte_stats_shared_dirty_resident / 1024);
    entry->set_shared_clean_resident_kb(
        region->byte_stats_shared_clean_resident / 1024);
    entry->set_swap_kb(region->byte_stats_swapped / 1024);
#endif
  }
}

}  // namespace memory_instrumentation
