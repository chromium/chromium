// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "gpu/vulkan/tests/basic_vulkan_test.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_swap_chain.h"
#include "gpu/vulkan/vulkan_util.h"

// This file tests basic vulkan initialization steps.

namespace gpu {

TEST_F(BasicVulkanTest, BasicVulkanSurface) {
  if (!supports_swapchain())
    return;
  std::unique_ptr<VulkanSurface> surface = CreateViewSurface(window());
  EXPECT_TRUE(surface);
  EXPECT_TRUE(surface->Initialize(GetDeviceQueue(),
                                  VulkanSurface::DEFAULT_SURFACE_FORMAT));
  EXPECT_TRUE(
      surface->Reshape(gfx::Size(100, 100), gfx::OVERLAY_TRANSFORM_NONE));
  surface->Destroy();
}

TEST_F(BasicVulkanTest, EmptyVulkanSwaps) {
  if (!supports_swapchain())
    return;

  auto command_pool = std::make_unique<VulkanCommandPool>(GetDeviceQueue());
  EXPECT_TRUE(command_pool->Initialize());

  std::unique_ptr<VulkanSurface> surface = CreateViewSurface(window());
  ASSERT_TRUE(surface);
  ASSERT_TRUE(surface->Initialize(GetDeviceQueue(),
                                  VulkanSurface::DEFAULT_SURFACE_FORMAT));
  ASSERT_TRUE(
      surface->Reshape(gfx::Size(100, 100), gfx::OVERLAY_TRANSFORM_NONE));

  constexpr VkSemaphore kNullSemaphore = VK_NULL_HANDLE;

  std::optional<VulkanSwapChain::ScopedWrite> scoped_write;
  scoped_write.emplace(surface->swap_chain());
  EXPECT_TRUE(scoped_write->success());

  VkSemaphore begin_semaphore = scoped_write->begin_semaphore();
  EXPECT_NE(begin_semaphore, kNullSemaphore);

  VkSemaphore end_semaphore = scoped_write->end_semaphore();
  EXPECT_NE(end_semaphore, kNullSemaphore);

  auto command_buffer = command_pool->CreatePrimaryCommandBuffer();

  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);
    command_buffer->TransitionImageLayout(scoped_write->image(),
                                          scoped_write->image_layout(),
                                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  }
  EXPECT_TRUE(command_buffer->Submit(1, &begin_semaphore, 1, &end_semaphore));
  scoped_write.reset();

  // First swap is a special case, call it first to get better errors.
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface->SwapBuffers(
                base::DoNothingAs<void(const gfx::PresentationFeedback&)>()));

  vkQueueWaitIdle(GetDeviceQueue()->GetVulkanQueue());
  command_buffer->Destroy();
  command_buffer.reset();

  // Also make sure we can swap multiple times.
  for (int i = 0; i < 10; ++i) {
    scoped_write.emplace(surface->swap_chain());
    EXPECT_TRUE(scoped_write->success());

    begin_semaphore = scoped_write->begin_semaphore();
    EXPECT_NE(begin_semaphore, kNullSemaphore);

    end_semaphore = scoped_write->end_semaphore();
    EXPECT_NE(end_semaphore, kNullSemaphore);

    command_buffer = command_pool->CreatePrimaryCommandBuffer();
    {
      ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);

      command_buffer->TransitionImageLayout(scoped_write->image(),
                                            scoped_write->image_layout(),
                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    EXPECT_TRUE(command_buffer->Submit(1, &begin_semaphore, 1, &end_semaphore));
    scoped_write.reset();

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface->SwapBuffers(
                  base::DoNothingAs<void(const gfx::PresentationFeedback&)>()));
    vkQueueWaitIdle(GetDeviceQueue()->GetVulkanQueue());
    command_buffer->Destroy();
    command_buffer.reset();
  }
  surface->Finish();
  surface->Destroy();

  command_pool->Destroy();
}

}  // namespace gpu
