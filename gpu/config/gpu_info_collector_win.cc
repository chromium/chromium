// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

// C system before C++ system.
#include <DirectML.h>
#include <d3d11.h>
#include <d3d11_3.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <wrl/client.h>

#include "base/file_version_info_win.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_com_initializer.h"
#include "build/branding_buildflags.h"
#include "gpu/config/gpu_util.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"
#include "third_party/microsoft_dxheaders/src/include/directx/dxcore.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum D3D12FeatureLevel in
// \tools\metrics\histograms\enums.xml
enum class D3D12FeatureLevel {
  kD3DFeatureLevelUnknown = 0,
  kD3DFeatureLevel_12_0 = 1,
  kD3DFeatureLevel_12_1 = 2,
  kD3DFeatureLevel_11_0 = 3,
  kD3DFeatureLevel_11_1 = 4,
  kD3DFeatureLevel_12_2 = 5,
  kMaxValue = kD3DFeatureLevel_12_2,
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
    case D3D_FEATURE_LEVEL_12_2:
      return D3D12FeatureLevel::kD3DFeatureLevel_12_2;
    case D3D_FEATURE_LEVEL_11_0:
      return D3D12FeatureLevel::kD3DFeatureLevel_11_0;
    case D3D_FEATURE_LEVEL_11_1:
      return D3D12FeatureLevel::kD3DFeatureLevel_11_1;
    default:
      NOTREACHED_IN_MIGRATION();
      return D3D12FeatureLevel::kD3DFeatureLevelUnknown;
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class D3D12ShaderModel {
  kUnknownOrNoD3D12Devices = 0,
  kD3DShaderModel_5_1 = 1,
  kD3DShaderModel_6_0 = 2,
  kD3DShaderModel_6_1 = 3,
  kD3DShaderModel_6_2 = 4,
  kD3DShaderModel_6_3 = 5,
  kD3DShaderModel_6_4 = 6,
  kD3DShaderModel_6_5 = 7,
  kD3DShaderModel_6_6 = 8,
  kD3DShaderModel_6_7 = 9,
  kMaxValue = kD3DShaderModel_6_7,
};

D3D12ShaderModel ConvertToHistogramShaderVersion(uint32_t version) {
  switch (version) {
    case 0:
      return D3D12ShaderModel::kUnknownOrNoD3D12Devices;
    case D3D_SHADER_MODEL_5_1:
      return D3D12ShaderModel::kD3DShaderModel_5_1;
    case D3D_SHADER_MODEL_6_0:
      return D3D12ShaderModel::kD3DShaderModel_6_0;
    case D3D_SHADER_MODEL_6_1:
      return D3D12ShaderModel::kD3DShaderModel_6_1;
    case D3D_SHADER_MODEL_6_2:
      return D3D12ShaderModel::kD3DShaderModel_6_2;
    case D3D_SHADER_MODEL_6_3:
      return D3D12ShaderModel::kD3DShaderModel_6_3;
    case D3D_SHADER_MODEL_6_4:
      return D3D12ShaderModel::kD3DShaderModel_6_4;
    case D3D_SHADER_MODEL_6_5:
      return D3D12ShaderModel::kD3DShaderModel_6_5;
    case D3D_SHADER_MODEL_6_6:
      return D3D12ShaderModel::kD3DShaderModel_6_6;
    case D3D_SHADER_MODEL_6_7:
      return D3D12ShaderModel::kD3DShaderModel_6_7;

    default:
      NOTREACHED_IN_MIGRATION();
      return D3D12ShaderModel::kUnknownOrNoD3D12Devices;
  }
}

OverlaySupport FlagsToOverlaySupport(bool overlays_supported, UINT flags) {
  if (flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING)
    return OverlaySupport::kScaling;
  if (flags & DXGI_OVERLAY_SUPPORT_FLAG_DIRECT)
    return OverlaySupport::kDirect;
  if (overlays_supported)
    return OverlaySupport::kSoftware;

  return OverlaySupport::kNone;
}

bool GetActiveAdapterLuid(LUID* luid) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device)
    return false;

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device.As(&dxgi_device)))
    return false;

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  if (FAILED(dxgi_device->GetAdapter(&adapter)))
    return false;

  DXGI_ADAPTER_DESC desc;
  CHECK_EQ(S_OK, adapter->GetDesc(&desc));

  // Zero isn't a valid LUID.
  if (desc.AdapterLuid.HighPart == 0 && desc.AdapterLuid.LowPart == 0)
    return false;

  *luid = desc.AdapterLuid;
  return true;
}

}  // namespace

