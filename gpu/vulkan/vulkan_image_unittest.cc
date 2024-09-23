// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/vulkan/tests/basic_vulkan_test.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#endif

namespace gpu {

namespace {

// TODO(penghuang): add more formats used by chrome.
const VkFormat kFormats[] = {
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_B8G8R8A8_UNORM,
};

}  // namespace

using VulkanImageTest = BasicVulkanTest;

TEST_F(VulkanImageTest, Create) {
  constexpr gfx::Size size(100, 100);
  constexpr VkImageUsageFlags usage =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  auto* device_queue = GetDeviceQueue();
  for (auto format : kFormats) {
    auto image = VulkanImage::Create(device_queue, size, format, usage);
    EXPECT_TRUE(image);
    EXPECT_EQ(image->size(), size);
    EXPECT_EQ(image->format(), format);
    EXPECT_GT(image->device_size(), 0u);
    EXPECT_EQ(image->image_tiling(), VK_IMAGE_TILING_OPTIMAL);
    EXPECT_NE(image->image(), static_cast<VkImage>(VK_NULL_HANDLE));
    EXPECT_NE(image->device_memory(),
              static_cast<VkDeviceMemory>(VK_NULL_HANDLE));
    EXPECT_EQ(image->handle_types(), 0u);
    image->Destroy();
  }
}

TEST_F(VulkanImageTest, CreateWithExternalMemory) {
  {
    // TODO(crbug.com/40125946) : Fails on current driver version on this bot.
    if (GPUTestBotConfig::CurrentConfigMatches("Win10"))
      return;
  }

  constexpr gfx::Size size(100, 100);
  constexpr VkImageUsageFlags usage =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  auto* device_queue = GetDeviceQueue();
  for (auto format : kFormats) {
    auto image = VulkanImage::CreateWithExternalMemory(device_queue, size,
                                                       format, usage);
    EXPECT_TRUE(image);
    EXPECT_EQ(image->size(), size);
    EXPECT_EQ(image->format(), format);
    EXPECT_GT(image->device_size(), 0u);
    EXPECT_EQ(image->image_tiling(), VK_IMAGE_TILING_OPTIMAL);
    EXPECT_NE(image->image(), static_cast<VkImage>(VK_NULL_HANDLE));
    EXPECT_NE(image->device_memory(),
              static_cast<VkDeviceMemory>(VK_NULL_HANDLE));

#if BUILDFLAG(IS_POSIX)
    EXPECT_TRUE(image->handle_types() &
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
        << std::hex << "handle_types = 0x" << image->handle_types();
    const VkExternalMemoryHandleTypeFlagBits kHandleTypes[] = {
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    // Get fd for all supported types.
    for (auto handle_type : kHandleTypes) {
      if ((image->handle_types() & handle_type) == 0)
        continue;
      base::ScopedFD scoped_fd = image->GetMemoryFd(handle_type);
      EXPECT_TRUE(scoped_fd.is_valid())
          << std::hex << " handle_types = 0x" << image->handle_types()
          << " handle_type = 0x" << handle_type;
    }
#elif BUILDFLAG(IS_WIN)
    EXPECT_TRUE(image->handle_types() &
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT)
        << std::hex << "handle_types = 0x" << image->handle_types();
    const VkExternalMemoryHandleTypeFlagBits kHandleTypes[] = {
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
    };
    // Get fd for all supported types.
    for (auto handle_type : kHandleTypes) {
      if ((image->handle_types() & handle_type) == 0)
        continue;
      base::win::ScopedHandle scoped_handle = image->GetMemoryHandle(
          static_cast<VkExternalMemoryHandleTypeFlagBits>(handle_type));
      EXPECT_TRUE(scoped_handle.IsValid())
          << std::hex << " handle_types = 0x" << image->handle_types()
          << " handle_type = 0x" << handle_type;
    }
#elif BUILDFLAG(IS_FUCHSIA)
    EXPECT_TRUE(image->handle_types() &
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA);
    zx::vmo handle = image->GetMemoryZirconHandle();
    EXPECT_TRUE(handle);
#endif

    image->Destroy();
  }
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(VulkanImageTest, CreateFromGpuMemoryBufferHandle) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable()) {
    LOG(ERROR) << "AndroidHardwareBuffer is not supported";
    return;
  }

  auto* device_queue = GetDeviceQueue();
  if (!gfx::HasExtension(
          device_queue->enabled_extensions(),
          VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME)) {
    LOG(ERROR) << "Vulkan extension "
                  "VK_ANDROID_external_memory_android_hardware_buffer is not "
                  "supported";
    return;
  }

  auto factory = GpuMemoryBufferFactory::CreateNativeType(
      /*viz::VulkanContextProvider=*/nullptr);
  EXPECT_TRUE(factory);
  constexpr gfx::Size size(100, 100);
  constexpr VkImageUsageFlags usage =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  const struct {
    gfx::BufferFormat buffer;
    VkFormat vk;
  } formats[] = {
      {gfx::BufferFormat::RGBA_8888, VK_FORMAT_R8G8B8A8_UNORM},
      {gfx::BufferFormat::BGR_565, VK_FORMAT_R5G6B5_UNORM_PACK16},
      {gfx::BufferFormat::RGBA_F16, VK_FORMAT_R16G16B16A16_SFLOAT},
      {gfx::BufferFormat::RGBX_8888, VK_FORMAT_R8G8B8A8_UNORM},
      {gfx::BufferFormat::RGBA_1010102, VK_FORMAT_A2B10G10R10_UNORM_PACK32},
  };
  for (const auto format : formats) {
    gfx::GpuMemoryBufferId id(1);
    gfx::BufferUsage buffer_usage = gfx::BufferUsage::SCANOUT;
    int client_id = 1;
    auto gmb_handle = factory->CreateGpuMemoryBuffer(
        id, size, /*framebuffer_size=*/size, format.buffer, buffer_usage,
        client_id, kNullSurfaceHandle);
    EXPECT_TRUE(!gmb_handle.is_null());
    EXPECT_EQ(gmb_handle.type,
              gfx::GpuMemoryBufferType::ANDROID_HARDWARE_BUFFER);
    auto image = VulkanImage::CreateFromGpuMemoryBufferHandle(
        device_queue, std::move(gmb_handle), size, format.vk, usage,
        /*flags=*/0, /*image_tiling=*/VK_IMAGE_TILING_OPTIMAL,
        /*queue_family_index=*/VK_QUEUE_FAMILY_EXTERNAL);
    EXPECT_TRUE(image);
    EXPECT_EQ(image->size(), size);
    EXPECT_EQ(image->format(), format.vk);
    EXPECT_GT(image->device_size(), 0u);
    EXPECT_EQ(image->image_tiling(), VK_IMAGE_TILING_OPTIMAL);
    EXPECT_NE(image->image(), static_cast<VkImage>(VK_NULL_HANDLE));
    EXPECT_NE(image->device_memory(),
              static_cast<VkDeviceMemory>(VK_NULL_HANDLE));
    image->Destroy();
    factory->DestroyGpuMemoryBuffer(id, client_id);
  }
}
#endif

}  // namespace gpu
