// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_proto.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/trace_test_utils.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/traced_value.h"
#include "base/tracing/trace_time.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/trace/memory_graph.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/smaps.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/ps/process_stats.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

using TrackEvent = perfetto::protos::pbzero::TrackEvent;
using MemoryTrackerSnapshot = perfetto::protos::MemoryTrackerSnapshot;

namespace tracing {

namespace {

class TracingObserverProtoTest : public testing::Test {
 public:
  void SetUp() override {
    base::trace_event::MemoryDumpManager::GetInstance()->Initialize(
        base::BindLambdaForTesting(
            [](base::trace_event::MemoryDumpType,
               base::trace_event::MemoryDumpLevelOfDetail) {}),
        false);
    memory_instrumentation::TracingObserverProto::GetInstance()
        ->ResetForTesting();
    tracing::PerfettoTracedProcess::SetSystemProducerEnabledForTesting(false);
    PerfettoTracedProcess::GetTaskRunner()->ResetTaskRunnerForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    base::trace_event::MemoryDumpManager::GetInstance()->ResetForTesting();
  }

  static base::trace_event::TraceConfig GetTraceConfig() {
    return base::trace_event::TraceConfig(
        std::string(base::trace_event::MemoryDumpManager::kTraceCategory) +
            ",-*",
        base::trace_event::RECORD_UNTIL_FULL);
  }

  base::trace_event::MemoryDumpRequestArgs FillMemoryDumpRequestArgs() {
    base::trace_event::MemoryDumpRequestArgs args;
    args.dump_guid = 1;
    args.dump_type = base::trace_event::MemoryDumpType::kExplicitlyTriggered;
    args.level_of_detail =
        base::trace_event::MemoryDumpLevelOfDetail::kDetailed;
    args.determinism = base::trace_event::MemoryDumpDeterminism::kForceGc;

    return args;
  }