// This has to be called after a context is created, active GPU is identified,
// and GPU driver bug workarounds are computed again. Otherwise the workaround
// |disable_direct_composition| may not be correctly applied.
// Also, this has to be called after falling back to SwiftShader decision is
// finalized because this function depends on GL is ANGLE's GLES or not.
void CollectHardwareOverlayInfo(OverlayInfo* overlay_info) {
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
    overlay_info->direct_composition = gl::DirectCompositionSupported();
    overlay_info->supports_overlays = gl::DirectCompositionOverlaysSupported();
    overlay_info->nv12_overlay_support = FlagsToOverlaySupport(
        overlay_info->supports_overlays,
        gl::GetDirectCompositionOverlaySupportFlags(DXGI_FORMAT_NV12));
    overlay_info->yuy2_overlay_support = FlagsToOverlaySupport(
        overlay_info->supports_overlays,
        gl::GetDirectCompositionOverlaySupportFlags(DXGI_FORMAT_YUY2));
    overlay_info->bgra8_overlay_support =
        FlagsToOverlaySupport(overlay_info->supports_overlays,
                              gl::GetDirectCompositionOverlaySupportFlags(
                                  DXGI_FORMAT_B8G8R8A8_UNORM));
    overlay_info->rgb10a2_overlay_support =
        FlagsToOverlaySupport(overlay_info->supports_overlays,
                              gl::GetDirectCompositionOverlaySupportFlags(
                                  DXGI_FORMAT_R10G10B10A2_UNORM));
    overlay_info->p010_overlay_support = FlagsToOverlaySupport(
        overlay_info->supports_overlays,
        gl::GetDirectCompositionOverlaySupportFlags(DXGI_FORMAT_P010));
  }
}

std::string DriverVersionToString(LARGE_INTEGER driver_version) {
  return base::StringPrintf("%d.%d.%d.%d", HIWORD(driver_version.HighPart),
                            LOWORD(driver_version.HighPart),
                            HIWORD(driver_version.LowPart),
                            LOWORD(driver_version.LowPart));
}

