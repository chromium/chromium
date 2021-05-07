// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "base/trace_event/category_registry.h"
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_category.h"
#include "base/trace_event/trace_event_filter.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/trace_log.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"

namespace {

// static
// Cache of the last TraceEvent seen by TestTraceEventFilter, as it cannot be
// stored directly in the class, due to the filtering methods being const.
std::unique_ptr<base::trace_event::TraceEvent> g_trace_event;

// Testing filter to observe "gpu" trace events. The latest one seen is copied
// into |g_trace_event|.
class TestTraceEventFilter : public base::trace_event::TraceEventFilter {
 public:
  TestTraceEventFilter() { g_trace_event.reset(); }
  ~TestTraceEventFilter() override { g_trace_event.reset(); }

  static std::unique_ptr<base::trace_event::TraceEventFilter> Factory(
      const std::string& predicate_name) {
    std::unique_ptr<TestTraceEventFilter> res =
        std::make_unique<TestTraceEventFilter>();
    return res;
  }

  // base::trace_event::TraceEventFilter:
  bool FilterTraceEvent(
      const base::trace_event::TraceEvent& trace_event) const override {
    const auto* category =
        base::trace_event::CategoryRegistry::GetCategoryByStatePtr(
            trace_event.category_group_enabled());

    if (!strcmp(category->name(), "gpu")) {
      CHECK_EQ(2u, trace_event.arg_size()) << trace_event.name();
      // The first arg is always recorded as a uint64_t, whereas the second is
      // a TracedValue. Here we force the first to be recorded as_uint, as on
      // KitKat the union is failing to transpose correctly when using
      // as_convertable.
      std::unique_ptr<base::trace_event::TraceArguments> args =
          std::make_unique<base::trace_event::TraceArguments>(
              trace_event.arg_name(0), trace_event.arg_value(0).as_uint,
              trace_event.arg_name(1),
              static_cast<void*>(trace_event.arg_value(1).as_convertable));

      g_trace_event = std::make_unique<base::trace_event::TraceEvent>(
          trace_event.thread_id(), trace_event.timestamp(),
          trace_event.thread_timestamp(),
          trace_event.thread_instruction_count(), trace_event.phase(),
          trace_event.category_group_enabled(), trace_event.name(),
          trace_event.scope(), trace_event.id(), trace_event.bind_id(),
          args.get(), trace_event.flags());
    }
    return true;
  }
};

}  // namespace

namespace gpu {

class GpuChannelManagerTest : public GpuChannelTestCommon {
 public:
  static constexpr uint64_t kUInt64_T_Max =
      std::numeric_limits<uint64_t>::max();

  GpuChannelManagerTest()
      : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~GpuChannelManagerTest() override = default;

  GpuChannelManager::GpuPeakMemoryMonitor* gpu_peak_memory_monitor() {
    return &channel_manager()->peak_memory_monitor_;
  }

  // Returns the peak memory usage from the channel_manager(). This will stop
  // tracking for |sequence_number|.
  uint64_t GetManagersPeakMemoryUsage(uint32_t sequence_num) {
    // Set default as max so that invalid cases can properly test 0u returns.
    uint64_t peak_memory = kUInt64_T_Max;
    auto allocation =
        channel_manager()->GetPeakMemoryUsage(sequence_num, &peak_memory);
    return peak_memory;
  }

  // Returns the peak memory usage currently stores in the GpuPeakMemoryMonitor.
  // Does not shut down tracking for |sequence_num|.
  uint64_t GetMonitorsPeakMemoryUsage(uint32_t sequence_num) {
    // Set default as max so that invalid cases can properly test 0u returns.
    uint64_t peak_memory = kUInt64_T_Max;
    auto allocation =
        channel_manager()->peak_memory_monitor_.GetPeakMemoryUsage(
            sequence_num, &peak_memory);
    return peak_memory;
  }

