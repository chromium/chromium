// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

// This has to be included before windows.h.
#include "third_party/re2/src/re2/re2.h"

#include <windows.h>

#include <cfgmgr32.h>
#include <d3d11.h>
#include <d3d12.h>
#include <d3d9.h>
#include <dxgi.h>
#include <setupapi.h>
#include <stddef.h>
#include <stdint.h>

#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_native_library.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_com_initializer.h"
#include "gpu/config/nvml_info.h"
#include "third_party/vulkan/include/vulkan/vulkan.h"

namespace gpu {

namespace {

void DeviceIDToVendorAndDevice(const std::wstring& id,
                               uint32_t* vendor_id,
                               uint32_t* device_id) {
  *vendor_id = 0;
  *device_id = 0;
  if (id.length() < 21)
    return;
  base::string16 vendor_id_string = id.substr(8, 4);
  base::string16 device_id_string = id.substr(17, 4);
  int vendor = 0;
  int device = 0;
  base::HexStringToInt(base::UTF16ToASCII(vendor_id_string), &vendor);
  base::HexStringToInt(base::UTF16ToASCII(device_id_string), &device);
  *vendor_id = vendor;
  *device_id = device;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum D3DFeatureLevel in \tools\metrics\histograms\enums.xml
enum class D3D12FeatureLevel {
  kD3DFeatureLevelUnknown = 0,
  kD3DFeatureLevel_12_0 = 1,
  kD3DFeatureLevel_12_1 = 2,
  kMaxValue = kD3DFeatureLevel_12_1,
};

inline D3D12FeatureLevel ConvertToHistogramFeatureLevel(
    uint32_t d3d_feature_level) {
  switch (d3d_feature_level) {
    case 0:
      return D3D12FeatureLevel::kD3DFeatureLevelUnknown;
    case D3D_FEATURE_LEVEL_12_0:
      return D3D12FeatureLevel::kD3DFeatureLevel_12_0;
    case D3D_FEATURE_LEVEL_12_1:
      return D3D12FeatureLevel::kD3DFeatureLevel_12_1;
    default:
      NOTREACHED();
      return D3D12FeatureLevel::kD3DFeatureLevelUnknown;
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum VulkanVersion in \tools\metrics\histograms\enums.xml
enum class VulkanVersion {
  kVulkanVersionUnknown = 0,
  kVulkanVersion_1_0_0 = 1,
  kVulkanVersion_1_1_0 = 2,
  kMaxValue = kVulkanVersion_1_1_0,
};

inline VulkanVersion ConvertToHistogramVulkanVersion(uint32_t vulkan_version) {
  switch (vulkan_version) {
    case 0:
      return VulkanVersion::kVulkanVersionUnknown;
    case VK_MAKE_VERSION(1, 0, 0):
      return VulkanVersion::kVulkanVersion_1_0_0;
    case VK_MAKE_VERSION(1, 1, 0):
      return VulkanVersion::kVulkanVersion_1_1_0;
    default:
      NOTREACHED();
      return VulkanVersion::kVulkanVersionUnknown;
  }
}

}  // namespace

#if defined(GOOGLE_CHROME_BUILD) && defined(OFFICIAL_BUILD)
// This function has a real implementation for official builds that can
// be found in src/third_party/amd.
bool GetAMDSwitchableInfo(bool* is_switchable,
                          uint32_t* active_vendor_id,
                          uint32_t* active_device_id);
#else
bool GetAMDSwitchableInfo(bool* is_switchable,
                          uint32_t* active_vendor_id,
                          uint32_t* active_device_id) {
  return false;
}
#endif

std::string ParseNVIDIARegistryDriverVersion(std::string registry_version) {
  // The NVIDIA driver version in the registry is most commonly in the format:
  // XX.XX.XD.DDDD
  // Where "X" corresponds to an OS-specific digit and "D" corresponds to a
  // digit of the actual driver version. We convert it to the following format:
  // DDD.DD
  // This matches with the actual driver version that NVML also returns.
  std::string second_to_last_digits;
  std::string last_digits;
  if (!RE2::FullMatch(registry_version, "\\d+\\.\\d+\\.(\\d+)\\.(\\d+)",
                      &second_to_last_digits, &last_digits)) {
    return registry_version;
  }
  std::string digits = second_to_last_digits + last_digits;
  if (digits.length() < 5u) {
    return registry_version;
  }
  digits.erase(0, digits.length() - 5u);
  DCHECK(digits.length() == 5u);
  return digits.substr(0u, 3u) + "." + digits.substr(3u);
}

bool CollectDriverInfoD3D(const std::wstring& device_id, GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectDriverInfoD3D");

  // Display adapter class GUID from
  // https://msdn.microsoft.com/en-us/library/windows/hardware/ff553426%28v=vs.85%29.aspx
  GUID display_class = {0x4d36e968,
                        0xe325,
                        0x11ce,
                        {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

  // create device info for the display device
  HDEVINFO device_info =
      ::SetupDiGetClassDevs(&display_class, nullptr, nullptr, DIGCF_PRESENT);
  if (device_info == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Creating device info failed";
    return false;
  }

  std::vector<GPUInfo::GPUDevice> devices;

  size_t primary_device = std::numeric_limits<size_t>::max();
  bool found_amd = false;
  bool found_intel = false;
  bool amd_is_primary = false;

  DWORD index = 0;
  SP_DEVINFO_DATA device_info_data;
  device_info_data.cbSize = sizeof(device_info_data);
  while (SetupDiEnumDeviceInfo(device_info, index++, &device_info_data)) {
    WCHAR value[255];
    if (SetupDiGetDeviceRegistryPropertyW(
            device_info, &device_info_data, SPDRP_DRIVER, nullptr,
            reinterpret_cast<PBYTE>(value), sizeof(value), nullptr)) {
      HKEY key;
      std::wstring driver_key = L"System\\CurrentControlSet\\Control\\Class\\";
      driver_key += value;
      LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, driver_key.c_str(), 0,
                                  KEY_QUERY_VALUE, &key);
      if (result == ERROR_SUCCESS) {
        GPUInfo::GPUDevice device;
        DWORD dwcb_data = sizeof(value);
        result = RegQueryValueExW(key, L"DriverVersion", nullptr, nullptr,
                                  reinterpret_cast<LPBYTE>(value), &dwcb_data);
        if (result == ERROR_SUCCESS)
          device.driver_version = base::UTF16ToASCII(std::wstring(value));

        dwcb_data = sizeof(value);
        result = RegQueryValueExW(key, L"DriverDate", nullptr, nullptr,
                                  reinterpret_cast<LPBYTE>(value), &dwcb_data);
        if (result == ERROR_SUCCESS)
          device.driver_date = base::UTF16ToASCII(std::wstring(value));

        dwcb_data = sizeof(value);
        result = RegQueryValueExW(key, L"ProviderName", nullptr, nullptr,
                                  reinterpret_cast<LPBYTE>(value), &dwcb_data);
        if (result == ERROR_SUCCESS)
          device.driver_vendor = base::UTF16ToASCII(std::wstring(value));

        wchar_t new_device_id[MAX_DEVICE_ID_LEN];
        CONFIGRET status = CM_Get_Device_ID(
            device_info_data.DevInst, new_device_id, MAX_DEVICE_ID_LEN, 0);

        if (status == CR_SUCCESS) {
          std::wstring id = new_device_id;
          DeviceIDToVendorAndDevice(id, &(device.vendor_id),
                                    &(device.device_id));
          if (id.compare(0, device_id.size(), device_id) == 0) {
            primary_device = devices.size();
            if (device.vendor_id == 0x1002)
              amd_is_primary = true;
          }
          if (device.vendor_id == 0x8086)
            found_intel = true;
          if (device.vendor_id == 0x1002)
            found_amd = true;
          if (device.vendor_id == 0x10de) {
            std::string nvml_driver_version;
            int major_cuda_compute_capability = 0;
            int minor_cuda_compute_capability = 0;
            bool nvml_success = GetNvmlDeviceInfo(
                device.device_id, &nvml_driver_version,
                &major_cuda_compute_capability, &minor_cuda_compute_capability);
            if (nvml_success) {
              // We use the NVML driver version instead of the registry version,
              // since the registry version includes OS-specific digits that are
              // not part of the actual driver version.
              device.driver_version = nvml_driver_version;
              device.cuda_compute_capability_major =
                  major_cuda_compute_capability;
            } else {
              // If we can't get the actual driver version from NVML, do
              // best-effort parsing of the actual driver version from the
              // registry driver version.
              device.driver_version =
                  ParseNVIDIARegistryDriverVersion(device.driver_version);
            }
          }
          devices.push_back(device);
        }

        RegCloseKey(key);
      }
    }
  }
  SetupDiDestroyDeviceInfoList(device_info);
  if (found_amd && found_intel) {
    // Potential AMD Switchable system found.
    if (!amd_is_primary) {
      // Some machines aren't properly detected as AMD switchable, but count
      // them anyway. This may erroneously count machines where there are
      // independent AMD and Intel cards and the AMD isn't hooked up to
      // anything, but that should be rare.
      gpu_info->amd_switchable = true;
    } else {
      bool is_amd_switchable = false;
      uint32_t active_vendor = 0, active_device = 0;
      GetAMDSwitchableInfo(&is_amd_switchable, &active_vendor, &active_device);
      gpu_info->amd_switchable = is_amd_switchable;
    }
  }
  bool found = false;
  for (size_t i = 0; i < devices.size(); ++i) {
    const GPUInfo::GPUDevice& device = devices[i];
    if (i == primary_device) {
      found = true;
      gpu_info->gpu = device;
    } else {
      gpu_info->secondary_gpus.push_back(device);
    }
  }
  return found;
}

// DirectX 12 are included with Windows 10 and Server 2016.
void GetGpuSupportedD3D12Version(Dx12VulkanVersionInfo* info) {
  TRACE_EVENT0("gpu", "GetGpuSupportedD3D12Version");
  info->supports_dx12 = false;
  info->d3d12_feature_level = 0;

  base::NativeLibrary d3d12_library =
      base::LoadNativeLibrary(base::FilePath(L"d3d12.dll"), nullptr);
  if (!d3d12_library) {
    return;
  }

  // The order of feature levels to attempt to create in D3D CreateDevice
  const D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_12_1,
                                              D3D_FEATURE_LEVEL_12_0};

  PFN_D3D12_CREATE_DEVICE D3D12CreateDevice =
      reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
          GetProcAddress(d3d12_library, "D3D12CreateDevice"));
  if (D3D12CreateDevice) {
    // For the default adapter only. (*pAdapter == nullptr)
    // Check to see if the adapter supports Direct3D 12, but don't create the
    // actual device yet. (**ppDevice == nullptr)
    for (auto level : feature_levels) {
      if (SUCCEEDED(D3D12CreateDevice(nullptr, level, _uuidof(ID3D12Device),
                                      nullptr))) {
        info->d3d12_feature_level = level;
        info->supports_dx12 = true;
        break;
      }
    }
  }

  base::UnloadNativeLibrary(d3d12_library);
}

bool BadAMDVulkanDriverVersion() {
  // Both 32-bit and 64-bit dll are broken. If 64-bit doesn't exist,
  // 32-bit dll will be used to detect the AMD Vulkan driver.
  const base::FilePath kAmdDriver64(FILE_PATH_LITERAL("amdvlk64.dll"));
  const base::FilePath kAmdDriver32(FILE_PATH_LITERAL("amdvlk32.dll"));
  auto file_version_info =
      base::WrapUnique(FileVersionInfoWin::CreateFileVersionInfo(kAmdDriver64));
  if (!file_version_info) {
    file_version_info.reset(
        FileVersionInfoWin::CreateFileVersionInfo(kAmdDriver32));
    if (!file_version_info)
      return false;
  }

  const VS_FIXEDFILEINFO* fixed_file_info =
      static_cast<FileVersionInfoWin*>(file_version_info.get())
          ->fixed_file_info();
  const int major = HIWORD(fixed_file_info->dwFileVersionMS);
  const int minor = LOWORD(fixed_file_info->dwFileVersionMS);
  const int minor_1 = HIWORD(fixed_file_info->dwFileVersionLS);

  // From the Canary crash logs, the broken amdvlk64.dll versions
  // are 1.0.39.0, 1.0.51.0 and 1.0.54.0. In the manual test, version
  // 9.2.10.1 dated 12/6/2017 works and version 1.0.54.0 dated 11/2/1017
  // crashes. All version numbers small than 1.0.54.0 will be marked as
  // broken.
  if (major == 1 && minor == 0 && minor_1 <= 54) {
    return true;
  }
  return false;
}

bool BadVulkanDllVersion() {
  std::unique_ptr<FileVersionInfoWin> file_version_info(
      static_cast<FileVersionInfoWin*>(
          FileVersionInfoWin::CreateFileVersionInfo(
              base::FilePath(FILE_PATH_LITERAL("vulkan-1.dll")))));

  if (!file_version_info)
    return false;

  const VS_FIXEDFILEINFO* fixed_file_info =
      static_cast<FileVersionInfoWin*>(file_version_info.get())
          ->fixed_file_info();
  const int major = HIWORD(fixed_file_info->dwFileVersionMS);
  const int minor = LOWORD(fixed_file_info->dwFileVersionMS);
  const int build_1 = HIWORD(fixed_file_info->dwFileVersionLS);
  const int build_2 = LOWORD(fixed_file_info->dwFileVersionLS);

  // From the logs, most vulkan-1.dll crashs are from the following versions.
  // As of 7/23/2018.
  // 0.0.0.0 -  # of crashes: 6556
  // 1.0.26.0 - # of crashes: 5890
  // 1.0.33.0 - # of crashes: 12271
  // 1.0.42.0 - # of crashes: 35749
  // 1.0.42.1 - # of crashes: 68214
  // 1.0.51.0 - # of crashes: 5152
  // The GPU could be from any vendor, but only some certain models would crash.
  // For those that don't crash, they usually return failures upon GPU vulkan
  // support querying even though the GPU drivers can support it.
  if ((major == 0 && minor == 0 && build_1 == 0 && build_2 == 0) ||
      (major == 1 && minor == 0 && build_1 == 26 && build_2 == 0) ||
      (major == 1 && minor == 0 && build_1 == 33 && build_2 == 0) ||
      (major == 1 && minor == 0 && build_1 == 42 && build_2 == 0) ||
      (major == 1 && minor == 0 && build_1 == 42 && build_2 == 1) ||
      (major == 1 && minor == 0 && build_1 == 51 && build_2 == 0)) {
    return true;
  }
  return false;
}

bool InitVulkan(base::NativeLibrary* vulkan_library,
                PFN_vkGetInstanceProcAddr* vkGetInstanceProcAddr,
                PFN_vkCreateInstance* vkCreateInstance) {
  *vulkan_library =
      base::LoadNativeLibrary(base::FilePath(L"vulkan-1.dll"), nullptr);

  if (!(*vulkan_library)) {
    return false;
  }

  *vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      GetProcAddress(*vulkan_library, "vkGetInstanceProcAddr"));

  if (*vkGetInstanceProcAddr) {
    *vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        (*vkGetInstanceProcAddr)(nullptr, "vkCreateInstance"));
    if (*vkCreateInstance) {
      return true;
    }
  }
  base::UnloadNativeLibrary(*vulkan_library);
  return false;
}

bool InitVulkanInstanceProc(
    const VkInstance& vk_instance,
    const PFN_vkGetInstanceProcAddr& vkGetInstanceProcAddr,
    PFN_vkDestroyInstance* vkDestroyInstance,
    PFN_vkEnumeratePhysicalDevices* vkEnumeratePhysicalDevices,
    PFN_vkEnumerateDeviceExtensionProperties*
        vkEnumerateDeviceExtensionProperties) {
  *vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
      vkGetInstanceProcAddr(vk_instance, "vkDestroyInstance"));