void CollectNPUInformation(GPUInfo* gpu_info) {
  // Enumerate all dxcore adapters to retrieve the NPUs.
  base::ScopedNativeLibrary dxcore_library(
      base::LoadSystemLibrary(L"DXCore.dll"));
  if (!dxcore_library.is_valid()) {
    return;
  }
  using DXCoreCreateAdapterFactoryProc =
      decltype(static_cast<STDMETHODIMP (*)(REFIID, void**)>(
          DXCoreCreateAdapterFactory));
  DXCoreCreateAdapterFactoryProc dxcore_create_adapter_factory_proc =
      reinterpret_cast<DXCoreCreateAdapterFactoryProc>(
          dxcore_library.GetFunctionPointer("DXCoreCreateAdapterFactory"));
  if (!dxcore_create_adapter_factory_proc) {
    return;
  }
  Microsoft::WRL::ComPtr<IDXCoreAdapterFactory> dxcore_factory;
  HRESULT hr =
      dxcore_create_adapter_factory_proc(IID_PPV_ARGS(&dxcore_factory));
  if (FAILED(hr)) {
    return;
  }
  // First query for NPU devices that satisfy the generic machine learning
  // property. Note this must be done as a separate query from core compute
  // because `CreateAdapterList()` returns the logical intersection of all
  // filter properties, not union.
  const std::array<GUID, 1> dx_guids_generic_ml = {
      DXCORE_ADAPTER_ATTRIBUTE_D3D12_GENERIC_ML};
  Microsoft::WRL::ComPtr<IDXCoreAdapterList> adapter_list;
  hr = dxcore_factory->CreateAdapterList(dx_guids_generic_ml.size(),
                                         dx_guids_generic_ml.data(),
                                         IID_PPV_ARGS(&adapter_list));
  if (FAILED(hr)) {
    return;
  }
  uint32_t adapter_count = adapter_list->GetAdapterCount();

  // If no generic ML devices were found, then retry with the core compute
  // filter, getting an adapter list that only contains core-compute capable
  // devices.
  if (adapter_count == 0) {
    adapter_list.Reset();

    const std::array<GUID, 1> dx_guids_core_compute = {
        DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE};
    hr = dxcore_factory->CreateAdapterList(dx_guids_core_compute.size(),
                                           dx_guids_core_compute.data(),
                                           IID_PPV_ARGS(&adapter_list));
    if (FAILED(hr)) {
      return;
    }
    adapter_count = adapter_list->GetAdapterCount();
  }
  for (uint32_t adapter_index = 0; adapter_index < adapter_count;
       ++adapter_index) {
    Microsoft::WRL::ComPtr<IDXCoreAdapter> dxcore_adapter;
    hr = adapter_list->GetAdapter(adapter_index, IID_PPV_ARGS(&dxcore_adapter));
    if (FAILED(hr)) {
      return;
    }
    // Because GPUs usually also have the core-compute capability, then we need
    // to filter out the GPUs with `DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS` or
    // `DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS` attribute to
    // get the NPUs.
    bool is_hardware;
    if (SUCCEEDED(dxcore_adapter->GetProperty(DXCoreAdapterProperty::IsHardware,
                                              &is_hardware)) &&
        is_hardware &&
        !dxcore_adapter->IsAttributeSupported(
            DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS) &&
        !dxcore_adapter->IsAttributeSupported(
            DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS)) {
      GPUInfo::GPUDevice device;
      DXCoreHardwareID hardware_id;
      if (SUCCEEDED(dxcore_adapter->GetProperty(
              DXCoreAdapterProperty::HardwareID, &hardware_id))) {
        device.vendor_id = hardware_id.vendorID;
        device.device_id = hardware_id.deviceID;
      }
      uint64_t raw_driver_version;
      if (SUCCEEDED(dxcore_adapter->GetProperty(
              DXCoreAdapterProperty::DriverVersion, &raw_driver_version))) {
        LARGE_INTEGER driver_version;
        driver_version.QuadPart = static_cast<LONGLONG>(raw_driver_version);
        device.driver_version = DriverVersionToString(driver_version);
      }
      LUID instance_luid;
      if (SUCCEEDED(dxcore_adapter->GetProperty(
              DXCoreAdapterProperty::InstanceLuid, &instance_luid))) {
        device.luid =
            CHROME_LUID{instance_luid.LowPart, instance_luid.HighPart};
      }
      gpu_info->npus.push_back(device);
    }
  }
}

bool CollectDriverInfoD3D(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectDriverInfoD3D");

  CollectNPUInformation(gpu_info);

  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  HRESULT hr = ::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
  if (FAILED(hr))
    return false;

  UINT i;
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  for (i = 0; SUCCEEDED(dxgi_factory->EnumAdapters(i, &dxgi_adapter)); i++) {
    DXGI_ADAPTER_DESC desc;
    CHECK_EQ(S_OK, dxgi_adapter->GetDesc(&desc));

    GPUInfo::GPUDevice device;
    device.vendor_id = desc.VendorId;
    device.device_id = desc.DeviceId;
    device.sub_sys_id = desc.SubSysId;
    device.revision = desc.Revision;
    device.luid =
        CHROME_LUID{desc.AdapterLuid.LowPart, desc.AdapterLuid.HighPart};

    LARGE_INTEGER umd_version;
    hr = dxgi_adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),
                                             &umd_version);
    if (SUCCEEDED(hr)) {
      device.driver_version = DriverVersionToString(umd_version);
    } else {
      DLOG(ERROR) << "Unable to retrieve the umd version of adapter: "
                  << desc.Description << " HR: " << std::hex << hr;
    }
    if (i == 0) {
      gpu_info->gpu = device;
    } else {
      gpu_info->secondary_gpus.push_back(device);
    }
  }

  Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi_factory6;
  if (gpu_info->GpuCount() > 1 && SUCCEEDED(dxgi_factory.As(&dxgi_factory6))) {
    if (SUCCEEDED(dxgi_factory6->EnumAdapterByGpuPreference(
            0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&dxgi_adapter)))) {
      DXGI_ADAPTER_DESC desc;
      CHECK_EQ(S_OK, dxgi_adapter->GetDesc(&desc));
      GPUInfo::GPUDevice* device = gpu_info->FindGpuByLuid(
          desc.AdapterLuid.LowPart, desc.AdapterLuid.HighPart);
      DCHECK(device);
      device->gpu_preference = gl::GpuPreference::kHighPerformance;
    }
    if (SUCCEEDED(dxgi_factory6->EnumAdapterByGpuPreference(
            0, DXGI_GPU_PREFERENCE_MINIMUM_POWER,
            IID_PPV_ARGS(&dxgi_adapter)))) {
      DXGI_ADAPTER_DESC desc;
      CHECK_EQ(S_OK, dxgi_adapter->GetDesc(&desc));
      GPUInfo::GPUDevice* device = gpu_info->FindGpuByLuid(
          desc.AdapterLuid.LowPart, desc.AdapterLuid.HighPart);
      DCHECK(device);
      device->gpu_preference = gl::GpuPreference::kLowPower;
    }
  }

  return i > 0;
}

