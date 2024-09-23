// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/basic_vulkan_test.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/tests/native_window.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gpu {

BasicVulkanTest::BasicVulkanTest() {}

BasicVulkanTest::~BasicVulkanTest() {}

void BasicVulkanTest::SetUp() {
  bool supports_swapchain = true;
#if BUILDFLAG(IS_OZONE)
  supports_swapchain = ui::OzonePlatform::GetInstance()
                           ->GetPlatformProperties()
                           .supports_vulkan_swap_chain;
#endif

  bool use_swiftshader =
      base::CommandLine::ForCurrentProcess()->HasSwitch("use-swiftshader");
  const gfx::Rect kDefaultBounds(10, 10, 100, 100);
  if (supports_swapchain) {
    window_ = CreateNativeWindow(kDefaultBounds);
    ASSERT_TRUE(window_ != gfx::kNullAcceleratedWidget);
  }
  vulkan_implementation_ = CreateVulkanImplementation(use_swiftshader);
  ASSERT_TRUE(vulkan_implementation_);
  ASSERT_TRUE(vulkan_implementation_->InitializeVulkanInstance());
  int flags = VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG;
  if (supports_swapchain)
    flags |= VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG;
  device_queue_ =
      gpu::CreateVulkanDeviceQueue(vulkan_implementation_.get(), flags);
  ASSERT_TRUE(device_queue_);
}

void BasicVulkanTest::TearDown() {
  if (window_ != gfx::kNullAcceleratedWidget) {
    DestroyNativeWindow(window_);
    window_ = gfx::kNullAcceleratedWidget;
  }
  device_queue_->Destroy();
  device_queue_.reset();
  vulkan_implementation_.reset();
}

std::unique_ptr<VulkanSurface> BasicVulkanTest::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  return window_ != gfx::kNullAcceleratedWidget
             ? vulkan_implementation_->CreateViewSurface(window)
             : nullptr;
}

}  // namespace gpu