  // Helpers to call MemoryTracker::Observer methods of
  // GpuChannelManager::GpuPeakMemoryMonitor.
  void OnMemoryAllocatedChange(CommandBufferId id,
                               uint64_t old_size,
                               uint64_t new_size) {
    static_cast<MemoryTracker::Observer*>(gpu_peak_memory_monitor())
        ->OnMemoryAllocatedChange(id, old_size, new_size,
                                  GpuPeakMemoryAllocationSource::UNKNOWN);
  }

#if defined(OS_ANDROID)
  void TestApplicationBackgrounded(ContextType type,
                                   bool should_destroy_channel) {
    ASSERT_TRUE(channel_manager());

    int32_t kClientId = 1;
    GpuChannel* channel = CreateChannel(kClientId, true);
    EXPECT_TRUE(channel);

    int32_t kRouteId =
        static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
    const SurfaceHandle kFakeSurfaceHandle = 1;
    SurfaceHandle surface_handle = kFakeSurfaceHandle;
    GPUCreateCommandBufferConfig init_params;
    init_params.surface_handle = surface_handle;
    init_params.share_group_id = MSG_ROUTING_NONE;
    init_params.stream_id = 0;
    init_params.stream_priority = SchedulingPriority::kNormal;
    init_params.attribs = ContextCreationAttribs();
    init_params.attribs.context_type = type;
    init_params.active_url = GURL();
    gpu::ContextResult result = gpu::ContextResult::kFatalFailure;
    gpu::Capabilities capabilities;
    HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                               init_params, kRouteId, GetSharedMemoryRegion(),
                               &result, &capabilities));
    EXPECT_EQ(result, gpu::ContextResult::kSuccess);

    auto raster_decoder_state =
        channel_manager()->GetSharedContextState(&result);
    EXPECT_EQ(result, ContextResult::kSuccess);
    ASSERT_TRUE(raster_decoder_state);

    CommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
    EXPECT_TRUE(stub);

    channel_manager()->OnBackgroundCleanup();

    channel = channel_manager()->LookupChannel(kClientId);
    if (should_destroy_channel) {
      EXPECT_FALSE(channel);
    } else {
      EXPECT_TRUE(channel);
    }

    // We should always clear the shared raster state on background cleanup.
    ASSERT_NE(channel_manager()->GetSharedContextState(&result).get(),
              raster_decoder_state.get());
  }
#endif
};

TEST_F(GpuChannelManagerTest, EstablishChannel) {
  int32_t kClientId = 1;
  uint64_t kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());
  GpuChannel* channel = channel_manager()->EstablishChannel(
      kClientId, kClientTracingId, false, true);
  EXPECT_TRUE(channel);
  EXPECT_EQ(channel_manager()->LookupChannel(kClientId), channel);
}

#if defined(OS_ANDROID)
TEST_F(GpuChannelManagerTest, OnBackgroundedWithoutWebGL) {
  TestApplicationBackgrounded(CONTEXT_TYPE_OPENGLES2, true);
}

TEST_F(GpuChannelManagerTest, OnBackgroundedWithWebGL) {
  TestApplicationBackgrounded(CONTEXT_TYPE_WEBGL2, false);
}

#endif

