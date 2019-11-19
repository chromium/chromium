// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"

namespace gpu {

class GpuChannelManagerTest : public GpuChannelTestCommon {
 public:
  GpuChannelManagerTest()
      : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~GpuChannelManagerTest() override = default;

  GpuChannelManager::GpuPeakMemoryMonitor* gpu_peak_memory_monitor() {
    return &channel_manager()->peak_memory_monitor_;
  }

  uint64_t GetMonitorsPeakMemoryUsage(uint32_t sequence_num) {
    return channel_manager()->peak_memory_monitor_.GetPeakMemoryUsage(
        sequence_num);
  }

  // Helpers to call MemoryTracker::Observer methods of
  // GpuChannelManager::GpuPeakMemoryMonitor.
  void OnMemoryAllocatedChange(CommandBufferId id,
                               uint64_t old_size,
                               uint64_t new_size) {
    static_cast<MemoryTracker::Observer*>(gpu_peak_memory_monitor())
        ->OnMemoryAllocatedChange(id, old_size, new_size);
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
  GpuChannelManager* manager = channel_manager();
  const CommandBufferId buffer_id =
      CommandBufferIdFromChannelAndRoute(42, 1337);
  const uint64_t current_memory = 42;
  OnMemoryAllocatedChange(buffer_id, 0u, current_memory);

  const uint32_t sequence_num = 1;
  manager->StartPeakMemoryMonitor(sequence_num);
  EXPECT_EQ(current_memory, GetMonitorsPeakMemoryUsage(sequence_num));

  // With no request to listen to memory it should report 0.
  const uint32_t invalid_sequence_num = 1337;
  EXPECT_EQ(0u, GetMonitorsPeakMemoryUsage(invalid_sequence_num));
  EXPECT_EQ(0u, manager->GetPeakMemoryUsage(invalid_sequence_num));

  // The valid sequence should receive a report.
  EXPECT_EQ(current_memory, manager->GetPeakMemoryUsage(sequence_num));
  // However it should be shut-down and no longer report anything.
  EXPECT_EQ(0u, GetMonitorsPeakMemoryUsage(sequence_num));
  EXPECT_EQ(0u, manager->GetPeakMemoryUsage(sequence_num));
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
  EXPECT_EQ(localized_peak_memory, manager->GetPeakMemoryUsage(sequence_num));
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

  EXPECT_EQ(initial_memory, manager->GetPeakMemoryUsage(sequence_num_1));
  EXPECT_EQ(localized_peak_memory, manager->GetPeakMemoryUsage(sequence_num_2));
}

}  // namespace gpu
