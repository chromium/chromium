// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "gpu/vulkan/tests/basic_vulkan_test.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_swap_chain.h"

// This file tests basic vulkan initialization steps.

namespace gpu {

TEST_F(BasicVulkanTest, BasicVulkanSurface) {
  std::unique_ptr<VulkanSurface> surface = CreateViewSurface(window());
  EXPECT_TRUE(surface);
  EXPECT_TRUE(surface->Initialize(GetDeviceQueue(),
                                  VulkanSurface::DEFAULT_SURFACE_FORMAT));
  EXPECT_TRUE(
      surface->Reshape(gfx::Size(100, 100), gfx::OVERLAY_TRANSFORM_NONE));
  surface->Destroy();
}

TEST_F(BasicVulkanTest, EmptyVulkanSwaps) {
  std::unique_ptr<VulkanSurface> surface = CreateViewSurface(window());
  ASSERT_TRUE(surface);
  ASSERT_TRUE(surface->Initialize(GetDeviceQueue(),
                                  VulkanSurface::DEFAULT_SURFACE_FORMAT));
  ASSERT_TRUE(
      surface->Reshape(gfx::Size(100, 100), gfx::OVERLAY_TRANSFORM_NONE));

  // First swap is a special case, call it first to get better errors.
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers());

  // Also make sure we can swap multiple times.
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers());
  }
  surface->Finish();
  surface->Destroy();
}

}  // namespace gpu
