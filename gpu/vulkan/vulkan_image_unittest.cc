// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/vulkan/tests/basic_vulkan_test.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gfx/geometry/rect.h"

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
      EXPECT_TRUE(scoped_handle.is_valid())
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(VulkanImageTest, RejectUndersizedDmaBuf) {
  auto* device_queue = GetDeviceQueue();
  if (!device_queue) {
    return;
  }

  // 1. Create a small anonymous file (memfd) to simulate an undersized dma-buf.
  int fd = memfd_create("undersized_dmabuf", 0);
  ASSERT_GE(fd, 0);
  base::ScopedFD scoped_fd(fd);
  // Give it a tiny size (e.g., 4KB).
  ASSERT_EQ(ftruncate(fd, 4096), 0);

  // 2. Construct a GpuMemoryBufferHandle claiming it's large (e.g., 1024x1024).
  constexpr gfx::Size image_size(1024, 1024);
  const int stride = image_size.width() * 4;  // 4 bytes per pixel
  const uint64_t size = static_cast<uint64_t>(stride) * image_size.height();

  gfx::NativePixmapHandle native_pixmap_handle;
  native_pixmap_handle.modifier = gfx::NativePixmapHandle::kNoModifier;
  native_pixmap_handle.planes.emplace_back(stride, 0, size,
                                           base::ScopedFD(dup(fd)));

  gfx::GpuMemoryBufferHandle gmb_handle(std::move(native_pixmap_handle));

  // 3. Verify that InitializeFromGpuMemoryBufferHandle rejects it.
  auto image = VulkanImage::CreateFromGpuMemoryBufferHandle(
      device_queue, std::move(gmb_handle), image_size, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_SAMPLED_BIT, 0, VK_IMAGE_TILING_LINEAR,
      VK_QUEUE_FAMILY_IGNORED);
  EXPECT_FALSE(image);
}
#endif

}  // namespace gpu