// CanCreateD3D12Device returns true/false depending on whether D3D12 device
// creation should be attempted on the passed in adapter. Returns false if there
// are known driver bugs.
bool CanCreateD3D12Device(IDXGIAdapter* dxgi_adapter) {
  DXGI_ADAPTER_DESC desc;
  CHECK_EQ(S_OK, dxgi_adapter->GetDesc(&desc));

  // Known driver bugs are Intel-only. Expand in the future, as necessary, for
  // other IHVs.
  if (desc.VendorId != 0x8086)
    return true;

  LARGE_INTEGER umd_version;
  HRESULT hr =
      dxgi_adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd_version);
  if (FAILED(hr)) {
    return false;
  }

  // On certain Intel drivers, the driver will crash if you call
  // D3D12CreateDevice and the command line of the process is greater than 1024
  // bytes. 100.9416 is the first driver to introduce the bug, while 100.9664 is
  // the first driver to fix it.
  if (HIWORD(umd_version.LowPart) == 100 &&
      LOWORD(umd_version.LowPart) >= 9416 &&
      LOWORD(umd_version.LowPart) < 9664) {
    const char* command_line = GetCommandLineA();
    const size_t command_line_length = strlen(command_line);
    // Check for 1023 since strlen doesn't include the null terminator.
    if (command_line_length > 1023) {
      return false;
    }
  }

  return true;
}