  *vkEnumeratePhysicalDevices =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          vkGetInstanceProcAddr(vk_instance, "vkEnumeratePhysicalDevices"));

  *vkEnumerateDeviceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceExtensionProperties"));

  if ((*vkDestroyInstance) && (*vkEnumeratePhysicalDevices) &&
      (*vkEnumerateDeviceExtensionProperties)) {
    return true;
  }
  return false;
}

void GetGpuSupportedVulkanVersionAndExtensions(
    Dx12VulkanVersionInfo* info,
    const std::vector<const char*>& requested_vulkan_extensions,
    std::vector<bool>* extension_support) {
  TRACE_EVENT0("gpu", "GetGpuSupportedVulkanVersionAndExtensions");

  base::NativeLibrary vulkan_library;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
  PFN_vkDestroyInstance vkDestroyInstance;
  VkInstance vk_instance = VK_NULL_HANDLE;
  uint32_t physical_device_count = 0;
  info->supports_vulkan = false;
  info->vulkan_version = 0;

  // Skip if the system has an older AMD Vulkan driver amdvlk64.dll or
  // amdvlk32.dll which crashes when vkCreateInstance() is called. This bug has
  // been fixed in the latest AMD driver.
  if (BadAMDVulkanDriverVersion()) {
    return;
  }

  // Some early versions of vulkan-1.dll might crash
  if (BadVulkanDllVersion()) {
    return;
  }

  if (!InitVulkan(&vulkan_library, &vkGetInstanceProcAddr, &vkCreateInstance)) {
    return;
  }

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  // Get the Vulkan API version supported in the GPU driver
  for (int minor_version = 1; minor_version >= 0; --minor_version) {
    app_info.apiVersion = VK_MAKE_VERSION(1, minor_version, 0);
    VkResult result = vkCreateInstance(&create_info, nullptr, &vk_instance);
    if (result == VK_SUCCESS && vk_instance &&
        InitVulkanInstanceProc(vk_instance, vkGetInstanceProcAddr,
                               &vkDestroyInstance, &vkEnumeratePhysicalDevices,
                               &vkEnumerateDeviceExtensionProperties)) {
      result = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count,
                                          nullptr);
      if (result == VK_SUCCESS && physical_device_count > 0) {
        info->supports_vulkan = true;
        info->vulkan_version = app_info.apiVersion;
        break;
      } else {
        vkDestroyInstance(vk_instance, nullptr);
        vk_instance = VK_NULL_HANDLE;
      }
    }
  }

  // Check whether the requested_vulkan_extensions are supported
  if (info->supports_vulkan) {
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count,
                               physical_devices.data());

    // physical_devices[0]: Only query the default device for now
    uint32_t property_count;
    vkEnumerateDeviceExtensionProperties(physical_devices[0], nullptr,
                                         &property_count, nullptr);

    std::vector<VkExtensionProperties> extension_properties(property_count);
    if (property_count > 0) {
      vkEnumerateDeviceExtensionProperties(physical_devices[0], nullptr,
                                           &property_count,
                                           extension_properties.data());
    }

    for (size_t i = 0; i < requested_vulkan_extensions.size(); ++i) {
      for (size_t p = 0; p < property_count; ++p) {
        if (strcmp(requested_vulkan_extensions[i],
                   extension_properties[p].extensionName) == 0) {
          (*extension_support)[i] = true;
          break;
        }
      }
    }
  }

  if (vk_instance) {
    vkDestroyInstance(vk_instance, nullptr);
  }

  base::UnloadNativeLibrary(vulkan_library);
}

