// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/eventfd.h>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/android/vulkan_implementation_android.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class VulkanImplementationAndroidTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a vulkan implementation.
    vk_implementation_ = std::make_unique<VulkanImplementationAndroid>();
    ASSERT_TRUE(vk_implementation_);

    // This call checks for all instance extensions. Let the test pass if this
    // call fails since many bots would not have this extension present.
    if (!vk_implementation_->InitializeVulkanInstance(true /* using_surface */))
      return;

    // Create vulkan context provider. This call checks for all device
    // extensions. Let the test pass if this call fails since many bots would
    // not have this extension present.
    vk_context_provider_ =
        viz::VulkanInProcessContextProvider::Create(vk_implementation_.get());
    if (!vk_context_provider_)
      return;

    // Get the VkDevice.
    vk_device_ = vk_context_provider_->GetDeviceQueue()->GetVulkanDevice();
    ASSERT_TRUE(vk_device_);

    // Get the physical device.
    vk_phy_device_ =
        vk_context_provider_->GetDeviceQueue()->GetVulkanPhysicalDevice();
    ASSERT_TRUE(vk_phy_device_);
  }

  void TearDown() override {
    if (vk_context_provider_)
      vk_context_provider_->Destroy();
    vk_device_ = VK_NULL_HANDLE;
  }

 protected:
  std::unique_ptr<VulkanImplementation> vk_implementation_;
  scoped_refptr<viz::VulkanInProcessContextProvider> vk_context_provider_;
  VkDevice vk_device_;
  VkPhysicalDevice vk_phy_device_;
};

TEST_F(VulkanImplementationAndroidTest, ExportImportSyncFd) {
  if (!vk_implementation_ || !vk_context_provider_)
    return;

  // Create a vk semaphore which can be exported.
  // To create a semaphore whose payload can be exported to external handles,
  // add the VkExportSemaphoreCreateInfo structure to the pNext chain of the
  // VkSemaphoreCreateInfo structure.
  VkExportSemaphoreCreateInfo export_info;
  export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  export_info.pNext = nullptr;
  export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

  VkSemaphore semaphore1;
  VkSemaphoreCreateInfo sem_info;
  sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sem_info.pNext = &export_info;
  sem_info.flags = 0;
  bool result = vkCreateSemaphore(vk_device_, &sem_info, nullptr, &semaphore1);
  EXPECT_EQ(result, VK_SUCCESS);

  // SYNC_FD semaphores must be signalled or have an associated semaphore
  // signal operation pending execution before the export.
  // Semaphores can be signaled by including them in a batch as part of a queue
  // submission command, defining a queue operation to signal that semaphore.
  EXPECT_TRUE(SubmitSignalVkSemaphore(
      vk_context_provider_->GetDeviceQueue()->GetVulkanQueue(), semaphore1));

  // Export a handle from the semaphore.
  SemaphoreHandle handle =
      vk_implementation_->GetSemaphoreHandle(vk_device_, semaphore1);
  EXPECT_TRUE(handle.is_valid());

  // Import the above semaphore handle into a new semaphore.
  VkSemaphore semaphore2 =
      vk_implementation_->ImportSemaphoreHandle(vk_device_, std::move(handle));
  EXPECT_NE(semaphore2, static_cast<VkSemaphore>(VK_NULL_HANDLE));

  // Wait for the device to be idle.
  result = vkDeviceWaitIdle(vk_device_);
  EXPECT_EQ(result, VK_SUCCESS);

  // Destroy the semaphores.
  vkDestroySemaphore(vk_device_, semaphore1, nullptr);
  vkDestroySemaphore(vk_device_, semaphore2, nullptr);
}

TEST_F(VulkanImplementationAndroidTest, CreateVkImageFromAHB) {
  if (!vk_implementation_ || !vk_context_provider_)
    return;

  // Setup and Create an AHardwareBuffer.
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc hwb_desc;
  hwb_desc.width = 128;
  hwb_desc.height = 128;
  hwb_desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  hwb_desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                   AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
  hwb_desc.layers = 1;
  hwb_desc.stride = 0;
  hwb_desc.rfu0 = 0;
  hwb_desc.rfu1 = 0;

  // Allocate an AHardwareBuffer.
  base::AndroidHardwareBufferCompat::GetInstance().Allocate(&hwb_desc, &buffer);
  EXPECT_TRUE(buffer);

  // Create a vkimage and import the AHB into it.
  const gfx::Size size(hwb_desc.width, hwb_desc.height);
  VkImage vk_image;
  VkImageCreateInfo vk_image_info;
  VkDeviceMemory vk_device_memory;
  VkDeviceSize mem_allocation_size;
  EXPECT_TRUE(vk_implementation_->CreateVkImageAndImportAHB(
      vk_device_, vk_phy_device_, size,
      base::android::ScopedHardwareBufferHandle::Adopt(buffer), &vk_image,
      &vk_image_info, &vk_device_memory, &mem_allocation_size));

  // Free up resources.
  vkDestroyImage(vk_device_, vk_image, nullptr);
  vkFreeMemory(vk_device_, vk_device_memory, nullptr);
}

}  // namespace gpu