// DirectX 12 are included with Windows 10 and Server 2016.
void GetGpuSupportedDirectXVersion(uint32_t& d3d12_feature_level,
                                   uint32_t& highest_shader_model_version,
                                   uint32_t& directml_feature_level) {
  TRACE_EVENT0("gpu", "GetGpuSupportedDirectXVersion");

  // Initialize to 0 to indicated an unknown type in UMA.
  d3d12_feature_level = 0;
  highest_shader_model_version = 0;
  directml_feature_level = 0;

  base::ScopedNativeLibrary d3d12_library(
      base::FilePath(FILE_PATH_LITERAL("d3d12.dll")));
  if (!d3d12_library.is_valid())
    return;

  // The order of feature levels to attempt to create in D3D CreateDevice
  const D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

  PFN_D3D12_CREATE_DEVICE d3d12_create_device_proc =
      reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
          d3d12_library.GetFunctionPointer("D3D12CreateDevice"));
  Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device;
  if (d3d12_create_device_proc) {
    Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
    HRESULT hr = ::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
    if (FAILED(hr)) {
      return;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    hr = dxgi_factory->EnumAdapters(0, &dxgi_adapter);
    if (FAILED(hr)) {
      return;
    }

    if (!CanCreateD3D12Device(dxgi_adapter.Get())) {
      return;
    }

    // For the default adapter only: EnumAdapters(0, ...).
    // Check to see if the adapter supports Direct3D 12.
    for (auto level : feature_levels) {
      if (SUCCEEDED(d3d12_create_device_proc(dxgi_adapter.Get(), level,
                                             _uuidof(ID3D12Device),
                                             &d3d12_device))) {
        d3d12_feature_level = level;
        break;
      }
    }

    // Query the maximum supported shader model version.
    if (d3d12_device) {
      // As per the documentation, CheckFeatureSupport will return E_INVALIDARG
      // if the shader model is not known by the current runtime, so we loop in
      // decreasing shader model version to determine the highest supported
      // model:
      // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_shader_model.
      const D3D_SHADER_MODEL shader_models[] = {
          D3D_SHADER_MODEL_6_7, D3D_SHADER_MODEL_6_6, D3D_SHADER_MODEL_6_5,
          D3D_SHADER_MODEL_6_4, D3D_SHADER_MODEL_6_3, D3D_SHADER_MODEL_6_2,
          D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0, D3D_SHADER_MODEL_5_1,
      };

      for (auto model : shader_models) {
        D3D12_FEATURE_DATA_SHADER_MODEL shader_model_data = {};
        shader_model_data.HighestShaderModel = model;
        if (SUCCEEDED(d3d12_device->CheckFeatureSupport(
                D3D12_FEATURE_SHADER_MODEL, &shader_model_data,
                sizeof(shader_model_data)))) {
          highest_shader_model_version = shader_model_data.HighestShaderModel;
          break;
        }
      }
      // DirectML is supported starting on D3D12.
      base::ScopedNativeLibrary dml_library(
          base::ScopedNativeLibrary(base::LoadSystemLibrary(L"directml.dll")));
      if (!dml_library.is_valid()) {
        return;
      }
      // On older versions of windows DMLCreateDevice accepts a different
      // number of parameters. We should use DMLCreateDevice1 which always
      // takes the same number of parameters to ensure consistency
      // among all versions of windows.
      auto dml_create_device1_proc =
          reinterpret_cast<decltype(DMLCreateDevice1)*>(
              dml_library.GetFunctionPointer("DMLCreateDevice1"));
      if (!dml_create_device1_proc) {
        return;
      }
      Microsoft::WRL::ComPtr<IDMLDevice> dml_device;
      if (FAILED(dml_create_device1_proc(
              d3d12_device.Get(), DML_CREATE_DEVICE_FLAG_NONE,
              DML_FEATURE_LEVEL_1_0, IID_PPV_ARGS(&dml_device)))) {
        return;
      }

      DML_FEATURE_LEVEL feature_levels_requested[] = {
          DML_FEATURE_LEVEL_1_0, DML_FEATURE_LEVEL_2_0, DML_FEATURE_LEVEL_2_1,
          DML_FEATURE_LEVEL_3_0, DML_FEATURE_LEVEL_3_1, DML_FEATURE_LEVEL_4_0,
          DML_FEATURE_LEVEL_4_1, DML_FEATURE_LEVEL_5_0};

      DML_FEATURE_QUERY_FEATURE_LEVELS feature_levels_query = {
          std::size(feature_levels_requested), feature_levels_requested};

      DML_FEATURE_DATA_FEATURE_LEVELS feature_levels_supported = {};
      if (SUCCEEDED(dml_device->CheckFeatureSupport(
              DML_FEATURE_FEATURE_LEVELS, sizeof(feature_levels_query),
              &feature_levels_query, sizeof(feature_levels_supported),
              &feature_levels_supported))) {
        directml_feature_level =
            feature_levels_supported.MaxSupportedFeatureLevel;
      }
    }
  }
}

// The old graphics drivers are installed to the Windows system directory
// c:\windows\system32 or SysWOW64. Those versions can be detected without
// specifying the absolute directory. For a newer version (>= ~2018), this won't
// work. The newer graphics drivers are located in
// c:\windows\system32\DriverStore\FileRepository\xxx.infxxx which contains a
// different number at each installation
bool BadAMDVulkanDriverVersion() {
  // Both 32-bit and 64-bit dll are broken. If 64-bit doesn't exist,
  // 32-bit dll will be used to detect the AMD Vulkan driver.
  const base::FilePath kAmdDriver64(FILE_PATH_LITERAL("amdvlk64.dll"));
  const base::FilePath kAmdDriver32(FILE_PATH_LITERAL("amdvlk32.dll"));
  std::unique_ptr<FileVersionInfoWin> file_version_info =
      FileVersionInfoWin::CreateFileVersionInfoWin(kAmdDriver64);
  if (!file_version_info) {
    file_version_info =
        FileVersionInfoWin::CreateFileVersionInfoWin(kAmdDriver32);
    if (!file_version_info)
      return false;
  }
  base::Version amd_version = file_version_info->GetFileVersion();

  // From the Canary crash logs, the broken amdvlk64.dll versions
  // are 1.0.39.0, 1.0.51.0 and 1.0.54.0. In the manual test, version
  // 9.2.10.1 dated 12/6/2017 works and version 1.0.54.0 dated 11/2/1017
  // crashes. All version numbers small than 1.0.54.0 will be marked as
  // broken.
  const base::Version kBadAMDVulkanDriverVersion("1.0.54.0");
  // CompareTo() returns -1, 0, 1 for <, ==, >.
  if (amd_version.CompareTo(kBadAMDVulkanDriverVersion) != 1)
    return true;

  return false;
}