void RecordGpuSupportedRuntimeVersionHistograms(Dx12VulkanVersionInfo* info) {
  // D3D
  GetGpuSupportedD3D12Version(info);
  UMA_HISTOGRAM_BOOLEAN("GPU.SupportsDX12", info->supports_dx12);
  UMA_HISTOGRAM_ENUMERATION(
      "GPU.D3D12FeatureLevel",
      ConvertToHistogramFeatureLevel(info->d3d12_feature_level));

  // Vulkan
  const std::vector<const char*> vulkan_extensions = {
      "VK_KHR_external_memory_win32", "VK_KHR_external_semaphore_win32",
      "VK_KHR_win32_keyed_mutex"};
  std::vector<bool> extension_support(vulkan_extensions.size(), false);
  GetGpuSupportedVulkanVersionAndExtensions(info, vulkan_extensions,
                                            &extension_support);

  UMA_HISTOGRAM_BOOLEAN("GPU.SupportsVulkan", info->supports_vulkan);
  UMA_HISTOGRAM_ENUMERATION(
      "GPU.VulkanVersion",
      ConvertToHistogramVulkanVersion(info->vulkan_version));

  for (size_t i = 0; i < vulkan_extensions.size(); ++i) {
    std::string name = "GPU.VulkanExtSupport.";
    name.append(vulkan_extensions[i]);
    base::UmaHistogramBoolean(name, extension_support[i]);
  }
}

