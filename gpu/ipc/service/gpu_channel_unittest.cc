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
