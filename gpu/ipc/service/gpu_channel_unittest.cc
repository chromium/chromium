// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel.h"

#include <stdint.h>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "ipc/constants.mojom.h"

namespace gpu {

class GpuChannelTest : public GpuChannelTestCommon {
 public:
  GpuChannelTest() : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~GpuChannelTest() override = default;
};

TEST_F(GpuChannelTest, CreateOffscreenCommandBuffer) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true);
  ASSERT_TRUE(channel);

  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs =
      mojom::ContextCreationAttribs::NewGles(mojom::GLESCreationAttribs::New());
  init_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kSuccess);

  CommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_TRUE(stub);
}

TEST_F(GpuChannelTest, CreateSharedImageWithMipmapRejected) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);
  ASSERT_TRUE(channel->shared_image_stub());

  auto params = mojom::CreateSharedImageParams::New();
  params->mailbox = Mailbox::Generate();
  params->si_info = mojom::SharedImageInfo::New();
  params->si_info->meta.format = viz::SinglePlaneFormat::kRGBA_8888;
  params->si_info->meta.size = gfx::Size(64, 64);
  params->si_info->meta.color_space = gfx::ColorSpace::CreateSRGB();
  params->si_info->meta.surface_origin = kTopLeft_GrSurfaceOrigin;
  params->si_info->meta.alpha_type = kPremul_SkAlphaType;
  params->si_info->meta.usage = SHARED_IMAGE_USAGE_RASTER_READ |
                                SHARED_IMAGE_USAGE_RASTER_WRITE |
                                SHARED_IMAGE_USAGE_MIPMAP;

  channel->shared_image_stub()->ExecuteDeferredRequest(
      mojom::DeferredSharedImageRequest::NewCreateSharedImage(
          std::move(params)));

  // Rejecting SHARED_IMAGE_USAGE_MIPMAP from a client triggers OnError(),
  // which stops and removes the channel.
  EXPECT_FALSE(channel_manager()->LookupChannel(kClientId));
}

TEST_F(GpuChannelTest, CreateLowPriorityCommandBuffer) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true);
  ASSERT_TRUE(channel);

  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kLow;
  init_params->attribs =
      mojom::ContextCreationAttribs::NewGles(mojom::GLESCreationAttribs::New());
  init_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kSuccess);

  CommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  ASSERT_TRUE(stub);
  EXPECT_EQ(
      channel->scheduler()->GetSequenceDefaultPriority(stub->sequence_id()),
      SchedulingPriority::kLow);
  EXPECT_EQ(channel->scheduler()->GetSequencePriority(stub->sequence_id()),
            SchedulingPriority::kLow);
}

class CapCommandBufferWaitPriorityFeature {
 protected:
  base::test::ScopedFeatureList feature_list_{
      features::kCapCommandBufferWaitPriority};
};

class GpuChannelWaitPriorityTest : public CapCommandBufferWaitPriorityFeature,
                                   public GpuChannelTestCommon {
 public:
  GpuChannelWaitPriorityTest()
      : GpuChannelTestCommon(
            /*use_stub_bindings=*/true,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(GpuChannelWaitPriorityTest,
       WaitForGetOffsetDoesNotPreemptHighPrioritySequence) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true);
  ASSERT_TRUE(channel);

  int32_t kNormalRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto normal_params = mojom::CreateCommandBufferParams::New();
  normal_params->stream_id = 0;
  normal_params->stream_priority = SchedulingPriority::kNormal;
  normal_params->attribs =
      mojom::ContextCreationAttribs::NewGles(mojom::GLESCreationAttribs::New());
  normal_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(normal_params), kNormalRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kSuccess);

  CommandBufferStub* normal_stub = channel->LookupCommandBuffer(kNormalRouteId);
  ASSERT_TRUE(normal_stub);

  // Client-side wait on the normal command buffer should cap its priority at
  // kNormal instead of boosting to kHigh.
  normal_stub->WaitForGetOffsetInRange(0, 10, 20, base::DoNothing());
  EXPECT_EQ(
      channel->scheduler()->GetSequencePriority(normal_stub->sequence_id()),
      SchedulingPriority::kNormal);

  // Create an independent kHigh sequence (representing Viz
  // Display::DrawAndSwap).
  SequenceId high_sequence_id = scheduler()->CreateSequence(
      SchedulingPriority::kHigh, task_environment().GetMainThreadTaskRunner());

  int count = 0;
  int ran_normal = 0;
  int ran_high = 0;

  // Schedule task on the waiting command buffer first (older order_num).
  scheduler()->ScheduleTask(Scheduler::Task(
      normal_stub->sequence_id(),
      base::BindOnce(
          [](int* count_ptr, int* ran_ptr) { *ran_ptr = ++(*count_ptr); },
          &count, &ran_normal),
      std::vector<SyncToken>()));

  // Schedule task on the kHigh sequence second (newer order_num).
  scheduler()->ScheduleTask(Scheduler::Task(
      high_sequence_id,
      base::BindOnce(
          [](int* count_ptr, int* ran_ptr) { *ran_ptr = ++(*count_ptr); },
          &count, &ran_high),
      std::vector<SyncToken>()));

  base::RunLoop().RunUntilIdle();

  // The kHigh sequence task must run first, not preempted by the waiting stub.
  EXPECT_EQ(ran_high, 1);
  EXPECT_EQ(ran_normal, 2);

  scheduler()->DestroySequence(high_sequence_id);
}

