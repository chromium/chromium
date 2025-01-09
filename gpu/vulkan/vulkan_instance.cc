// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/vulkan/vulkan_instance.h"

#include <vector>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_crash_keys.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gl/gl_angle_util_vulkan.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <sys/sysmacros.h>
#endif

namespace gpu {

namespace {

#if DCHECK_IS_ON()
constexpr const char* kSkippedErrors[] = {
    // http://anglebug.com/4583
    "VUID-VkGraphicsPipelineCreateInfo-blendEnable-02023",
};

VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanErrorCallback(VkDebugReportFlagsEXT flags,
                    VkDebugReportObjectTypeEXT object_type,
                    uint64_t object,
                    size_t location,
                    int32_t message_code,
                    const char* layer_prefix,
                    const char* message,
                    void* user_data) {
  static bool encountered_errors[std::size(kSkippedErrors)];
  for (size_t i = 0; i < std::size(kSkippedErrors); ++i) {
    if (strstr(message, kSkippedErrors[i])) {
      if (encountered_errors[i]) {
        return VK_FALSE;
      }
      encountered_errors[i] = true;
    }
  }
  LOG(ERROR) << message;
  return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanWarningCallback(VkDebugReportFlagsEXT flags,
                      VkDebugReportObjectTypeEXT object_type,
                      uint64_t object,
                      size_t location,
                      int32_t message_code,
                      const char* layer_prefix,
                      const char* message,
                      void* user_data) {
  LOG(WARNING) << message;
  return VK_FALSE;
}
#endif  // DCHECK_IS_ON()

}  // namespace

VulkanInstance::VulkanInstance()
    : is_from_angle_(base::FeatureList::IsEnabled(features::kVulkanFromANGLE)) {
}

VulkanInstance::~VulkanInstance() {
  Destroy();
}

bool VulkanInstance::Initialize(
    const base::FilePath& vulkan_loader_library_path,
    const std::vector<const char*>& required_extensions,
    const std::vector<const char*>& required_layers) {
  if (!BindUnassignedFunctionPointers(vulkan_loader_library_path))
    return false;
  return InitializeInstance(required_extensions, required_layers);
}

bool VulkanInstance::BindUnassignedFunctionPointers(
    const base::FilePath& vulkan_loader_library_path) {
  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  if (is_from_angle_) {
    PFN_vkGetInstanceProcAddr proc = gl::QueryVkGetInstanceProcAddrFromANGLE();
    if (!proc) {
      LOG(ERROR) << "Failed to get vkGetInstanceProcAddr pointer from ANGLE.";
      return false;
    }
    if (!vulkan_function_pointers
             ->BindUnassociatedFunctionPointersFromGetProcAddr(proc)) {
      return false;
    }
  } else {
    base::NativeLibraryLoadError error;
    loader_library_ =
        base::LoadNativeLibrary(vulkan_loader_library_path, &error);
    if (!loader_library_) {
      LOG(ERROR) << "Failed to load '" << vulkan_loader_library_path
                 << "': " << error.ToString();
      return false;
    }
    if (!vulkan_function_pointers
             ->BindUnassociatedFunctionPointersFromLoaderLib(loader_library_)) {
      return false;
    }
  }
  return true;
}

bool VulkanInstance::InitializeInstance(
    const std::vector<const char*>& required_extensions,
    const std::vector<const char*>& required_layers) {
  if (is_from_angle_)
    return InitializeFromANGLE(required_extensions, required_layers);
  return CreateInstance(required_extensions, required_layers);
}

bool VulkanInstance::CreateInstance(
    const std::vector<const char*>& required_extensions,
    const std::vector<const char*>& required_layers) {
  DCHECK(!vk_instance_);

  if (!CollectBasicInfo(required_layers))
    return false;

  vulkan_info_.used_api_version = kVulkanRequiredApiVersion;

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Chromium";
  app_info.apiVersion = vulkan_info_.used_api_version;

  vulkan_info_.enabled_instance_extensions = required_extensions;

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

  VkInstanceCreateInfo instance_create_info = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,  // sType
      nullptr,                                 // pNext
      0,                                       // flags
      &app_info,                               // pApplicationInfo
      base::checked_cast<uint32_t>(required_layers.size()),
      // enableLayerCount
      required_layers.data(),  // ppEnabledLayerNames
      base::checked_cast<uint32_t>(
          vulkan_info_.enabled_instance_extensions.size()),
      // enabledExtensionCount
      vulkan_info_.enabled_instance_extensions.data(),
      // ppEnabledExtensionNames
  };

  VkResult result =
      vkCreateInstance(&instance_create_info, nullptr, &owned_vk_instance_);
  if (VK_SUCCESS != result) {
    LOG(ERROR) << "vkCreateInstance() failed: " << result;
    return false;
  }
  vk_instance_ = owned_vk_instance_;

