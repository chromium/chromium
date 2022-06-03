// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_TESTS_BASIC_VULKAN_TEST_H_
#define GPU_VULKAN_TESTS_BASIC_VULKAN_TEST_H_

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {

class BasicVulkanTest : public testing::Test {
 public:
  BasicVulkanTest();
  ~BasicVulkanTest() override;

  void SetUp() override;
  void TearDown() override;

  gfx::AcceleratedWidget window() const { return window_; }
  VulkanImplementation* GetVulkanImplementation() {
    return vulkan_implementation_.get();
  }
  VulkanDeviceQueue* GetDeviceQueue() { return device_queue_.get(); }
  VkDevice device() { return device_queue_->GetVulkanDevice(); }
  VkQueue queue() { return device_queue_->GetVulkanQueue(); }
  bool supports_swapchain() const {
    return window_ != gfx::kNullAcceleratedWidget;
  }
  std::unique_ptr<VulkanSurface> CreateViewSurface(
      gfx::AcceleratedWidget window);

 private:
  std::unique_ptr<VulkanImplementation> vulkan_implementation_;
  std::unique_ptr<VulkanDeviceQueue> device_queue_;
  gfx::AcceleratedWidget window_ = gfx::kNullAcceleratedWidget;
};

}  // namespace gpu

#endif  // GPU_VULKAN_TESTS_BASIC_VULKAN_TEST_H_