// Vulkan 1.1 was released by the Khronos Group on March 7, 2018.
// Blocklist all driver versions without Vulkan 1.1 support and those that cause
// lots of crashes.
bool BadGraphicsDriverVersions(const gpu::GPUInfo::GPUDevice& gpu_device) {
  // GPU Device info is not available in gpu_integration_test.info-collection
  // with --no-delay-for-dx12-vulkan-info-collection.
  if (gpu_device.driver_version.empty())
    return false;

  base::Version driver_version(gpu_device.driver_version);
  if (!driver_version.IsValid())
    return true;

  // AMD Vulkan drivers - amdvlk64.dll
  constexpr uint32_t kAMDVendorId = 0x1002;
  if (gpu_device.vendor_id == kAMDVendorId) {
    // 26.20.12028.2 (2019)- number of crashes 1,188,048 as of 5/14/2020.
    // Returns -1, 0, 1 for <, ==, >.
    if (driver_version.CompareTo(base::Version("26.20.12028.2")) == 0)
      return true;
  }

  return false;
}

bool InitVulkan(base::NativeLibrary* vulkan_library,
                PFN_vkGetInstanceProcAddr* vkGetInstanceProcAddr,
                PFN_vkCreateInstance* vkCreateInstance,
                uint32_t* vulkan_version) {
  *vulkan_version = 0;

  *vulkan_library =
      base::LoadNativeLibrary(base::FilePath(L"vulkan-1.dll"), nullptr);

  if (!(*vulkan_library)) {
    return false;
  }

  *vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(*vulkan_library,
                                                "vkGetInstanceProcAddr"));

  if (*vkGetInstanceProcAddr) {
    *vulkan_version = VK_MAKE_VERSION(1, 0, 0);
    PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
    vkEnumerateInstanceVersion =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            (*vkGetInstanceProcAddr)(nullptr, "vkEnumerateInstanceVersion"));

    // If the vkGetInstanceProcAddr returns nullptr for
    // vkEnumerateInstanceVersion, it is a Vulkan 1.0 implementation.
    if (!vkEnumerateInstanceVersion) {
      return false;
    }

    // Return value can be VK_SUCCESS or VK_ERROR_OUT_OF_HOST_MEMORY.
    if (vkEnumerateInstanceVersion(vulkan_version) != VK_SUCCESS) {
      return false;
    }

    // The minimum version required for Vulkan to be enabled is 1.1.0.
    // No further queries will be called for early versions. They are unstable
    // and might cause crashes.
    if (*vulkan_version < VK_MAKE_VERSION(1, 1, 0)) {
      return false;
    }

    *vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        (*vkGetInstanceProcAddr)(nullptr, "vkCreateInstance"));

    if (*vkCreateInstance)
      return true;
  }

  // From the crash reports, unloading the library here might cause a crash in
  // the Vulkan loader or in the Vulkan driver. To work around it, don't
  // explicitly unload the DLL. Instead, GPU process shutdown will unload all
  // loaded DLLs.
  // base::UnloadNativeLibrary(*vulkan_library);
  return false;
}

bool InitVulkanInstanceProc(
    const VkInstance& vk_instance,
    const PFN_vkGetInstanceProcAddr& vkGetInstanceProcAddr,
    PFN_vkEnumeratePhysicalDevices* vkEnumeratePhysicalDevices,
    PFN_vkEnumerateDeviceExtensionProperties*
        vkEnumerateDeviceExtensionProperties) {
  *vkEnumeratePhysicalDevices =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          vkGetInstanceProcAddr(vk_instance, "vkEnumeratePhysicalDevices"));

  *vkEnumerateDeviceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceExtensionProperties"));

  if ((*vkEnumeratePhysicalDevices) &&
      (*vkEnumerateDeviceExtensionProperties)) {
    return true;
  }
  return false;
}

