// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/functional/bind.h"
#include "gpu/vulkan/tests/basic_vulkan_test.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"

namespace gpu {

using VulkanFenceHelperTest = BasicVulkanTest;

TEST_F(VulkanFenceHelperTest, BasicFenceUsage) {
  VulkanFenceHelper* fence_helper = GetDeviceQueue()->GetFenceHelper();
  VkFence fence = VK_NULL_HANDLE;
  ASSERT_TRUE(VK_SUCCESS == fence_helper->GetFence(&fence));
  ASSERT_TRUE(fence != VK_NULL_HANDLE);
  ASSERT_TRUE(VK_SUCCESS == vkQueueSubmit(GetDeviceQueue()->GetVulkanQueue(), 0,
                                          nullptr, fence));
  VulkanFenceHelper::FenceHandle fence_handle =
      fence_helper->EnqueueFence(fence);
  EXPECT_TRUE(fence_helper->Wait(fence_handle, UINT64_MAX));
  EXPECT_TRUE(fence_helper->HasPassed(fence_handle));
}

TEST_F(VulkanFenceHelperTest, TestBasicCallback) {
  VulkanFenceHelper* fence_helper = GetDeviceQueue()->GetFenceHelper();
  bool cleanup_run = false;
  fence_helper->EnqueueCleanupTaskForSubmittedWork(
      base::BindOnce([](bool* cleanup_run, VulkanDeviceQueue* device_queue,
                        bool is_lost) { *cleanup_run = true; },
                     &cleanup_run));
  VulkanFenceHelper::FenceHandle fence_handle =
      fence_helper->GenerateCleanupFence();
  EXPECT_TRUE(fence_handle.is_valid());
  fence_helper->Wait(fence_handle, UINT64_MAX);
  EXPECT_TRUE(cleanup_run);
}

TEST_F(VulkanFenceHelperTest, TestBasicCallbackExternalSubmission) {
  VulkanFenceHelper* fence_helper = GetDeviceQueue()->GetFenceHelper();
  bool cleanup_run = false;
  fence_helper->EnqueueCleanupTaskForSubmittedWork(
      base::BindOnce([](bool* cleanup_run, VulkanDeviceQueue* device_queue,
                        bool is_lost) { *cleanup_run = true; },
                     &cleanup_run));
  VkFence fence = VK_NULL_HANDLE;
  ASSERT_TRUE(VK_SUCCESS == fence_helper->GetFence(&fence));
  ASSERT_TRUE(fence != VK_NULL_HANDLE);
  ASSERT_TRUE(VK_SUCCESS == vkQueueSubmit(GetDeviceQueue()->GetVulkanQueue(), 0,
                                          nullptr, fence));
  VulkanFenceHelper::FenceHandle fence_handle =
      fence_helper->EnqueueFence(fence);
  EXPECT_TRUE(fence_handle.is_valid());
  fence_helper->Wait(fence_handle, UINT64_MAX);
  EXPECT_TRUE(cleanup_run);
}

TEST_F(VulkanFenceHelperTest, TestMultipleCallbacks) {
  VulkanFenceHelper* fence_helper = GetDeviceQueue()->GetFenceHelper();
  uint32_t cleanups_run = 0;
  auto increment_cleanups_callback =
      [](uint32_t expected_index, uint32_t* cleanups_run,
         VulkanDeviceQueue* device_queue, bool is_lost) {
        EXPECT_EQ(expected_index, *cleanups_run);
        *cleanups_run = *cleanups_run + 1;
      };

  // Enqueue 5 callbacks.
  for (int i = 0; i < 5; i++) {
    fence_helper->EnqueueCleanupTaskForSubmittedWork(
        base::BindOnce(increment_cleanups_callback, i, &cleanups_run));
  }

  // Generate a cleanup fence for the first 5 callbacks.
  VulkanFenceHelper::FenceHandle fence_handle =
      fence_helper->GenerateCleanupFence();

  // Enqueue 5 more callbacks.
  for (int i = 5; i < 10; i++) {
    fence_helper->EnqueueCleanupTaskForSubmittedWork(
        base::BindOnce(increment_cleanups_callback, i, &cleanups_run));
  }

  // Generate a cleanup fence for the next 5 callbacks.
  fence_handle = fence_helper->GenerateCleanupFence();
  EXPECT_TRUE(fence_handle.is_valid());
  fence_helper->Wait(fence_handle, UINT64_MAX);
  EXPECT_EQ(10u, cleanups_run);
}

TEST_F(VulkanFenceHelperTest, TestSkiaCallback) {
  VulkanFenceHelper* fence_helper = GetDeviceQueue()->GetFenceHelper();
  bool cleanup_run = false;
  fence_helper->EnqueueCleanupTaskForSubmittedWork(
      base::BindOnce([](bool* cleanup_run, VulkanDeviceQueue* device_queue,
                        bool is_lost) { *cleanup_run = true; },
                     &cleanup_run));
  auto cleanup_closure = fence_helper->CreateExternalCallback();
  EXPECT_FALSE(cleanup_run);
  std::move(cleanup_closure).Run();
  EXPECT_TRUE(cleanup_run);
}

TEST_F(VulkanFenceHelperTest, SkiaCallbackBeforeFences) {
  VulkanFenceHelper* fence_helper = GetDeviceQueue()->GetFenceHelper();
  uint32_t cleanups_run = 0;
  auto increment_cleanups_callback =
      [](uint32_t expected_index, uint32_t* cleanups_run,
         VulkanDeviceQueue* device_queue, bool is_lost) {
        EXPECT_EQ(expected_index, *cleanups_run);
        *cleanups_run = *cleanups_run + 1;
      };

  // Enqueue 5 callbacks.
  for (int i = 0; i < 5; i++) {
    fence_helper->EnqueueCleanupTaskForSubmittedWork(
        base::BindOnce(increment_cleanups_callback, i, &cleanups_run));
  }

  // The first 5 callbacks use a callback to trigger.
  auto cleanup_closure = fence_helper->CreateExternalCallback();

  // Enqueue 5 more callbacks.
  for (int i = 5; i < 10; i++) {
    fence_helper->EnqueueCleanupTaskForSubmittedWork(
        base::BindOnce(increment_cleanups_callback, i, &cleanups_run));
  }

  // Generate a cleanup fence for the next 5 callbacks.
  VulkanFenceHelper::FenceHandle fence_handle =
      fence_helper->GenerateCleanupFence();
  EXPECT_TRUE(fence_handle.is_valid());

  // After waiting for the second fence, all callbacks should have run, Skia
  // callbacks can be delayed, so we check future fences as well.
  EXPECT_TRUE(fence_helper->Wait(fence_handle, UINT64_MAX));
  EXPECT_EQ(10u, cleanups_run);

  // Running the callback now should be a no-op.
  std::move(cleanup_closure).Run();
  EXPECT_EQ(10u, cleanups_run);
}

TEST_F(VulkanFenceHelperTest, SkiaCallbackAfterFences) {
  VulkanFenceHelper* fence_helper = GetDeviceQueue()->GetFenceHelper();
  uint32_t cleanups_run = 0;
  auto increment_cleanups_callback =
      [](uint32_t expected_index, uint32_t* cleanups_run,
         VulkanDeviceQueue* device_queue, bool is_lost) {
        EXPECT_EQ(expected_index, *cleanups_run);
        *cleanups_run = *cleanups_run + 1;
      };

  // Enqueue 5 callbacks.
  for (int i = 0; i < 5; i++) {
    fence_helper->EnqueueCleanupTaskForSubmittedWork(
        base::BindOnce(increment_cleanups_callback, i, &cleanups_run));
  }

  // The first 5 callbacks use a fence to trigger.
  VulkanFenceHelper::FenceHandle fence_handle =
      fence_helper->GenerateCleanupFence();
  EXPECT_TRUE(fence_handle.is_valid());

  // Call vkQueueWaitIdle() to make sure the |fence_handle| is passed.
  vkQueueWaitIdle(queue());

  // Enqueue 5 more callbacks.
  for (int i = 5; i < 10; i++) {
    fence_helper->EnqueueCleanupTaskForSubmittedWork(
        base::BindOnce(increment_cleanups_callback, i, &cleanups_run));
  }

  // The next 5 callbacks use a callback to trigger.
  auto cleanup_closure = fence_helper->CreateExternalCallback();

  // Call the cleanup closure, all callbacks should run.
  // Generate a cleanup fence for the next 5 callbacks.
  std::move(cleanup_closure).Run();
  EXPECT_EQ(10u, cleanups_run);
}

}  // namespace gpu