  gfx::ExtensionSet enabled_extensions(
      std::begin(vulkan_info_.enabled_instance_extensions),
      std::end(vulkan_info_.enabled_instance_extensions));

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  if (!vulkan_function_pointers->BindInstanceFunctionPointers(
          vk_instance_, vulkan_info_.used_api_version, enabled_extensions)) {
    return false;
  }

#if DCHECK_IS_ON()
  // Register our error logging function.
  if (debug_report_enabled_) {
    VkDebugReportCallbackCreateInfoEXT cb_create_info = {};
    cb_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;

    cb_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
    cb_create_info.pfnCallback = &VulkanErrorCallback;
    result = vkCreateDebugReportCallbackEXT(vk_instance_, &cb_create_info,
                                            nullptr, &error_callback_);
    if (VK_SUCCESS != result) {
      error_callback_ = VK_NULL_HANDLE;
      LOG(ERROR) << "vkCreateDebugReportCallbackEXT(ERROR) failed: " << result;
      return false;
    }

    cb_create_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                           VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    cb_create_info.pfnCallback = &VulkanWarningCallback;
    result = vkCreateDebugReportCallbackEXT(vk_instance_, &cb_create_info,
                                            nullptr, &warning_callback_);
    if (VK_SUCCESS != result) {
      warning_callback_ = VK_NULL_HANDLE;
      LOG(ERROR) << "vkCreateDebugReportCallbackEXT(WARN) failed: " << result;
      return false;
    }
  }
#endif

  if (!CollectDeviceInfo())
    return false;