// Tests that peak memory usage is only reported for valid sequence numbers,
// and that polling shuts down the monitoring.
TEST_F(GpuChannelManagerTest, GpuPeakMemoryOnlyReportedForValidSequence) {
  // Setup filtering to observe traces emitted.
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();
  trace_log->SetFilterFactoryForTesting(TestTraceEventFilter::Factory);
  const char config_json[] = R"(
      {
        "event_filters": [
           {
             "filter_predicate": "gpu",
             "included_categories": ["*"]
           }
        ]
      } )";
  trace_log->SetEnabled(base::trace_event::TraceConfig(config_json),
                        base::trace_event::TraceLog::FILTERING_MODE);

  GpuChannelManager* manager = channel_manager();
  const CommandBufferId buffer_id =
      CommandBufferIdFromChannelAndRoute(42, 1337);
  const uint64_t current_memory = 42;
  OnMemoryAllocatedChange(buffer_id, 0u, current_memory);

  const uint32_t sequence_num = 1;
  manager->StartPeakMemoryMonitor(sequence_num);
  EXPECT_EQ(current_memory, GetMonitorsPeakMemoryUsage(sequence_num));

  // A trace should have been emitted.
  EXPECT_NE(nullptr, g_trace_event);
  EXPECT_STREQ("PeakMemoryTracking", g_trace_event->name());
  EXPECT_STREQ("start", g_trace_event->arg_name(0));
  EXPECT_EQ(current_memory, g_trace_event->arg_value(0).as_uint);
  EXPECT_STREQ("start_sources", g_trace_event->arg_name(1));
  EXPECT_NE(nullptr, g_trace_event->arg_value(1).as_pointer);
  g_trace_event.reset();

  // With no request to listen to memory it should report 0.
  const uint32_t invalid_sequence_num = 1337;
  EXPECT_EQ(0u, GetMonitorsPeakMemoryUsage(invalid_sequence_num));
  EXPECT_EQ(0u, GetManagersPeakMemoryUsage(invalid_sequence_num));
  // There should be no trace emitted for invalid sequence.
  EXPECT_EQ(nullptr, g_trace_event);

  // The valid sequence should receive a report.
  EXPECT_EQ(current_memory, GetManagersPeakMemoryUsage(sequence_num));
  // However it should be shut-down and no longer report anything.
  EXPECT_EQ(0u, GetMonitorsPeakMemoryUsage(sequence_num));
  EXPECT_EQ(0u, GetManagersPeakMemoryUsage(sequence_num));

  // A trace should have been emitted as well.
  EXPECT_NE(nullptr, g_trace_event);
  EXPECT_STREQ("PeakMemoryTracking", g_trace_event->name());
  EXPECT_STREQ("peak", g_trace_event->arg_name(0));
  EXPECT_EQ(current_memory, g_trace_event->arg_value(0).as_uint);
  EXPECT_STREQ("end_sources", g_trace_event->arg_name(1));
  EXPECT_NE(nullptr, g_trace_event->arg_value(1).as_pointer);
  g_trace_event.reset();

  // Tracing's globals are not reset between tests. Clear out our filter and
  // disable tracing.
  trace_log->SetFilterFactoryForTesting(nullptr);
  trace_log->SetDisabled(base::trace_event::TraceLog::FILTERING_MODE);
}

// Tests that while a channel may exist for longer than a request to monitor,
// that only peaks seen are reported.
TEST_F(GpuChannelManagerTest,
       GpuPeakMemoryOnlyReportsPeaksFromObservationTime) {
  GpuChannelManager* manager = channel_manager();

  const CommandBufferId buffer_id =
      CommandBufferIdFromChannelAndRoute(42, 1337);
  const uint64_t initial_memory = 42;
  OnMemoryAllocatedChange(buffer_id, 0u, initial_memory);
  const uint64_t reduced_memory = 2;
  OnMemoryAllocatedChange(buffer_id, initial_memory, reduced_memory);

  const uint32_t sequence_num = 1;
  manager->StartPeakMemoryMonitor(sequence_num);
  EXPECT_EQ(reduced_memory, GetMonitorsPeakMemoryUsage(sequence_num));

  // While not the peak memory for the lifetime of |buffer_id| this should be
  // the peak seen during the observation of |sequence_num|.
  const uint64_t localized_peak_memory = 24;
  OnMemoryAllocatedChange(buffer_id, reduced_memory, localized_peak_memory);
  EXPECT_EQ(localized_peak_memory, GetManagersPeakMemoryUsage(sequence_num));
}

// Checks that when there are more than one sequence, that each has a separately
// calulcated peak.
TEST_F(GpuChannelManagerTest, GetPeakMemoryUsageCalculatedPerSequence) {
  GpuChannelManager* manager = channel_manager();

  const CommandBufferId buffer_id =
      CommandBufferIdFromChannelAndRoute(42, 1337);
  const uint64_t initial_memory = 42;
  OnMemoryAllocatedChange(buffer_id, 0u, initial_memory);

  // Start the first sequence so it is the only one to see the peak of
  // |initial_memory|.
  const uint32_t sequence_num_1 = 1;
  manager->StartPeakMemoryMonitor(sequence_num_1);

  // Reduce the memory before the second sequence starts.
  const uint64_t reduced_memory = 2;
  OnMemoryAllocatedChange(buffer_id, initial_memory, reduced_memory);

  const uint32_t sequence_num_2 = 2;
  manager->StartPeakMemoryMonitor(sequence_num_2);
  const uint64_t localized_peak_memory = 24;
  OnMemoryAllocatedChange(buffer_id, reduced_memory, localized_peak_memory);

  EXPECT_EQ(initial_memory, GetManagersPeakMemoryUsage(sequence_num_1));
  EXPECT_EQ(localized_peak_memory, GetManagersPeakMemoryUsage(sequence_num_2));
}

}  // namespace gpu
