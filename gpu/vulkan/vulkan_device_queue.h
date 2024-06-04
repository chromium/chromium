// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_DEVICE_QUEUE_H_
#define GPU_VULKAN_VULKAN_DEVICE_QUEUE_H_

#include <vulkan/vulkan_core.h>

#include <memory>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "gpu/vulkan/vma_wrapper.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "ui/gfx/extension_set.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/pre_freeze_background_memory_trimmer.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace gpu {

class VulkanCommandPool;
class VulkanFenceHelper;
struct GPUInfo;

class COMPONENT_EXPORT(VULKAN) VulkanDeviceQueue
    : public base::trace_event::MemoryDumpProvider {
 public:
  enum DeviceQueueOption {
    GRAPHICS_QUEUE_FLAG = 0x01,
    PRESENTATION_SUPPORT_QUEUE_FLAG = 0x02,
  };

  explicit VulkanDeviceQueue(VkInstance vk_instance);
  explicit VulkanDeviceQueue(VulkanInstance* instance);

  VulkanDeviceQueue(const VulkanDeviceQueue&) = delete;
  VulkanDeviceQueue& operator=(const VulkanDeviceQueue&) = delete;

  ~VulkanDeviceQueue() override;

  using GetPresentationSupportCallback =
      base::RepeatingCallback<bool(VkPhysicalDevice,
                                   const std::vector<VkQueueFamilyProperties>&,
                                   uint32_t queue_family_index)>;
  bool Initialize(
      uint32_t options,
      const GPUInfo* gpu_info,
      const std::vector<const char*>& required_extensions,
      const std::vector<const char*>& optional_extensions,
      bool allow_protected_memory,
      const GetPresentationSupportCallback& get_presentation_support,
      uint32_t heap_memory_limit,
      const bool is_thread_safe = false);

  bool InitializeFromANGLE();

  bool InitializeForWebView(VkPhysicalDevice vk_physical_device,
                            VkDevice vk_device,
                            VkQueue vk_queue,
                            uint32_t vk_queue_index,
                            gfx::ExtensionSet enabled_extensions);

  // To be used by CompositorGpuThread when DrDc is enabled. CompositorGpuThread
  // will use same |vk_device| and |vk_queue| as the gpu main thread but will
  // have its own instance of VulkanFenceHelper and VmaAllocator. Also note that
  // this CompositorGpuThread does not own the |vk_device| and |vk_queue| and
  // hence will not destroy them.
  bool InitializeForCompositorGpuThread(
      VkPhysicalDevice vk_physical_device,
      VkDevice vk_device,
      VkQueue vk_queue,
      uint32_t vk_queue_index,
      gfx::ExtensionSet enabled_extensions,
      const VkPhysicalDeviceFeatures2& vk_physical_device_features2,
      VmaAllocator vma_allocator);

  const gfx::ExtensionSet& enabled_extensions() const {
    return enabled_extensions_;
  }

  void Destroy();

  VkPhysicalDevice GetVulkanPhysicalDevice() const {
    DCHECK_NE(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE),
              vk_physical_device_);
    return vk_physical_device_;
  }

  const VkPhysicalDeviceProperties& vk_physical_device_properties() const {
    return vk_physical_device_properties_;
  }

  const VkPhysicalDeviceDriverProperties& vk_physical_device_driver_properties()
      const {
    return vk_physical_device_driver_properties_;
  }

  uint64_t drm_device_id() const { return drm_device_id_; }

  VkDevice GetVulkanDevice() const {
    DCHECK_NE(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
    return vk_device_;
  }

  VkQueue GetVulkanQueue() const {
    DCHECK_NE(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
    return vk_queue_;
  }

  VkInstance GetVulkanInstance() const { return vk_instance_; }

  uint32_t GetVulkanQueueIndex() const { return vk_queue_index_; }

  std::unique_ptr<gpu::VulkanCommandPool> CreateCommandPool();

  VmaAllocator vma_allocator() const { return vma_allocator_; }

  VulkanFenceHelper* GetFenceHelper() const { return cleanup_helper_.get(); }

  const VkPhysicalDeviceFeatures2& enabled_device_features_2() const {
    if (enabled_device_features_2_from_angle_)
      return *enabled_device_features_2_from_angle_;
    return enabled_device_features_2_;
  }

  const VkPhysicalDeviceFeatures& enabled_device_features() const {
    if (enabled_device_features_2_from_angle_)
      return enabled_device_features_2_from_angle_->features;
    return enabled_device_features_2_.features;
  }

  bool allow_protected_memory() const { return allow_protected_memory_; }

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // Common Init method to be used by both webview and compositor gpu thread.
  bool InitCommon(VkPhysicalDevice vk_physical_device,
                  VkDevice vk_device,
                  VkQueue vk_queue,
                  uint32_t vk_queue_index,
                  gfx::ExtensionSet enabled_extensions);

  gfx::ExtensionSet enabled_extensions_;
  VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties vk_physical_device_properties_;
  VkPhysicalDeviceDriverProperties vk_physical_device_driver_properties_;
  uint64_t drm_device_id_ = 0;
  VkDevice owned_vk_device_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VkQueue vk_queue_ = VK_NULL_HANDLE;
  uint32_t vk_queue_index_ = 0;
  VkInstance vk_instance_ = VK_NULL_HANDLE;
  raw_ptr<VulkanInstance> instance_ = nullptr;
  VmaAllocator owned_vma_allocator_ = VK_NULL_HANDLE;
  VmaAllocator vma_allocator_ = VK_NULL_HANDLE;
  std::unique_ptr<VulkanFenceHelper> cleanup_helper_;
  VkPhysicalDeviceFeatures2 enabled_device_features_2_{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  raw_ptr<const VkPhysicalDeviceFeatures2>
      enabled_device_features_2_from_angle_ = nullptr;

  bool allow_protected_memory_ = false;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<
      const base::android::PreFreezeBackgroundMemoryTrimmer::PreFreezeMetric>
      metric_ = nullptr;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  VkPhysicalDeviceSamplerYcbcrConversionFeatures
      sampler_ycbcr_conversion_features_{
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)
        // || BUILDFLAG(IS_CHROMEOS)

  VkPhysicalDeviceProtectedMemoryFeatures protected_memory_features_{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_DEVICE_QUEUE_H_