TEST_F(GpuChannelWaitPriorityTest,
       LowPriorityWaitBoostsToNormalThenEscalatesToHigh) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true);
  ASSERT_TRUE(channel);

  int32_t kLowRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto low_params = mojom::CreateCommandBufferParams::New();
  low_params->stream_id = 1;
  low_params->stream_priority = SchedulingPriority::kLow;
  low_params->attribs =
      mojom::ContextCreationAttribs::NewGles(mojom::GLESCreationAttribs::New());
  low_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(low_params), kLowRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kSuccess);

  CommandBufferStub* low_stub = channel->LookupCommandBuffer(kLowRouteId);
  ASSERT_TRUE(low_stub);
  EXPECT_EQ(channel->scheduler()->GetSequencePriority(low_stub->sequence_id()),
            SchedulingPriority::kLow);

  // A wait that finishes immediately stops the escalation timer and restores
  // default kLow priority.
  low_stub->WaitForGetOffsetInRange(0, 0, 0, base::DoNothing());
  EXPECT_EQ(channel->scheduler()->GetSequencePriority(low_stub->sequence_id()),
            SchedulingPriority::kLow);
  task_environment().FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(channel->scheduler()->GetSequencePriority(low_stub->sequence_id()),
            SchedulingPriority::kLow);

  // A subsequent unresolved wait re-arms the retained timer, boosts kLow to
  // kNormal immediately, and escalates to kHigh after 100ms to prevent client
  // thread HangWatcher starvation.
  low_stub->WaitForGetOffsetInRange(0, 10, 20, base::DoNothing());
  EXPECT_EQ(channel->scheduler()->GetSequencePriority(low_stub->sequence_id()),
            SchedulingPriority::kNormal);

  task_environment().FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(channel->scheduler()->GetSequencePriority(low_stub->sequence_id()),
            SchedulingPriority::kHigh);
}

class GpuChannelExitForContextLostTest : public GpuChannelTestCommon {
 public:
  GpuChannelExitForContextLostTest()
      : GpuChannelTestCommon({EXIT_ON_CONTEXT_LOST} /* enabled_workarounds */,
                             true /* use_stub_bindings */) {}
};

TEST_F(GpuChannelExitForContextLostTest,
       CreateFailsDuringLostContextShutdown_1) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  // Put channel manager into shutdown state.
  channel_manager()->OnContextLost(-1 /* context_lost_count */,
                                   false /* synthetic_loss */,
                                   error::ContextLostReason::kUnknown);

  // Calling OnContextLost() above may destroy the gpu channel via post task.
  // Ensure that post task has happened.
  base::RunLoop().RunUntilIdle();

  // If the channel is destroyed, then skip the test.
  if (!channel_manager()->LookupChannel(kClientId)) {
    return;
  }

  // Try to create a context.
  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs =
      mojom::ContextCreationAttribs::NewGles(mojom::GLESCreationAttribs::New());
  init_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kTransientFailure);
  EXPECT_FALSE(channel->LookupCommandBuffer(kRouteId));
}

TEST_F(GpuChannelExitForContextLostTest,
       CreateFailsDuringLostContextShutdown_2) {
  // Put channel manager into shutdown state. Do this before creating a channel,
  // as doing this may destroy any active channels.
  channel_manager()->OnContextLost(-1 /* context_lost_count */,
                                   false /* synthetic_loss */,
                                   error::ContextLostReason::kUnknown);

  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  // Try to create a context.
  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs =
      mojom::ContextCreationAttribs::NewGles(mojom::GLESCreationAttribs::New());
  init_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kTransientFailure);
  EXPECT_FALSE(channel->LookupCommandBuffer(kRouteId));
}

}  // namespace gpu
