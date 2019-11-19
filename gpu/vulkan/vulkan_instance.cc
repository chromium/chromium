// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_instance.h"

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanErrorCallback(VkDebugReportFlagsEXT flags,
                    VkDebugReportObjectTypeEXT objectType,
                    uint64_t object,
                    size_t location,
                    int32_t messageCode,
                    const char* pLayerPrefix,
                    const char* pMessage,
                    void* pUserData) {
  LOG(ERROR) << pMessage;
  return VK_TRUE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanWarningCallback(VkDebugReportFlagsEXT flags,
                      VkDebugReportObjectTypeEXT objectType,
                      uint64_t object,
                      size_t location,
                      int32_t messageCode,
                      const char* pLayerPrefix,
                      const char* pMessage,
                      void* pUserData) {
  LOG(WARNING) << pMessage;
  return VK_TRUE;
}

VulkanInstance::VulkanInstance() {}

VulkanInstance::~VulkanInstance() {
  Destroy();
}

bool VulkanInstance::Initialize(
    const std::vector<const char*>& required_extensions,
    const std::vector<const char*>& required_layers) {
  DCHECK(!vk_instance_);

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  if (!vulkan_function_pointers->BindUnassociatedFunctionPointers())
    return false;

  if (vulkan_function_pointers->vkEnumerateInstanceVersionFn) {
    vulkan_function_pointers->vkEnumerateInstanceVersionFn(
        &vulkan_info_.api_version);
  }

#if defined(OS_ANDROID)
  // Ensure that android works only with vulkan apiVersion >= 1.1. Vulkan will
  // only be enabled for Android P+ and Android P+ requires vulkan
  // apiVersion >= 1.1.
  if (vulkan_info_.api_version < VK_MAKE_VERSION(1, 1, 0))
    return false;
#endif

  // Use Vulkan 1.1 if it's available.
  vulkan_info_.used_api_version =
      (vulkan_info_.api_version >= VK_MAKE_VERSION(1, 1, 0))
          ? VK_MAKE_VERSION(1, 1, 0)
          : VK_MAKE_VERSION(1, 0, 0);

  VkResult result = VK_SUCCESS;

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Chromium";
  app_info.apiVersion = vulkan_info_.used_api_version;

  vulkan_info_.enabled_instance_extensions = required_extensions;
  uint32_t num_instance_exts = 0;
  result = vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_exts,
                                                  nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceExtensionProperties(NULL) failed: "
                << result;
    return false;
  }

  vulkan_info_.instance_extensions.resize(num_instance_exts);
  result = vkEnumerateInstanceExtensionProperties(
      nullptr, &num_instance_exts, vulkan_info_.instance_extensions.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceExtensionProperties() failed: "
                << result;
    return false;
  }

  for (const VkExtensionProperties& ext_property :
       vulkan_info_.instance_extensions) {
    if (strcmp(ext_property.extensionName,
               VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0) {
      debug_report_enabled_ = true;
      vulkan_info_.enabled_instance_extensions.push_back(
          VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
  }

#if DCHECK_IS_ON()
  for (const char* enabled_extension :
       vulkan_info_.enabled_instance_extensions) {
    bool found = false;
    for (const VkExtensionProperties& ext_property :
         vulkan_info_.instance_extensions) {
      if (strcmp(ext_property.extensionName, enabled_extension) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      DLOG(ERROR) << "Required extension " << enabled_extension
                  << " missing from enumerated Vulkan extensions. "
                     "vkCreateInstance will likely fail.";
    }
  }
#endif

  std::vector<const char*> enabled_layer_names = required_layers;
  uint32_t num_instance_layers = 0;
  result = vkEnumerateInstanceLayerProperties(&num_instance_layers, nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceLayerProperties(NULL) failed: "
                << result;
    return false;
  }

  vulkan_info_.instance_layers.resize(num_instance_layers);
  result = vkEnumerateInstanceLayerProperties(
      &num_instance_layers, vulkan_info_.instance_layers.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceLayerProperties() failed: " << result;
    return false;
  }

  gfx::ExtensionSet enabled_extensions(
      std::begin(vulkan_info_.enabled_instance_extensions),
      std::end(vulkan_info_.enabled_instance_extensions));

#if DCHECK_IS_ON()
  // TODO(crbug.com/843346): Make validation work in combination with
  // VK_KHR_xlib_surface or switch to VK_KHR_xcb_surface.
  bool require_xlib_surface_extension =
      gfx::HasExtension(enabled_extensions, "VK_KHR_xlib_surface");

  // VK_LAYER_KHRONOS_validation 1.1.106 is required to support
  // VK_KHR_xlib_surface.
  constexpr base::StringPiece standard_validation(
      "VK_LAYER_KHRONOS_validation");
  for (const VkLayerProperties& layer_property : vulkan_info_.instance_layers) {
    if (standard_validation != layer_property.layerName)
      continue;
    if (!require_xlib_surface_extension ||
        layer_property.specVersion >= VK_MAKE_VERSION(1, 1, 106)) {
      enabled_layer_names.push_back(standard_validation.data());
    }
    break;
  }
#endif  // DCHECK_IS_ON()

  VkInstanceCreateInfo instance_create_info = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,           // sType
      nullptr,                                          // pNext
      0,                                                // flags
      &app_info,                                        // pApplicationInfo
      enabled_layer_names.size(),                       // enableLayerCount
      enabled_layer_names.data(),                       // ppEnabledLayerNames
      vulkan_info_.enabled_instance_extensions.size(),  // enabledExtensionCount
      vulkan_info_.enabled_instance_extensions
          .data(),  // ppEnabledExtensionNames
  };

  result = vkCreateInstance(&instance_create_info, nullptr, &vk_instance_);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateInstance() failed: " << result;
    return false;
  }

#if DCHECK_IS_ON()
  // Register our error logging function.
  if (debug_report_enabled_) {
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance_,
                                  "vkCreateDebugReportCallbackEXT"));
    DCHECK(vkCreateDebugReportCallbackEXT);

    VkDebugReportCallbackCreateInfoEXT cb_create_info = {};
    cb_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;

    cb_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
    cb_create_info.pfnCallback = &VulkanErrorCallback;
    result = vkCreateDebugReportCallbackEXT(vk_instance_, &cb_create_info,
                                            nullptr, &error_callback_);
    if (VK_SUCCESS != result) {
      error_callback_ = VK_NULL_HANDLE;
      DLOG(ERROR) << "vkCreateDebugReportCallbackEXT(ERROR) failed: " << result;
      return false;
    }

    cb_create_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                           VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    cb_create_info.pfnCallback = &VulkanWarningCallback;
    result = vkCreateDebugReportCallbackEXT(vk_instance_, &cb_create_info,
                                            nullptr, &warning_callback_);
    if (VK_SUCCESS != result) {
      warning_callback_ = VK_NULL_HANDLE;
      DLOG(ERROR) << "vkCreateDebugReportCallbackEXT(WARN) failed: " << result;
      return false;
    }
  }
#endif

  if (!vulkan_function_pointers->BindInstanceFunctionPointers(
          vk_instance_, vulkan_info_.used_api_version, enabled_extensions)) {
    return false;
  }

  CollectInfo();
  return true;
}

