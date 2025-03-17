// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_INSTANCE_H_
#define GPU_VULKAN_VULKAN_INSTANCE_H_

#include <vulkan/vulkan_core.h>
#include <memory>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "gpu/config/vulkan_info.h"
#include "ui/gfx/extension_set.h"

namespace gpu {

class COMPONENT_EXPORT(VULKAN) VulkanInstance {
 public:
  VulkanInstance();

  VulkanInstance(const VulkanInstance&) = delete;
  VulkanInstance& operator=(const VulkanInstance&) = delete;

  ~VulkanInstance();

  // Creates the vulkan instance.
  //
  // The extensions in |required_extensions| and the layers in |required_layers|
  // will be enabled in the created instance. See the "Extended Functionality"
  // section of vulkan specification for more information.
  bool Initialize(const base::FilePath& vulkan_loader_library_path,
                  const std::vector<const char*>& required_extensions,
                  const std::vector<const char*>& required_layers);

  // These are the steps of `Initialize`. Most caller can use Initialize
  // directly. These are useful if unassigned functions are needed to compute
  // `required_extensions` or `required_layers`.
  bool BindUnassignedFunctionPointers(
      const base::FilePath& vulkan_loader_library_path);
  bool InitializeInstance(const std::vector<const char*>& required_extensions,
                          const std::vector<const char*>& required_layers);

  const VulkanInfo& vulkan_info() const { return vulkan_info_; }

  VkInstance vk_instance() { return vk_instance_; }

  bool is_from_angle() const { return is_from_angle_; }

 private:
  bool CreateInstance(const std::vector<const char*>& required_extensions,
                      const std::vector<const char*>& required_layers);
  bool InitializeFromANGLE(const std::vector<const char*>& required_extensions,
                           const std::vector<const char*>& required_layers);

  bool CollectBasicInfo(const std::vector<const char*>& required_layers);
  bool CollectDeviceInfo(VkPhysicalDevice physical_device = VK_NULL_HANDLE);
  void Destroy();

  const bool is_from_angle_;

  VulkanInfo vulkan_info_;

  base::NativeLibrary loader_library_ = nullptr;

  VkInstance owned_vk_instance_ = VK_NULL_HANDLE;
  VkInstance vk_instance_ = VK_NULL_HANDLE;
  bool debug_report_enabled_ = false;
#if DCHECK_IS_ON()
  VkDebugReportCallbackEXT error_callback_ = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT warning_callback_ = VK_NULL_HANDLE;
#endif
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_INSTANCE_H_