uint32_t GetGpuSupportedVulkanVersion(
    const gpu::GPUInfo::GPUDevice& gpu_device) {
  TRACE_EVENT0("gpu", "GetGpuSupportedVulkanVersion");

  base::NativeLibrary vulkan_library;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
  VkInstance vk_instance = VK_NULL_HANDLE;
  uint32_t physical_device_count = 0;

  // Skip if the system has an older AMD Vulkan driver amdvlk64.dll or
  // amdvlk32.dll which crashes when vkCreateInstance() is called. This bug has
  // been fixed in the latest AMD driver.
  // Detected by the file version of amdvlk64.dll.
  if (BadAMDVulkanDriverVersion()) {
    return 0;
  }

  // Don't collect any info if the graphics vulkan driver is blocklisted or
  // doesn't support Vulkan 1.1
  // Detected by the graphic driver version returned by DXGI
  if (BadGraphicsDriverVersions(gpu_device))
    return 0;

  // Only supports a version >= 1.1.0.
  uint32_t vulkan_version = 0;
  if (!InitVulkan(&vulkan_library, &vkGetInstanceProcAddr, &vkCreateInstance,
                  &vulkan_version)) {
    return 0;
  }

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

  const std::vector<const char*> enabled_instance_extensions = {
      "VK_KHR_surface", "VK_KHR_win32_surface"};

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = enabled_instance_extensions.size();
  create_info.ppEnabledExtensionNames = enabled_instance_extensions.data();

  // Get the Vulkan API version supported in the GPU driver
  int highest_minor_version = VK_VERSION_MINOR(vulkan_version);
  for (int minor_version = highest_minor_version; minor_version >= 1;
       --minor_version) {
    app_info.apiVersion = VK_MAKE_VERSION(1, minor_version, 0);
    VkResult result = vkCreateInstance(&create_info, nullptr, &vk_instance);
    if (result == VK_SUCCESS && vk_instance &&
        InitVulkanInstanceProc(vk_instance, vkGetInstanceProcAddr,
                               &vkEnumeratePhysicalDevices,
                               &vkEnumerateDeviceExtensionProperties)) {
      result = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count,
                                          nullptr);
      if (result == VK_SUCCESS && physical_device_count > 0) {
        return app_info.apiVersion;
      } else {
        // Skip destroy here. GPU process shutdown will unload all loaded DLLs.
        // vkDestroyInstance(vk_instance, nullptr);
        vk_instance = VK_NULL_HANDLE;
      }
    }
  }

  // From the crash reports, calling the following two functions might cause a
  // crash in the Vulkan loader or in the Vulkan driver. To work around it,
  // don't explicitly unload the DLL. Instead, GPU process shutdown will unload
  // all loaded DLLs.
  // if (vk_instance) {
  //   vkDestroyInstance(vk_instance, nullptr);
  // }
  // base::UnloadNativeLibrary(vulkan_library);
  return 0;
}

void RecordGpuSupportedDx12VersionHistograms(
    uint32_t d3d12_feature_level,
    uint32_t highest_shader_model_version) {
  UMA_HISTOGRAM_ENUMERATION(
      "GPU.D3D12FeatureLevel",
      ConvertToHistogramFeatureLevel(d3d12_feature_level));

  UMA_HISTOGRAM_ENUMERATION(
      "GPU.D3D12HighestShaderModel2",
      ConvertToHistogramShaderVersion(highest_shader_model_version));
}

