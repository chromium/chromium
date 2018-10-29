// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_instance.h"

#include <unordered_set>
#include <vector>
#include "base/logging.h"
#include "base/macros.h"
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

  VkResult result = VK_SUCCESS;

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Chromium";
  app_info.apiVersion = VK_MAKE_VERSION(1, 0, 2);

  std::vector<const char*> enabled_extensions;
  enabled_extensions.insert(std::end(enabled_extensions),
                            std::begin(required_extensions),
                            std::end(required_extensions));

  uint32_t num_instance_exts = 0;
  result = vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_exts,
                                                  nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceExtensionProperties(NULL) failed: "
                << result;
    return false;
  }

  std::vector<VkExtensionProperties> instance_exts(num_instance_exts);
  result = vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_exts,
                                                  instance_exts.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceExtensionProperties() failed: "
                << result;
    return false;
  }

  for (const VkExtensionProperties& ext_property : instance_exts) {
    if (strcmp(ext_property.extensionName,
               VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0) {
      debug_report_enabled_ = true;
      enabled_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
  }

  std::vector<const char*> enabled_layer_names;
#if DCHECK_IS_ON()
  uint32_t num_instance_layers = 0;
  result = vkEnumerateInstanceLayerProperties(&num_instance_layers, nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceLayerProperties(NULL) failed: "
                << result;
    return false;
  }

  std::vector<VkLayerProperties> instance_layers(num_instance_layers);
  result = vkEnumerateInstanceLayerProperties(&num_instance_layers,
                                              instance_layers.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceLayerProperties() failed: " << result;
    return false;
  }

  std::unordered_set<std::string> desired_layers({
#if defined(USE_X11) && !defined(USE_OZONE)
    "VK_LAYER_LUNARG_standard_validation",
#endif
  });

  for (const VkLayerProperties& layer_property : instance_layers) {
    if (desired_layers.find(layer_property.layerName) != desired_layers.end())
      enabled_layer_names.push_back(layer_property.layerName);
  }
#endif
  enabled_layer_names.insert(std::end(enabled_layer_names),
                             std::begin(required_layers),
                             std::end(required_layers));

  VkInstanceCreateInfo instance_create_info = {};
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create_info.pApplicationInfo = &app_info;
  instance_create_info.enabledLayerCount = enabled_layer_names.size();
  instance_create_info.ppEnabledLayerNames = enabled_layer_names.data();
  instance_create_info.enabledExtensionCount = enabled_extensions.size();
  instance_create_info.ppEnabledExtensionNames = enabled_extensions.data();

  result = vkCreateInstance(&instance_create_info, nullptr, &vk_instance_);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateInstance() failed: " << result;
    return false;
  }

  enabled_extensions_ = gfx::ExtensionSet(std::begin(enabled_extensions),
                                          std::end(enabled_extensions));

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
      DLOG(ERROR) << "vkCreateDebugReportCallbackEXT(ERROR) failed: " << result;
      return false;
    }

    cb_create_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                           VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    cb_create_info.pfnCallback = &VulkanWarningCallback;
    result = vkCreateDebugReportCallbackEXT(vk_instance_, &cb_create_info,
                                            nullptr, &warning_callback_);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkCreateDebugReportCallbackEXT(WARN) failed: " << result;
      return false;
    }
  }
#endif

  if (!vulkan_function_pointers->BindInstanceFunctionPointers(vk_instance_))
    return false;

  if (!vulkan_function_pointers->BindPhysicalDeviceFunctionPointers(
          vk_instance_))
    return false;

  if (gfx::HasExtension(enabled_extensions_, VK_KHR_SURFACE_EXTENSION_NAME)) {
    vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance_, "vkDestroySurfaceKHR"));
    if (!vkDestroySurfaceKHR)
      return false;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            vkGetInstanceProcAddr(vk_instance_,
                                  "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
      return false;

    vkGetPhysicalDeviceSurfaceFormatsKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            vkGetInstanceProcAddr(vk_instance_,
                                  "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    if (!vkGetPhysicalDeviceSurfaceFormatsKHR)
      return false;

    vkGetPhysicalDeviceSurfaceSupportKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
            vkGetInstanceProcAddr(vk_instance_,
                                  "vkGetPhysicalDeviceSurfaceSupportKHR"));
    if (!vkGetPhysicalDeviceSurfaceSupportKHR)
      return false;
  }

  return true;
}

void VulkanInstance::Destroy() {
#if DCHECK_IS_ON()
  if (debug_report_enabled_) {
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance_,
                                  "vkDestroyDebugReportCallbackEXT"));
    DCHECK(vkDestroyDebugReportCallbackEXT);
    vkDestroyDebugReportCallbackEXT(vk_instance_, error_callback_, nullptr);
    vkDestroyDebugReportCallbackEXT(vk_instance_, warning_callback_, nullptr);
  }
#endif
  if (vk_instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(vk_instance_, nullptr);
  }
  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  if (vulkan_function_pointers->vulkan_loader_library_)
    base::UnloadNativeLibrary(vulkan_function_pointers->vulkan_loader_library_);
  vulkan_function_pointers->vulkan_loader_library_ = nullptr;
  vk_instance_ = VK_NULL_HANDLE;
}

}  // namespace gpu
