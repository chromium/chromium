// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/heap_profiling/heap_profiling_data_source.h"

#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/run_loop.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "services/tracing/public/cpp/perfetto/perfetto_data_source_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/sampling_heap_profiler.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

namespace tracing {
namespace {

class HeapProfilingDataSourceTest : public testing::Test {
 public:
  void SetUp() override { HeapProfilingDataSource::Register(); }

  void WaitForEvents() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(150));
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::TracingEnvironment tracing_environment_;
};

TEST_F(HeapProfilingDataSourceTest, EmitSamples) {
  // 1. Start tracing.
  perfetto::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  trace_config.mutable_incremental_state_config()->set_clear_period_ms(100);
  auto* ds_cfg = trace_config.add_data_sources()->mutable_config();
  ds_cfg->set_name(kNativeHeapProfilerSourceName);

  perfetto::protos::gen::ChromiumSamplingHeapProfilerConfig config;
  // Set sampling interval to 1 byte to ensure our allocation is sampled.
  config.set_sampling_interval_bytes(1);
  config.set_sampling_interval_ms(100);
  ds_cfg->set_chromium_sampling_heap_profiler_raw(config.SerializeAsString());

  auto tracing_session =
      perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
  tracing_session->Setup(trace_config);
  tracing_session->StartBlocking();

  // 2. Manually trigger an allocation sample via PoissonAllocationSampler.
  // We need to suppress randomness to make sure it samples.
  base::PoissonAllocationSampler::ScopedSuppressRandomnessForTesting
      suppress_randomness;

  base::allocator::dispatcher::AllocationNotificationData alloc_data(
      reinterpret_cast<void*>(0x1234), 1024, "test_type",
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator);
  base::PoissonAllocationSampler::Get()->OnAllocation(alloc_data);

  // 3. Wait for the dump interval.
  WaitForEvents();

  // Trigger another allocation after the first dump and clear.
  base::allocator::dispatcher::AllocationNotificationData alloc_data2(
      reinterpret_cast<void*>(0x5678), 2048, "test_type2",
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator);
  base::PoissonAllocationSampler::Get()->OnAllocation(alloc_data2);

  // Wait for the second dump interval.
  WaitForEvents();

  // 4. Stop tracing and read.
  base::TrackEvent::Flush();
  base::RunLoop wait_for_stop;
  tracing_session->SetOnStopCallback(
      [&wait_for_stop] { wait_for_stop.Quit(); });
  tracing_session->Stop();
  wait_for_stop.Run();

  std::vector<char> serialized_data = tracing_session->ReadTraceBlocking();
  perfetto::protos::Trace trace;
  EXPECT_TRUE(
      trace.ParseFromArray(serialized_data.data(), serialized_data.size()));

  int heap_samples_count = 0;
  int cleared_packets_count = 0;
  for (const auto& packet : trace.packet()) {
    if (packet.has_stack_sample()) {
      heap_samples_count++;
      const auto& sample = packet.stack_sample();
      EXPECT_TRUE(sample.has_callstack_iid());
    }
    if (packet.sequence_flags() &
        perfetto::protos::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED) {
      cleared_packets_count++;
    }
  }
  EXPECT_GE(heap_samples_count, 2);
  EXPECT_GE(cleared_packets_count, 2);

  // Cleanup.
  base::allocator::dispatcher::FreeNotificationData free_data(
      reinterpret_cast<void*>(0x1234),
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator);
  base::PoissonAllocationSampler::Get()->OnFree(free_data);
  base::allocator::dispatcher::FreeNotificationData free_data2(
      reinterpret_cast<void*>(0x5678),
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator);
  base::PoissonAllocationSampler::Get()->OnFree(free_data2);
}

TEST_F(HeapProfilingDataSourceTest, MultipleInstances) {
  perfetto::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = trace_config.add_data_sources()->mutable_config();
  ds_cfg->set_name(kNativeHeapProfilerSourceName);

  perfetto::protos::gen::ChromiumSamplingHeapProfilerConfig config;
  config.set_sampling_interval_bytes(1);
  config.set_sampling_interval_ms(100);
  ds_cfg->set_chromium_sampling_heap_profiler_raw(config.SerializeAsString());

  // 1. Start tracing session 1.
  auto tracing_session1 =
      perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
  tracing_session1->Setup(trace_config);
  tracing_session1->StartBlocking();

  // 2. Trigger first allocation.
  base::PoissonAllocationSampler::ScopedSuppressRandomnessForTesting
      suppress_randomness;

  base::allocator::dispatcher::AllocationNotificationData alloc_data1(
      reinterpret_cast<void*>(0x1234), 1024, "test_type",
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator);
  base::PoissonAllocationSampler::Get()->OnAllocation(alloc_data1);

  // 3. Start tracing session 2 after the first allocation.
  auto tracing_session2 =
      perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
  tracing_session2->Setup(trace_config);
  tracing_session2->StartBlocking();

  // 4. Trigger second allocation.
  base::allocator::dispatcher::AllocationNotificationData alloc_data2(
      reinterpret_cast<void*>(0x5678), 2048, "test_type2",
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator);
  base::PoissonAllocationSampler::Get()->OnAllocation(alloc_data2);

  // 5. Wait for dump interval.
  WaitForEvents();

  // 6. Stop both sessions.
  base::TrackEvent::Flush();
  base::RunLoop wait_for_stop1;
  tracing_session1->SetOnStopCallback(
      [&wait_for_stop1] { wait_for_stop1.Quit(); });
  tracing_session1->Stop();
  wait_for_stop1.Run();

  base::RunLoop wait_for_stop2;
  tracing_session2->SetOnStopCallback(
      [&wait_for_stop2] { wait_for_stop2.Quit(); });
  tracing_session2->Stop();
  wait_for_stop2.Run();

  // 7. Verify trace 1 contains samples for both allocations.
  std::vector<char> serialized_data1 = tracing_session1->ReadTraceBlocking();
  perfetto::protos::Trace trace1;
  EXPECT_TRUE(
      trace1.ParseFromArray(serialized_data1.data(), serialized_data1.size()));

  int heap_samples_count1 = 0;
  for (const auto& packet : trace1.packet()) {
    if (packet.has_stack_sample()) {
      heap_samples_count1++;
      EXPECT_TRUE(packet.stack_sample().has_callstack_iid());
    }
  }
  EXPECT_GE(heap_samples_count1, 2);

  // 8. Verify trace 2 contains samples only for the second allocation (started
  // after first).
  std::vector<char> serialized_data2 = tracing_session2->ReadTraceBlocking();
  perfetto::protos::Trace trace2;
  EXPECT_TRUE(
      trace2.ParseFromArray(serialized_data2.data(), serialized_data2.size()));

  int heap_samples_count2 = 0;
  for (const auto& packet : trace2.packet()) {
    if (packet.has_stack_sample()) {
      heap_samples_count2++;
      EXPECT_TRUE(packet.stack_sample().has_callstack_iid());
    }
  }
  EXPECT_GE(heap_samples_count2, 1);
  EXPECT_LT(heap_samples_count2, heap_samples_count1);

  // 9. Cleanup.
  base::allocator::dispatcher::FreeNotificationData free_data1(
      reinterpret_cast<void*>(0x1234),
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator);
  base::PoissonAllocationSampler::Get()->OnFree(free_data1);
  base::allocator::dispatcher::FreeNotificationData free_data2(
      reinterpret_cast<void*>(0x5678),
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator);
  base::PoissonAllocationSampler::Get()->OnFree(free_data2);
}

}  // namespace
}  // namespace tracing