  return true;
}

bool VulkanInstance::InitializeFromANGLE(
    const std::vector<const char*>& required_extensions,
    const std::vector<const char*>& required_layers) {
  vk_instance_ = gl::QueryVkInstanceFromANGLE();
  if (vk_instance_ == VK_NULL_HANDLE)
    return false;

  uint32_t api_version = gl::QueryVkVersionFromANGLE();
  if (api_version < kVulkanRequiredApiVersion)
    return false;

  if (!CollectBasicInfo({}))
    return false;

  vulkan_info_.used_api_version = api_version;

  auto extensions = gl::QueryVkInstanceExtensionsFromANGLE();

  for (const auto& extension : extensions)
    vulkan_info_.enabled_instance_extensions.push_back(extension.data());

#if DCHECK_IS_ON()
  for (const char* required_extension_name : required_extensions) {
    bool found = false;
    for (const char* enabled_extension :
         vulkan_info_.enabled_instance_extensions) {
      if (strcmp(required_extension_name, enabled_extension) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      DLOG(ERROR) << "Required extension " << required_extension_name
                  << " missing from enumerated Vulkan extensions. "
                     "vkCreateInstance will may fail but could succeed if "
                     "extension has been promoted to core.";
    }
  }
#endif

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  if (!vulkan_function_pointers->BindInstanceFunctionPointers(
          vk_instance_, vulkan_info_.used_api_version, extensions)) {
    return false;
  }

  VkPhysicalDevice physical_device = gl::QueryVkPhysicalDeviceFromANGLE();
  if (physical_device == VK_NULL_HANDLE)
    return false;

  if (!CollectDeviceInfo(physical_device))
    return false;

  return true;
}

bool VulkanInstance::CollectBasicInfo(
    const std::vector<const char*>& required_layers) {
  VkResult result = vkEnumerateInstanceVersion(&vulkan_info_.api_version);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkEnumerateInstanceVersion() failed: " << result;
    return false;
  }

  if (vulkan_info_.api_version < kVulkanRequiredApiVersion)
    return false;

  gpu::crash_keys::vulkan_api_version.Set(
      VkVersionToString(vulkan_info_.api_version));

  // Query the extensions from all layers, including ones that are implicitly
  // available (identified by passing a null ptr as the layer name).
  std::vector<const char*> all_required_layers = required_layers;

  // Include the extension properties provided by the Vulkan implementation as
  // part of the enumeration.
  all_required_layers.push_back(nullptr);

  for (const char* layer_name : all_required_layers) {
    uint32_t num_instance_exts = 0;
    result = vkEnumerateInstanceExtensionProperties(
        layer_name, &num_instance_exts, nullptr);
    if (VK_SUCCESS != result) {
      LOG(ERROR) << "vkEnumerateInstanceExtensionProperties("
                 << (layer_name ? layer_name : "nullptr")
                 << ") failed: " << result;
      return false;
    }

    const size_t previous_extension_count =
        vulkan_info_.instance_extensions.size();
    vulkan_info_.instance_extensions.resize(previous_extension_count +
                                            num_instance_exts);
    result = vkEnumerateInstanceExtensionProperties(
        layer_name, &num_instance_exts,
        &vulkan_info_.instance_extensions.data()[previous_extension_count]);
    if (VK_SUCCESS != result) {
      LOG(ERROR) << "vkEnumerateInstanceExtensionProperties("
                 << (layer_name ? layer_name : "nullptr")
                 << ") failed: " << result;
      return false;
    }
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

  uint32_t num_instance_layers = 0;
  result = vkEnumerateInstanceLayerProperties(&num_instance_layers, nullptr);
  if (VK_SUCCESS != result) {
    LOG(ERROR) << "vkEnumerateInstanceLayerProperties(NULL) failed: " << result;
    return false;
  }

  vulkan_info_.instance_layers.resize(num_instance_layers);
  result = vkEnumerateInstanceLayerProperties(
      &num_instance_layers, vulkan_info_.instance_layers.data());
  if (VK_SUCCESS != result) {
    LOG(ERROR) << "vkEnumerateInstanceLayerProperties() failed: " << result;
    return false;
  }

  return true;
}

bool VulkanInstance::CollectDeviceInfo(VkPhysicalDevice physical_device) {
  std::vector<VkPhysicalDevice> physical_devices;
  if (physical_device == VK_NULL_HANDLE) {
    uint32_t count = 0;
    VkResult result = vkEnumeratePhysicalDevices(vk_instance_, &count, nullptr);
    if (result != VK_SUCCESS) {
      LOG(ERROR) << "vkEnumeratePhysicalDevices failed: " << result;
      return false;
    }

    if (!count) {
      LOG(ERROR) << "vkEnumeratePhysicalDevices returns zero device.";
      return false;
    }

    physical_devices.resize(count);
    result = vkEnumeratePhysicalDevices(vk_instance_, &count,
                                        physical_devices.data());
    if (VK_SUCCESS != result) {
      LOG(ERROR) << "vkEnumeratePhysicalDevices() failed: " << result;
      return false;
    }
  } else {
    physical_devices.push_back(physical_device);
  }

  vulkan_info_.physical_devices.reserve(physical_devices.size());
  for (VkPhysicalDevice device : physical_devices) {
    vulkan_info_.physical_devices.emplace_back();
    auto& info = vulkan_info_.physical_devices.back();
    info.device = device;

    vkGetPhysicalDeviceProperties(device, &info.properties);

    uint32_t count = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(
        device, nullptr /* pLayerName */, &count, nullptr);
    LOG_IF(ERROR, result != VK_SUCCESS)
        << "vkEnumerateDeviceExtensionProperties failed: " << result;

    info.extensions.resize(count);
    result = vkEnumerateDeviceExtensionProperties(
        device, nullptr /* pLayerName */, &count, info.extensions.data());
    LOG_IF(ERROR, result != VK_SUCCESS)
        << "vkEnumerateDeviceExtensionProperties failed: " << result;

    // The API version of the VkInstance might be different than the supported
    // API version of the VkPhysicalDevice, so we need to check the GPU's
    // API version instead of just testing to see if
    // vkGetPhysicalDeviceProperties2 and vkGetPhysicalDeviceFeatures2 are
    // non-null.
    static_assert(kVulkanRequiredApiVersion >= VK_API_VERSION_1_1, "");
    if (info.properties.apiVersion >= kVulkanRequiredApiVersion) {
      bool has_drm_extension =
          base::ranges::any_of(info.extensions, [](const auto& ext) {
            return strcmp(ext.extensionName,
                          VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME) == 0;
          });

      info.driver_properties = VkPhysicalDeviceDriverProperties{
          .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
      };

      VkPhysicalDeviceDrmPropertiesEXT drm_properties = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT};
      if (has_drm_extension) {
        info.driver_properties.pNext = &drm_properties;
      }

      VkPhysicalDeviceProperties2 properties2 = {
          .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
          .pNext = &info.driver_properties,
      };
      vkGetPhysicalDeviceProperties2(device, &properties2);

#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (has_drm_extension &&
          (drm_properties.hasRender || drm_properties.hasPrimary)) {
        static_assert(sizeof(dev_t) <= sizeof(info.drm_device_id),
                      "unexpected dev_t size");
        if (drm_properties.hasRender) {
          info.drm_device_id =
              makedev(drm_properties.renderMajor, drm_properties.renderMinor);
        } else {
          info.drm_device_id =
              makedev(drm_properties.primaryMajor, drm_properties.primaryMinor);
        }
        DCHECK(info.drm_device_id);
      }
#endif

      VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcr_conversion_features =
          {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
      VkPhysicalDeviceProtectedMemoryFeatures protected_memory_feature = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES};
      VkPhysicalDeviceFeatures2 features_2 = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      features_2.pNext = &ycbcr_conversion_features;
      ycbcr_conversion_features.pNext = &protected_memory_feature;

      vkGetPhysicalDeviceFeatures2(device, &features_2);
      info.features = features_2.features;
      info.feature_sampler_ycbcr_conversion =
          ycbcr_conversion_features.samplerYcbcrConversion;
      info.feature_protected_memory = protected_memory_feature.protectedMemory;
    }

    count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    if (count) {
      info.queue_families.resize(count);
      vkGetPhysicalDeviceQueueFamilyProperties(device, &count,
                                               info.queue_families.data());
    }
  }
  return true;
}

void VulkanInstance::Destroy() {
#if DCHECK_IS_ON()
  if (debug_report_enabled_ && (error_callback_ != VK_NULL_HANDLE ||
                                warning_callback_ != VK_NULL_HANDLE)) {
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
  if (owned_vk_instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(owned_vk_instance_, nullptr);
    owned_vk_instance_ = VK_NULL_HANDLE;
  }
  vk_instance_ = VK_NULL_HANDLE;

  if (loader_library_) {
    base::UnloadNativeLibrary(loader_library_);
    loader_library_ = nullptr;
  }
}

}  // namespace gpu