bool CollectContextGraphicsInfo(GPUInfo* gpu_info,
                                const GpuPreferences& gpu_preferences) {
  TRACE_EVENT0("gpu", "CollectGraphicsInfo");

  DCHECK(gpu_info);

  if (!CollectGraphicsInfoGL(gpu_info, gpu_preferences))
    return false;

  // ANGLE's renderer strings are of the form:
  // ANGLE (<adapter_identifier> Direct3D<version> vs_x_x ps_x_x)
  std::string direct3d_version;
  int vertex_shader_major_version = 0;
  int vertex_shader_minor_version = 0;
  int pixel_shader_major_version = 0;
  int pixel_shader_minor_version = 0;
  if (RE2::FullMatch(gpu_info->gl_renderer,
                     "ANGLE \\(.*\\)") &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " Direct3D(\\w+)",
                        &direct3d_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " vs_(\\d+)_(\\d+)",
                        &vertex_shader_major_version,
                        &vertex_shader_minor_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " ps_(\\d+)_(\\d+)",
                        &pixel_shader_major_version,
                        &pixel_shader_minor_version)) {
    gpu_info->vertex_shader_version =
        base::StringPrintf("%d.%d",
                           vertex_shader_major_version,
                           vertex_shader_minor_version);
    gpu_info->pixel_shader_version =
        base::StringPrintf("%d.%d",
                           pixel_shader_major_version,
                           pixel_shader_minor_version);

    DCHECK(!gpu_info->vertex_shader_version.empty());
    // Note: do not reorder, used by UMA_HISTOGRAM below
    enum ShaderModel {
      SHADER_MODEL_UNKNOWN,
      SHADER_MODEL_2_0,
      SHADER_MODEL_3_0,
      SHADER_MODEL_4_0,
      SHADER_MODEL_4_1,
      SHADER_MODEL_5_0,
      NUM_SHADER_MODELS
    };
    ShaderModel shader_model = SHADER_MODEL_UNKNOWN;
    if (gpu_info->vertex_shader_version == "5.0") {
      shader_model = SHADER_MODEL_5_0;
    } else if (gpu_info->vertex_shader_version == "4.1") {
      shader_model = SHADER_MODEL_4_1;
    } else if (gpu_info->vertex_shader_version == "4.0") {
      shader_model = SHADER_MODEL_4_0;
    } else if (gpu_info->vertex_shader_version == "3.0") {
      shader_model = SHADER_MODEL_3_0;
    } else if (gpu_info->vertex_shader_version == "2.0") {
      shader_model = SHADER_MODEL_2_0;
    }
    UMA_HISTOGRAM_ENUMERATION("GPU.D3DShaderModel", shader_model,
                              NUM_SHADER_MODELS);

    // DirectX diagnostics are collected asynchronously because it takes a
    // couple of seconds.
  }
  return true;
}

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectPreliminaryGraphicsInfo");

  DCHECK(gpu_info);

  // nvd3d9wrap.dll is loaded into all processes when Optimus is enabled.
  HMODULE nvd3d9wrap = GetModuleHandleW(L"nvd3d9wrap.dll");
  gpu_info->optimus = nvd3d9wrap != nullptr;

  // Taken from http://www.nvidia.com/object/device_ids.html
  DISPLAY_DEVICE dd;
  dd.cb = sizeof(DISPLAY_DEVICE);
  std::wstring id;
  for (int i = 0; EnumDisplayDevices(nullptr, i, &dd, 0); ++i) {
    if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      id = dd.DeviceID;
      break;
    }
  }

  if (id.length() <= 20) {
    // EnumDisplayDevices returns an empty id when called inside a remote
    // session (unless that session happens to be attached to the console). In
    // that case, we do not want to fail, as we should be able to grab the
    // device/vendor ids from the D3D context, below. Therefore, only fail if
    // the device string is not one of either the RDP mirror driver "RDPUDD
    // Chained DD" or the citrix display driver.
    if (wcscmp(dd.DeviceString, L"RDPUDD Chained DD") != 0 &&
        wcscmp(dd.DeviceString, L"Citrix Systems Inc. Display Driver") != 0) {
      // Set vendor_id/device_id for blacklisting purpose.
      gpu_info->gpu.vendor_id = 0xffff;
      gpu_info->gpu.device_id = 0xfffe;
      return false;
    }
  }

  DeviceIDToVendorAndDevice(id, &gpu_info->gpu.vendor_id,
                            &gpu_info->gpu.device_id);
  // TODO(zmo): we only need to call CollectDriverInfoD3D() if we use ANGLE.
  return CollectDriverInfoD3D(id, gpu_info);
}

}  // namespace gpu