void VulkanInstance::CollectInfo() {
  uint32_t count = 0;
  VkResult result = vkEnumeratePhysicalDevices(vk_instance_, &count, nullptr);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkEnumeratePhysicalDevices failed: " << result;
  }

  std::vector<VkPhysicalDevice> physical_devices(count);
  result =
      vkEnumeratePhysicalDevices(vk_instance_, &count, physical_devices.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumeratePhysicalDevices() failed: " << result;
    return;
  }

  vulkan_info_.physical_devices.reserve(count);
  for (VkPhysicalDevice device : physical_devices) {
    vulkan_info_.physical_devices.emplace_back();
    auto& info = vulkan_info_.physical_devices.back();
    info.device = device;

    vkGetPhysicalDeviceProperties(device, &info.properties);

    count = 0;
    result = vkEnumerateDeviceLayerProperties(device, &count, nullptr);
    DLOG_IF(ERROR, result != VK_SUCCESS)
        << "vkEnumerateDeviceLayerProperties failed: " << result;

    info.layers.resize(count);
    result =
        vkEnumerateDeviceLayerProperties(device, &count, info.layers.data());
    DLOG_IF(ERROR, result != VK_SUCCESS)
        << "vkEnumerateDeviceLayerProperties failed: " << result;

    // The API version of the VkInstance might be different than the supported
    // API version of the VkPhysicalDevice, so we need to check the GPU's
    // API version instead of just testing to see if
    // vkGetPhysicalDeviceFeatures2 is non-null.
    if (info.properties.apiVersion >= VK_MAKE_VERSION(1, 1, 0)) {
      VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcr_converson_features =
          {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
      VkPhysicalDeviceProtectedMemoryFeatures protected_memory_feature = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES};
      VkPhysicalDeviceFeatures2 features_2 = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      features_2.pNext = &ycbcr_converson_features;
      ycbcr_converson_features.pNext = &protected_memory_feature;

      vkGetPhysicalDeviceFeatures2(device, &features_2);
      info.features = features_2.features;
      info.feature_sampler_ycbcr_conversion =
          ycbcr_converson_features.samplerYcbcrConversion;
      info.feature_protected_memory = protected_memory_feature.protectedMemory;
    } else {
      vkGetPhysicalDeviceFeatures(device, &info.features);
    }

    count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    if (count) {
      info.queue_families.resize(count);
      vkGetPhysicalDeviceQueueFamilyProperties(device, &count,
                                               info.queue_families.data());
    }
  }
}

void VulkanInstance::Destroy() {
#if DCHECK_IS_ON()
  if (debug_report_enabled_ && (error_callback_ != VK_NULL_HANDLE ||
                                warning_callback_ != VK_NULL_HANDLE)) {
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance_,
                                  "vkDestroyDebugReportCallbackEXT"));
    DCHECK(vkDestroyDebugReportCallbackEXT);
    if (error_callback_ != VK_NULL_HANDLE) {
      vkDestroyDebugReportCallbackEXT(vk_instance_, error_callback_, nullptr);
      error_callback_ = VK_NULL_HANDLE;
    }
    if (warning_callback_ != VK_NULL_HANDLE) {
      vkDestroyDebugReportCallbackEXT(vk_instance_, warning_callback_, nullptr);
      warning_callback_ = VK_NULL_HANDLE;
    }
  }
#endif
  if (vk_instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(vk_instance_, nullptr);
    vk_instance_ = VK_NULL_HANDLE;
  }
  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  if (vulkan_function_pointers->vulkan_loader_library_)
    base::UnloadNativeLibrary(vulkan_function_pointers->vulkan_loader_library_);
  vulkan_function_pointers->vulkan_loader_library_ = nullptr;
}

}  // namespace gpu