bool CollectD3D11FeatureInfo(D3D_FEATURE_LEVEL* d3d11_feature_level,
                             bool* has_discrete_gpu) {
  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  if (FAILED(::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory))))
    return false;

  base::ScopedNativeLibrary d3d11_library(
      base::FilePath(FILE_PATH_LITERAL("d3d11.dll")));
  if (!d3d11_library.is_valid())
    return false;
  PFN_D3D11_CREATE_DEVICE D3D11CreateDevice =
      reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
          d3d11_library.GetFunctionPointer("D3D11CreateDevice"));
  if (!D3D11CreateDevice)
    return false;

  // The order of feature levels to attempt to create in D3D CreateDevice.
  // TODO(crbug.com/40831714): Using 12_2 in kFeatureLevels[] will cause failure
  // in D3D11CreateDevice(). Limit the highest feature to 12_1.
  const D3D_FEATURE_LEVEL kFeatureLevels[] = {
      D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
      D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1};

  bool detected_discrete_gpu = false;
  D3D_FEATURE_LEVEL max_level = D3D_FEATURE_LEVEL_1_0_CORE;
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  for (UINT ii = 0; SUCCEEDED(dxgi_factory->EnumAdapters(ii, &dxgi_adapter));
       ++ii) {
    DXGI_ADAPTER_DESC desc;
    CHECK_EQ(S_OK, dxgi_adapter->GetDesc(&desc));
    if (desc.VendorId == 0x1414) {
      // Bypass Microsoft software renderer.
      continue;
    }
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    D3D_FEATURE_LEVEL returned_feature_level = D3D_FEATURE_LEVEL_1_0_CORE;
    if (FAILED(D3D11CreateDevice(dxgi_adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN,
                                 /*Software=*/0,
                                 /*Flags=*/0, kFeatureLevels,
                                 _countof(kFeatureLevels), D3D11_SDK_VERSION,
                                 &d3d11_device, &returned_feature_level,
                                 /*ppImmediateContext=*/nullptr))) {
      continue;
    }
    if (returned_feature_level > max_level)
      max_level = returned_feature_level;
    Microsoft::WRL::ComPtr<ID3D11Device3> d3d11_device_3;
    if (FAILED(d3d11_device.As(&d3d11_device_3)))
      continue;
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 data = {};
    if (FAILED(d3d11_device_3->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2,
                                                   &data, sizeof(data)))) {
      continue;
    }
    if (!data.UnifiedMemoryArchitecture)
      detected_discrete_gpu = true;
  }

  if (max_level > D3D_FEATURE_LEVEL_1_0_CORE) {
    *d3d11_feature_level = max_level;
    *has_discrete_gpu = detected_discrete_gpu;
    return true;
  }
  return false;
}

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectGraphicsInfo");

  DCHECK(gpu_info);

  if (!CollectGraphicsInfoGL(gpu_info, gl::GetDefaultDisplayEGL())) {
    return false;
  }

  // ANGLE's renderer strings are of the form:
  // ANGLE (<adapter_identifier> Direct3D<version> vs_x_x ps_x_x)
  std::string direct3d_version;
  int vertex_shader_major_version = 0;
  int vertex_shader_minor_version = 0;
  int pixel_shader_major_version = 0;
  int pixel_shader_minor_version = 0;
  if (RE2::FullMatch(gpu_info->gl_renderer, "ANGLE \\(.*\\)") &&
      RE2::PartialMatch(gpu_info->gl_renderer, " Direct3D(\\w+)",
                        &direct3d_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer, " vs_(\\d+)_(\\d+)",
                        &vertex_shader_major_version,
                        &vertex_shader_minor_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer, " ps_(\\d+)_(\\d+)",
                        &pixel_shader_major_version,
                        &pixel_shader_minor_version)) {
    gpu_info->vertex_shader_version = base::StringPrintf(
        "%d.%d", vertex_shader_major_version, vertex_shader_minor_version);
    gpu_info->pixel_shader_version = base::StringPrintf(
        "%d.%d", pixel_shader_major_version, pixel_shader_minor_version);

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
  // TODO(zmo): we only need to call CollectDriverInfoD3D() if we use ANGLE.
  return CollectDriverInfoD3D(gpu_info);
}

bool IdentifyActiveGPUWithLuid(GPUInfo* gpu_info) {
  LUID luid;
  if (!GetActiveAdapterLuid(&luid))
    return false;

  gpu_info->gpu.active = false;
  for (size_t i = 0; i < gpu_info->secondary_gpus.size(); i++)
    gpu_info->secondary_gpus[i].active = false;

  if (gpu_info->gpu.luid.HighPart == luid.HighPart &&
      gpu_info->gpu.luid.LowPart == luid.LowPart) {
    gpu_info->gpu.active = true;
    return true;
  }

  for (size_t i = 0; i < gpu_info->secondary_gpus.size(); i++) {
    if (gpu_info->secondary_gpus[i].luid.HighPart == luid.HighPart &&
        gpu_info->secondary_gpus[i].luid.LowPart == luid.LowPart) {
      gpu_info->secondary_gpus[i].active = true;
      return true;
    }
  }

  return false;
}

}  // namespace gpu
