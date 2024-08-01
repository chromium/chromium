// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <vulkan/vulkan_core.h>

#include <memory>

#include "gpu/vulkan/drm_modifiers_filter_vulkan.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"

namespace gpu {

namespace {

const uint64_t kSupportedModifier1 = 1;
const uint64_t kSupportedModifier2 = 2;
const uint64_t kUnsupportedModifier = 3;
const std::vector<uint64_t> kSupportedModifiers = {kSupportedModifier1,
                                                   kSupportedModifier2};

}  // namespace

class DrmModifiersFilterVulkanTest : public testing::Test {
 public:
  DrmModifiersFilterVulkanTest() = default;

  DrmModifiersFilterVulkanTest(const DrmModifiersFilterVulkanTest&) = delete;
  DrmModifiersFilterVulkanTest& operator=(const DrmModifiersFilterVulkanTest&) =
      delete;

  ~DrmModifiersFilterVulkanTest() override = default;

  void SetUp() override {
    vulkan_impl_ = CreateVulkanImplementation();
    if (!vulkan_impl_ ||
        !vulkan_impl_->InitializeVulkanInstance(false /* using_surface */)) {
      // Some test platforms like linux-chromeos-rel advertise Vulkan support
      // but do not support 1.1. vkGetPhysicalDeviceFormatProperties as used
      // below is part of 1.1 which causes Vulkan initialization (and the test)
      // to fail.
      GTEST_SKIP() << "Unable to initialize Vulkan";
    }
    filter_ = std::make_unique<DrmModifiersFilterVulkan>(vulkan_impl_.get());

    // Chrome's Vulkan interface doesn't have mock bindings yet, so as a
    // workaround we just overwrite the pointers.
    gpu::VulkanFunctionPointers* ptrs = gpu::GetVulkanFunctionPointers();
    cached_vk_fn_ = ptrs->vkGetPhysicalDeviceFormatProperties2.get();
    ptrs->vkGetPhysicalDeviceFormatProperties2.OverrideForTesting(
        [](VkPhysicalDevice p, VkFormat f, VkFormatProperties2* out) {
          EXPECT_NE(nullptr, out);
          EXPECT_NE(nullptr, out->pNext);
          auto* modifiers =
              static_cast<VkDrmFormatModifierPropertiesListEXT*>(out->pNext);
          EXPECT_EQ(VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
                    modifiers->sType);
          if (!modifiers->pDrmFormatModifierProperties) {
            modifiers->drmFormatModifierCount = kSupportedModifiers.size();
            return;
          }
          EXPECT_EQ(kSupportedModifiers.size(),
                    modifiers->drmFormatModifierCount);
          for (size_t i = 0; i < kSupportedModifiers.size(); i++) {
            auto& modifier = modifiers->pDrmFormatModifierProperties[i];
            modifier.drmFormatModifier = kSupportedModifiers[i];
            modifier.drmFormatModifierPlaneCount = 1;
            modifier.drmFormatModifierTilingFeatures = 0;
          }
        });
  }

  void TearDown() override {
    filter_.reset();
    if (cached_vk_fn_) {
      gpu::VulkanFunctionPointers* ptrs = gpu::GetVulkanFunctionPointers();
      ptrs->vkGetPhysicalDeviceFormatProperties2.OverrideForTesting(
          cached_vk_fn_);
    }
  }

 protected:
  std::unique_ptr<VulkanImplementation> vulkan_impl_;
  std::unique_ptr<DrmModifiersFilterVulkan> filter_;
  PFN_vkGetPhysicalDeviceFormatProperties2 cached_vk_fn_;
};

TEST_F(DrmModifiersFilterVulkanTest, FilterUnsupported) {
  std::vector<uint64_t> all_modifiers = {kSupportedModifier1,
                                         kUnsupportedModifier};

  std::vector<uint64_t> filtered_modifiers =
      filter_->Filter(gfx::BufferFormat::BGRX_8888, all_modifiers);

  EXPECT_EQ(1u, filtered_modifiers.size());
  EXPECT_EQ(kSupportedModifier1, filtered_modifiers[0]);
}

}  // namespace gpu
