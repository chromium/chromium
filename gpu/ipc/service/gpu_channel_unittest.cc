// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel.h"

#include <stdint.h>

#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"

namespace gpu {

class GpuChannelTest : public GpuChannelTestCommon {
 public:
  GpuChannelTest() : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~GpuChannelTest() override = default;
};

#if BUILDFLAG(IS_ANDROID)
const SurfaceHandle kFakeSurfaceHandle = 1;

TEST_F(GpuChannelTest, CreateViewCommandBufferAllowed) {
  // TODO(crbug.com/40062603): Currently it's not possible to create
  // onscreen GLSurface with Null binding with angle.
  if (channel_manager()->use_passthrough_cmd_decoder()) {
    GTEST_SKIP();
  }

  int32_t kClientId = 1;
  bool is_gpu_host = true;
  GpuChannel* channel = CreateChannel(kClientId, is_gpu_host);
  ASSERT_TRUE(channel);

  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->surface_handle = kFakeSurfaceHandle;
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs = ContextCreationAttribs();
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
}

TEST_F(GpuChannelTest, CreateViewCommandBufferDisallowed) {
  int32_t kClientId = 1;
  bool is_gpu_host = false;
  GpuChannel* channel = CreateChannel(kClientId, is_gpu_host);
  ASSERT_TRUE(channel);

  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->surface_handle = kFakeSurfaceHandle;
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs = ContextCreationAttribs();
  init_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kFatalFailure);

  CommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_FALSE(stub);
}
#endif

TEST_F(GpuChannelTest, CreateOffscreenCommandBuffer) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true);
  ASSERT_TRUE(channel);

  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs = ContextCreationAttribs();
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

TEST_F(GpuChannelTest, IncompatibleStreamIds) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId1 =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  int32_t kStreamId1 = 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = kStreamId1;
  init_params->stream_priority = SchedulingPriority::kNormal;
  auto init_params2 = init_params.Clone();

  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId1,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kSuccess);

  CommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId1);
  EXPECT_TRUE(stub);

  // Create second context in same share group but different stream.
  int32_t kRouteId2 = kRouteId1 + 1;
  int32_t kStreamId2 = 2;

  init_params2->share_group_id = kRouteId1;
  init_params2->stream_id = kStreamId2;
  init_params2->stream_priority = SchedulingPriority::kNormal;
  CreateCommandBuffer(*channel, std::move(init_params2), kRouteId2,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kFatalFailure);

  stub = channel->LookupCommandBuffer(kRouteId2);
  EXPECT_FALSE(stub);
}

TEST_F(GpuChannelTest, CreateFailsIfSharedContextIsLost) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  // Create first context, we will share this one.
  int32_t kSharedRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  {
    SCOPED_TRACE("kSharedRouteId");
    auto init_params = mojom::CreateCommandBufferParams::New();
    init_params->share_group_id = MSG_ROUTING_NONE;
    init_params->stream_id = 0;
    init_params->stream_priority = SchedulingPriority::kNormal;
    init_params->attribs = ContextCreationAttribs();
    init_params->active_url = GURL();
    gpu::ContextResult result = gpu::ContextResult::kSuccess;
    gpu::Capabilities capabilities;
    gpu::GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel, std::move(init_params), kSharedRouteId,
                        GetSharedMemoryRegion(), &result, &capabilities,
                        &gl_capabilities);
    EXPECT_EQ(result, gpu::ContextResult::kSuccess);
  }
  EXPECT_TRUE(channel->LookupCommandBuffer(kSharedRouteId));

  // This context shares with the first one, this should be possible.
  int32_t kFriendlyRouteId = kSharedRouteId + 1;
  {
    SCOPED_TRACE("kFriendlyRouteId");
    auto init_params = mojom::CreateCommandBufferParams::New();
    init_params->share_group_id = kSharedRouteId;
    init_params->stream_id = 0;
    init_params->stream_priority = SchedulingPriority::kNormal;
    init_params->attribs = ContextCreationAttribs();
    init_params->active_url = GURL();
    gpu::ContextResult result = gpu::ContextResult::kSuccess;
    gpu::Capabilities capabilities;
    gpu::GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel, std::move(init_params), kFriendlyRouteId,
                        GetSharedMemoryRegion(), &result, &capabilities,
                        &gl_capabilities);
    EXPECT_EQ(result, gpu::ContextResult::kSuccess);
  }
  EXPECT_TRUE(channel->LookupCommandBuffer(kFriendlyRouteId));

  // The shared context is lost.
  channel->LookupCommandBuffer(kSharedRouteId)->MarkContextLost();

  // Meanwhile another context is being made pointing to the shared one. This
  // should fail.
  int32_t kAnotherRouteId = kFriendlyRouteId + 1;
  {
    SCOPED_TRACE("kAnotherRouteId");
    auto init_params = mojom::CreateCommandBufferParams::New();
    init_params->share_group_id = kSharedRouteId;
    init_params->stream_id = 0;
    init_params->stream_priority = SchedulingPriority::kNormal;
    init_params->attribs = ContextCreationAttribs();
    init_params->active_url = GURL();
    gpu::ContextResult result = gpu::ContextResult::kSuccess;
    gpu::Capabilities capabilities;
    gpu::GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel, std::move(init_params), kAnotherRouteId,
                        GetSharedMemoryRegion(), &result, &capabilities,
                        &gl_capabilities);
    EXPECT_EQ(result, gpu::ContextResult::kTransientFailure);
  }
  EXPECT_FALSE(channel->LookupCommandBuffer(kAnotherRouteId));

  // The lost context is still around though (to verify the failure happened due
  // to the shared context being lost, not due to it being deleted).
  EXPECT_TRUE(channel->LookupCommandBuffer(kSharedRouteId));

  // Destroy the command buffers we initialized before destoying GL.
  channel->DestroyCommandBuffer(kFriendlyRouteId);
  channel->DestroyCommandBuffer(kSharedRouteId);
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
  if (!channel_manager()->LookupChannel(kClientId))
    return;

  // Try to create a context.
  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs = ContextCreationAttribs();
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
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs = ContextCreationAttribs();
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