  base::trace_event::ProcessMemoryDump FillSamplePmd() {
    base::trace_event::MemoryDumpArgs dump_args = {
        base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
    base::trace_event::ProcessMemoryDump pmd =
        base::trace_event::ProcessMemoryDump(dump_args);
    pmd.CreateAllocatorDump("mad1",
                            base::trace_event::MemoryAllocatorDumpGuid(421));
    pmd.CreateAllocatorDump("mad2",
                            base::trace_event::MemoryAllocatorDumpGuid(422));
    pmd.CreateAllocatorDump("mad3",
                            base::trace_event::MemoryAllocatorDumpGuid(423));
    pmd.AddOwnershipEdge(base::trace_event::MemoryAllocatorDumpGuid(421),
                         base::trace_event::MemoryAllocatorDumpGuid(422));
    pmd.AddOwnershipEdge(base::trace_event::MemoryAllocatorDumpGuid(422),
                         base::trace_event::MemoryAllocatorDumpGuid(423));

    return pmd;
  }

 protected:
  base::test::TracingEnvironment tracing_environment_;
  base::test::TaskEnvironment task_environment_;
};

const base::ProcessId kTestPid = 1;
const int kRegionsCount = 3;

const uint32_t kResidentSetKb = 1;
const uint32_t kPrivateFootprintKb = 2;
const uint32_t kSharedFootprintKb = 3;

const base::TimeTicks kTimestamp =
    base::TimeTicks() + base::Microseconds(100000);
const uint64_t kTimestampProto = kTimestamp.since_origin().InNanoseconds();

uint64_t GetFakeAddrForVmRegion(int pid, int region_index) {
  return 0x100000ul * pid * (region_index + 1);
}

uint64_t GetFakeSizeForVmRegion(int pid, int region_index) {
  return 4096 * pid * (region_index + 1);
}

std::vector<memory_instrumentation::mojom::VmRegionPtr> FillMemoryMap(int pid) {
  std::vector<memory_instrumentation::mojom::VmRegionPtr> memory_map;

  for (int i = 0; i < kRegionsCount; i++) {
    memory_instrumentation::mojom::VmRegionPtr vm_region =
        memory_instrumentation::mojom::VmRegion::New();
    vm_region->start_address = GetFakeAddrForVmRegion(pid, i);
    vm_region->size_in_bytes = GetFakeSizeForVmRegion(pid, i);
    memory_map.push_back(std::move(vm_region));
  }
  return memory_map;
}

memory_instrumentation::mojom::OSMemDump GetFakeOSMemDump(
    uint32_t resident_set_kb,
    uint32_t private_footprint_kb,
    uint32_t shared_footprint_kb) {
  return memory_instrumentation::mojom::OSMemDump(
      resident_set_kb, /*peak_resident_set_kb=*/resident_set_kb,
      /*is_peak_rss_resettable=*/true, private_footprint_kb, shared_footprint_kb
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
      ,
      0
#endif
  );
}

// crbug.com/1242040: flaky on linux, chromeos
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_AddChromeDumpToTraceIfEnabled_When_TraceLog_Disabled \
  DISABLED_AddChromeDumpToTraceIfEnabled_When_TraceLog_Disabled
#else
#define MAYBE_AddChromeDumpToTraceIfEnabled_When_TraceLog_Disabled \
  AddChromeDumpToTraceIfEnabled_When_TraceLog_Disabled
#endif
TEST_F(TracingObserverProtoTest,
       MAYBE_AddChromeDumpToTraceIfEnabled_When_TraceLog_Disabled) {
  auto* tracing_observer =
      memory_instrumentation::TracingObserverProto::GetInstance();
  tracing::DataSourceTester data_source_tester(tracing_observer);

  base::trace_event::MemoryDumpRequestArgs args = FillMemoryDumpRequestArgs();

  base::trace_event::ProcessMemoryDump pmd = FillSamplePmd();

  EXPECT_FALSE(tracing_observer->AddChromeDumpToTraceIfEnabled(
      args, kTestPid, &pmd, kTimestamp));

  data_source_tester.BeginTrace(GetTraceConfig());

  EXPECT_TRUE(tracing_observer->AddChromeDumpToTraceIfEnabled(
      args, kTestPid, &pmd, kTimestamp));

  data_source_tester.EndTracing();
}

TEST_F(TracingObserverProtoTest,
       AddOsDumpToTraceIfEnabled_When_TraceLog_Disabled) {
  auto* tracing_observer =
      memory_instrumentation::TracingObserverProto::GetInstance();
  tracing::DataSourceTester data_source_tester(tracing_observer);

  perfetto::DataSourceConfig config;

  base::trace_event::MemoryDumpRequestArgs args = FillMemoryDumpRequestArgs();

  memory_instrumentation::mojom::OSMemDump os_dump = GetFakeOSMemDump(1, 1, 1);

  std::vector<memory_instrumentation::mojom::VmRegionPtr> memory_map =
      FillMemoryMap(kTestPid);
  EXPECT_FALSE(tracing_observer->AddOsDumpToTraceIfEnabled(
      args, kTestPid, os_dump, memory_map, kTimestamp));

  data_source_tester.BeginTrace(GetTraceConfig());

  EXPECT_TRUE(tracing_observer->AddOsDumpToTraceIfEnabled(
      args, kTestPid, os_dump, memory_map, kTimestamp));

  data_source_tester.EndTracing();
}

TEST_F(TracingObserverProtoTest, AddChromeDumpToTraceIfEnabled) {
  auto* tracing_observer =
      memory_instrumentation::TracingObserverProto::GetInstance();
  tracing::DataSourceTester data_source_tester(tracing_observer);
  data_source_tester.BeginTrace(GetTraceConfig());
  base::trace_event::MemoryDumpRequestArgs args = FillMemoryDumpRequestArgs();

  base::trace_event::ProcessMemoryDump pmd = FillSamplePmd();

  EXPECT_TRUE(tracing_observer->AddChromeDumpToTraceIfEnabled(
      args, kTestPid, &pmd, kTimestamp));
  data_source_tester.EndTracing();

  const perfetto::protos::TracePacket* packet = nullptr;
  for (size_t i = 0; i < data_source_tester.GetFinalizedPacketCount(); ++i) {
    if (data_source_tester.GetFinalizedPacket(i)
            ->has_memory_tracker_snapshot()) {
      packet = data_source_tester.GetFinalizedPacket(i);
      break;
    }
  }
  ASSERT_NE(nullptr, packet);
  EXPECT_TRUE(packet->has_timestamp());
  EXPECT_EQ(kTimestampProto, packet->timestamp());
  EXPECT_TRUE(packet->has_timestamp_clock_id());
  EXPECT_EQ(static_cast<uint32_t>(base::tracing::kTraceClockId),
            packet->timestamp_clock_id());
  EXPECT_TRUE(packet->has_memory_tracker_snapshot());

  const MemoryTrackerSnapshot& snapshot = packet->memory_tracker_snapshot();
  EXPECT_TRUE(snapshot.has_level_of_detail());
  EXPECT_EQ(MemoryTrackerSnapshot::DETAIL_FULL, snapshot.level_of_detail());
  EXPECT_EQ(1, snapshot.process_memory_dumps_size());

  const MemoryTrackerSnapshot::ProcessSnapshot& process_memory_dump =
      snapshot.process_memory_dumps(0);
  EXPECT_EQ(3, process_memory_dump.allocator_dumps_size());

  EXPECT_TRUE(process_memory_dump.has_pid());
  EXPECT_EQ(static_cast<int>(kTestPid), process_memory_dump.pid());

  const MemoryTrackerSnapshot::ProcessSnapshot::MemoryNode& dump0 =
      process_memory_dump.allocator_dumps(0);
  EXPECT_EQ("mad1", dump0.absolute_name());
  const MemoryTrackerSnapshot::ProcessSnapshot::MemoryNode& dump1 =
      process_memory_dump.allocator_dumps(1);
  EXPECT_EQ("mad2", dump1.absolute_name());
  const MemoryTrackerSnapshot::ProcessSnapshot::MemoryNode& dump2 =
      process_memory_dump.allocator_dumps(2);
  EXPECT_EQ("mad3", dump2.absolute_name());

  EXPECT_EQ(2, process_memory_dump.memory_edges_size());

  const MemoryTrackerSnapshot::ProcessSnapshot::MemoryEdge& edge0 =
      process_memory_dump.memory_edges(0);
  EXPECT_EQ(421ul, edge0.source_id());
  EXPECT_EQ(422ul, edge0.target_id());

  const MemoryTrackerSnapshot::ProcessSnapshot::MemoryEdge& edge1 =
      process_memory_dump.memory_edges(1);
  EXPECT_EQ(422ul, edge1.source_id());
  EXPECT_EQ(423ul, edge1.target_id());
}

TEST_F(TracingObserverProtoTest, AddOsDumpToTraceIfEnabled) {
  auto* tracing_observer =
      memory_instrumentation::TracingObserverProto::GetInstance();
  tracing::DataSourceTester data_source_tester(tracing_observer);
  data_source_tester.BeginTrace(GetTraceConfig());

  base::trace_event::MemoryDumpRequestArgs args = FillMemoryDumpRequestArgs();

  memory_instrumentation::mojom::OSMemDump os_dump =
      GetFakeOSMemDump(kResidentSetKb, kPrivateFootprintKb, kSharedFootprintKb);

  std::vector<memory_instrumentation::mojom::VmRegionPtr> memory_map =
      FillMemoryMap(kTestPid);
  EXPECT_TRUE(tracing_observer->AddOsDumpToTraceIfEnabled(
      args, kTestPid, os_dump, memory_map, kTimestamp));
  data_source_tester.EndTracing();

  const perfetto::protos::TracePacket* process_stats_trace_packet = nullptr;
  const perfetto::protos::TracePacket* smaps_trace_packet = nullptr;
  for (size_t i = 0; i < data_source_tester.GetFinalizedPacketCount(); ++i) {
    if (data_source_tester.GetFinalizedPacket(i)->has_process_stats()) {
      process_stats_trace_packet = data_source_tester.GetFinalizedPacket(i);
    } else if (data_source_tester.GetFinalizedPacket(i)->has_smaps_packet()) {
      smaps_trace_packet = data_source_tester.GetFinalizedPacket(i);
    }
  }

  ASSERT_NE(nullptr, process_stats_trace_packet);
  EXPECT_TRUE(process_stats_trace_packet->has_timestamp());
  EXPECT_EQ(kTimestampProto, process_stats_trace_packet->timestamp());
  EXPECT_TRUE(process_stats_trace_packet->has_timestamp_clock_id());
  EXPECT_EQ(static_cast<uint32_t>(base::tracing::kTraceClockId),
            process_stats_trace_packet->timestamp_clock_id());
  EXPECT_TRUE(process_stats_trace_packet->has_process_stats());

  const ::perfetto::protos::ProcessStats& process_stats =
      process_stats_trace_packet->process_stats();
  EXPECT_EQ(1, process_stats.processes_size());

  const ::perfetto::protos::ProcessStats::Process& process =
      process_stats.processes(0);

  EXPECT_TRUE(process.has_pid());
  EXPECT_EQ(static_cast<int>(kTestPid), process.pid());

  EXPECT_TRUE(process.has_chrome_private_footprint_kb());
  EXPECT_EQ(kPrivateFootprintKb, process.chrome_private_footprint_kb());

  EXPECT_TRUE(process.has_chrome_peak_resident_set_kb());
  EXPECT_EQ(kResidentSetKb, process.chrome_peak_resident_set_kb());

  EXPECT_TRUE(process.has_is_peak_rss_resettable());
  EXPECT_TRUE(process.is_peak_rss_resettable());

  EXPECT_NE(nullptr, smaps_trace_packet);
  const ::perfetto::protos::SmapsPacket& smaps_packet =
      smaps_trace_packet->smaps_packet();

  EXPECT_TRUE(smaps_packet.has_pid());
  EXPECT_EQ(static_cast<uint32_t>(kTestPid), smaps_packet.pid());

  EXPECT_EQ(kRegionsCount, smaps_packet.entries_size());
  for (int i = 0; i < kRegionsCount; i++) {
    const ::perfetto::protos::SmapsEntry& entry = smaps_packet.entries(i);

    uint64_t start_address = GetFakeAddrForVmRegion(kTestPid, i);
    EXPECT_EQ(start_address, entry.start_address());

    uint64_t size_kb = GetFakeSizeForVmRegion(kTestPid, i) / 1024;
    EXPECT_EQ(size_kb, entry.size_kb());
  }
}

TEST_F(TracingObserverProtoTest, AsProtoInto) {
  auto* tracing_observer =
      memory_instrumentation::TracingObserverProto::GetInstance();
  tracing::DataSourceTester data_source_tester(tracing_observer);
  data_source_tester.BeginTrace(GetTraceConfig());

  base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump pmd =
      base::trace_event::ProcessMemoryDump(dump_args);

  using MemoryAllocatorDump = base::trace_event::MemoryAllocatorDump;

  MemoryAllocatorDump* dump = pmd.CreateAllocatorDump(
      "mad1", base::trace_event::MemoryAllocatorDumpGuid(421));
  dump->AddScalar("size", MemoryAllocatorDump::kUnitsBytes, 10);
  dump->AddScalar("one", MemoryAllocatorDump::kUnitsBytes, 1);
  dump->AddString("two", MemoryAllocatorDump::kUnitsObjects, "one");

  auto write_dump = [&](perfetto::TraceWriter::TracePacketHandle handle) {
    perfetto::protos::pbzero::MemoryTrackerSnapshot* memory_snapshot =
        handle->set_memory_tracker_snapshot();
    perfetto::protos::pbzero::MemoryTrackerSnapshot::ProcessSnapshot*
        process_snapshot = memory_snapshot->add_process_memory_dumps();
    perfetto::protos::pbzero::MemoryTrackerSnapshot::ProcessSnapshot::
        MemoryNode* memory_node = process_snapshot->add_allocator_dumps();

    dump->AsProtoInto(memory_node);
    handle->Finalize();
  };

  base::TrackEvent::Trace([&](base::TrackEvent::TraceContext ctx) {
    write_dump(ctx.NewTracePacket());
  });
  data_source_tester.EndTracing();

  const perfetto::protos::TracePacket* packet = nullptr;
  for (size_t i = 0; i < data_source_tester.GetFinalizedPacketCount(); ++i) {
    if (data_source_tester.GetFinalizedPacket(i)
            ->has_memory_tracker_snapshot()) {
      packet = data_source_tester.GetFinalizedPacket(i);
      break;
    }
  }
  ASSERT_NE(nullptr, packet);

  const MemoryTrackerSnapshot& snapshot = packet->memory_tracker_snapshot();
  const MemoryTrackerSnapshot::ProcessSnapshot& process_memory_dump =
      snapshot.process_memory_dumps(0);
  EXPECT_EQ(1, process_memory_dump.allocator_dumps_size());

  const MemoryTrackerSnapshot::ProcessSnapshot::MemoryNode& dump0 =
      process_memory_dump.allocator_dumps(0);
  EXPECT_TRUE(dump0.has_absolute_name());
  EXPECT_EQ("mad1", dump0.absolute_name());
  EXPECT_TRUE(dump0.has_id());
  EXPECT_EQ(421ul, dump0.id());
  EXPECT_TRUE(dump0.has_size_bytes());
  EXPECT_EQ(10ul, dump0.size_bytes());
  EXPECT_EQ(2, dump0.entries_size());

  const MemoryTrackerSnapshot::ProcessSnapshot::MemoryNode::MemoryNodeEntry&
      entry0 = dump0.entries(0);
  const MemoryTrackerSnapshot::ProcessSnapshot::MemoryNode::MemoryNodeEntry&
      entry1 = dump0.entries(1);

  EXPECT_TRUE(entry0.has_name());
  EXPECT_EQ("one", entry0.name());
  EXPECT_TRUE(entry0.has_units());
  EXPECT_EQ(MemoryTrackerSnapshot::ProcessSnapshot::MemoryNode::
                MemoryNodeEntry::BYTES,
            entry0.units());
  EXPECT_TRUE(entry0.has_value_uint64());
  EXPECT_EQ(1ul, entry0.value_uint64());

  EXPECT_TRUE(entry1.has_name());
  EXPECT_EQ("two", entry1.name());
  EXPECT_TRUE(entry0.has_units());
  EXPECT_EQ(MemoryTrackerSnapshot::ProcessSnapshot::MemoryNode::
                MemoryNodeEntry::COUNT,
            entry1.units());
  EXPECT_TRUE(entry1.has_value_string());
  EXPECT_EQ("one", entry1.value_string());
}

}  // namespace

}  // namespace tracing
