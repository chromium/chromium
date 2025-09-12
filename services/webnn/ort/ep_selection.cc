// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ep_selection.h"

#include <algorithm>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/cstring_view.h"
#include "services/webnn/ort/platform_functions_ort.h"

namespace webnn::ort {

namespace {

constexpr base::cstring_view kCpuExecutionProvider = "CPUExecutionProvider";
constexpr base::cstring_view kDmlExecutionProvider = "DmlExecutionProvider";
constexpr base::cstring_view kWebGpuExecutionProvider =
    "WebGpuExecutionProvider";

bool IsDefaultCpuEpDevice(const OrtEpDevice* device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  return UNSAFE_BUFFERS(base::cstring_view(ort_api->EpDevice_EpName(device))) ==
         kCpuExecutionProvider;
}

bool MatchesEpVendor(const OrtEpDevice* ep_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  // Map hardware device vendor ID to EP vendor name.
  // TODO(crbug.com/444048012): Map other hardware device vendor IDs to EP
  // vendor names for EP selection.
  constexpr auto kDeviceVendorIdToEpVendorName =
      base::MakeFixedFlatMap<uint32_t, base::cstring_view>({
          {0x8086, "Intel"},  // Intel Corporation
      });

  uint32_t hardware_device_vendor_id =
      ort_api->HardwareDevice_VendorId(ort_api->EpDevice_Device(ep_device));
  auto vendor_it =
      kDeviceVendorIdToEpVendorName.find(hardware_device_vendor_id);
  if (vendor_it == kDeviceVendorIdToEpVendorName.end()) {
    // Unknown vendor ID, no matching possibility.
    return false;
  }

  // Match EP vendor names.
  return vendor_it->second == UNSAFE_BUFFERS(base::cstring_view(
                                  ort_api->EpDevice_EpVendor(ep_device)));
}

bool IsDiscreteGpu(const OrtEpDevice* device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  const OrtHardwareDevice* hardware_device = ort_api->EpDevice_Device(device);
  if (ort_api->HardwareDevice_Type(hardware_device) !=
      OrtHardwareDeviceType_GPU) {
    return false;
  }

  const OrtKeyValuePairs* device_metadata =
      ort_api->HardwareDevice_Metadata(hardware_device);

  size_t num_entries = 0;
  const char* const* keys = nullptr;
  const char* const* values = nullptr;
  ort_api->GetKeyValuePairs(device_metadata, &keys, &values, &num_entries);

  for (size_t i = 0; i < num_entries; ++i) {
    // SAFETY: ORT guarantees that `keys[i]` is valid and null-terminated.
    base::cstring_view key = UNSAFE_BUFFERS(base::cstring_view(keys[i]));
    if (key == "Discrete") {
      // SAFETY: ORT guarantees that `values[i]` is valid and null-terminated.
      base::cstring_view value = UNSAFE_BUFFERS(base::cstring_view(values[i]));
      return value == "1";
    }
  }

  return false;
}

// Select the first device of specified hardware device type from the sorted
// devices. Return nullptr if no such device is found.
// This behavior mimics the selection logic in ORT's provider_policy_context.cc:
// https://github.com/microsoft/onnxruntime/blob/9d650a4b2348d737407f9dbbf4f0cfd3789723c3/onnxruntime/core/session/provider_policy_context.cc#L402-L444
const OrtEpDevice* SelectFirstEpDeviceForDeviceType(
    base::span<const OrtEpDevice* const> sorted_devices,
    OrtHardwareDeviceType device_type) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  auto first_device = std::ranges::find_if(
      sorted_devices, [ort_api, device_type](const OrtEpDevice* device) {
        return ort_api->HardwareDevice_Type(ort_api->EpDevice_Device(device)) ==
               device_type;
      });

  if (first_device != sorted_devices.end()) {
    return *first_device;
  }

  return nullptr;
}

// Select the first CPU device and also append the default CPU EP device if the
// selected device is not the default one.
std::vector<const OrtEpDevice*> SelectEpDevicesForCpu(
    base::span<const OrtEpDevice* const> sorted_devices) {
  std::vector<const OrtEpDevice*> selected_devices;

  const OrtEpDevice* first_cpu = SelectFirstEpDeviceForDeviceType(
      sorted_devices, OrtHardwareDeviceType_CPU);

  // Handle the rare case where no CPU EP device is available.
  if (!first_cpu) {
    LOG(ERROR) << "[WebNN] No CPU execution provider available.";
    return selected_devices;
  }

  selected_devices.push_back(first_cpu);

  // Add the default CPU EP device to ensure maximum coverage of opsets and
  // operators.
  if (!IsDefaultCpuEpDevice(first_cpu) &&
      IsDefaultCpuEpDevice(sorted_devices.back())) {
    selected_devices.push_back(sorted_devices.back());
  }

  return selected_devices;
}

// Select the first GPU device with CPU fallback.
std::vector<const OrtEpDevice*> SelectEpDevicesForGpu(
    base::span<const OrtEpDevice* const> sorted_devices) {
  std::vector<const OrtEpDevice*> selected_devices;

  const OrtEpDevice* first_gpu = SelectFirstEpDeviceForDeviceType(
      sorted_devices, OrtHardwareDeviceType_GPU);

  if (first_gpu) {
    selected_devices.push_back(first_gpu);
  }

  std::vector<const OrtEpDevice*> cpu_fallback_devices =
      SelectEpDevicesForCpu(sorted_devices);
  selected_devices.insert(selected_devices.end(), cpu_fallback_devices.begin(),
                          cpu_fallback_devices.end());

  return selected_devices;
}

// Select the first NPU device with CPU fallback. If no NPU device is selected,
// delegate to GPU device selection logic which selects the first GPU device
// with CPU fallback.
std::vector<const OrtEpDevice*> SelectEpDevicesForNpu(
    base::span<const OrtEpDevice* const> sorted_devices) {
  const OrtEpDevice* first_npu = SelectFirstEpDeviceForDeviceType(
      sorted_devices, OrtHardwareDeviceType_NPU);

  if (!first_npu) {
    return SelectEpDevicesForGpu(sorted_devices);
  }

  std::vector<const OrtEpDevice*> selected_devices;
  selected_devices.push_back(first_npu);

  std::vector<const OrtEpDevice*> cpu_fallback_devices =
      SelectEpDevicesForCpu(sorted_devices);
  selected_devices.insert(selected_devices.end(), cpu_fallback_devices.begin(),
                          cpu_fallback_devices.end());

  return selected_devices;
}

// Sort EP devices in the following order:
// 1. Device type priority: NPU > GPU > CPU.
// 2. For both GPU devices: Discrete > Integrated.
// 3. EP vendor matching preference.
// 4. Sort by EP name:
//    a. WebGPU EP > DML EP
//    b. Arbitrarily sort for tie-breaking.
// 5. Default CPU EP placed last.
//
// The sorting logic closely mimics ORT's approach, but prioritizes the WebGPU
// EP over the DML EP specifically for GPU devices.
// According to:
// https://github.com/microsoft/onnxruntime/blob/9d650a4b2348d737407f9dbbf4f0cfd3789723c3/onnxruntime/core/session/provider_policy_context.cc#L24-L115
//
// Repeated calls with the same device set will return the same ordered devices,
// regardless of the input order of `available_devices`.
std::vector<const OrtEpDevice*> SortEpDevices(
    base::span<const OrtEpDevice* const> available_devices) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  std::vector<const OrtEpDevice*> sorted_devices(available_devices.begin(),
                                                 available_devices.end());
  std::stable_sort(
      sorted_devices.begin(), sorted_devices.end(),
      [ort_api](const OrtEpDevice* a, const OrtEpDevice* b) {
        OrtHardwareDeviceType a_device_type =
            ort_api->HardwareDevice_Type(ort_api->EpDevice_Device(a));
        OrtHardwareDeviceType b_device_type =
            ort_api->HardwareDevice_Type(ort_api->EpDevice_Device(b));

        if (a_device_type != b_device_type) {
          // Create priority values for clearer comparison.
          auto GetDevicePriority = [](OrtHardwareDeviceType type) -> uint32_t {
            switch (type) {
              case OrtHardwareDeviceType_NPU:
                return 3u;
              case OrtHardwareDeviceType_GPU:
                return 2u;
              case OrtHardwareDeviceType_CPU:
                return 1u;
            }
          };

          return GetDevicePriority(a_device_type) >
                 GetDevicePriority(b_device_type);
        }

        // Both devices are GPU.
        if (a_device_type == OrtHardwareDeviceType_GPU) {
          bool a_is_discrete = IsDiscreteGpu(a);
          bool b_is_discrete = IsDiscreteGpu(b);
          if (a_is_discrete != b_is_discrete) {
            return a_is_discrete;
          }
        }

        // EP vendor matching preference.
        bool a_matches_vendor = MatchesEpVendor(a);
        bool b_matches_vendor = MatchesEpVendor(b);
        if (a_matches_vendor != b_matches_vendor) {
          return a_matches_vendor;
        }

        bool a_is_default_cpu = IsDefaultCpuEpDevice(a);
        bool b_is_default_cpu = IsDefaultCpuEpDevice(b);
        CHECK(!(a_is_default_cpu && b_is_default_cpu))
            << "Default CPU EP should be unique.";

        // If neither are default CPU EP and both do/don't match vendor, sort by
        // EP name.
        if (!a_is_default_cpu && !b_is_default_cpu) {
          const char* ep_name_a = ort_api->EpDevice_EpName(a);
          const char* ep_name_b = ort_api->EpDevice_EpName(b);
          base::cstring_view ep_name_a_view =
              UNSAFE_BUFFERS(base::cstring_view(ep_name_a));
          base::cstring_view ep_name_b_view =
              UNSAFE_BUFFERS(base::cstring_view(ep_name_b));

          // WebGPU EP > DML EP
          bool a_is_webgpu = (ep_name_a_view == kWebGpuExecutionProvider);
          bool b_is_webgpu = (ep_name_b_view == kWebGpuExecutionProvider);
          bool a_is_dml = (ep_name_a_view == kDmlExecutionProvider);
          bool b_is_dml = (ep_name_b_view == kDmlExecutionProvider);

          if (a_is_webgpu && b_is_dml) {
            return true;
          }
          if (a_is_dml && b_is_webgpu) {
            return false;
          }

          // Arbitrarily sort for tie-breaking.
          // TODO(crbug.com/444049495): Implement a sophisticated tie-breaker
          // for this scenario.
          CHECK_NE(ep_name_a_view, ep_name_b_view);
          return ep_name_a_view < ep_name_b_view;
        }

        // Default CPU EP placed last.
        return !a_is_default_cpu;
      });

  return sorted_devices;
}

}  // namespace

// TODO(crbug.com/444049496): Log these selected EP devices when ORT logging
// level is set to VERBOSE or INFO.
std::vector<const OrtEpDevice*> SelectEpDevicesForDeviceType(
    base::span<const OrtEpDevice* const> available_devices,
    mojom::Device device_type) {
  // Apply WebNN's custom sorting.
  std::vector<const OrtEpDevice*> sorted_devices =
      SortEpDevices(available_devices);

  // Select devices based on the requested device type.
  std::vector<const OrtEpDevice*> selected_devices;
  switch (device_type) {
    case mojom::Device::kCpu:
      selected_devices = SelectEpDevicesForCpu(sorted_devices);
      break;
    case mojom::Device::kGpu:
      selected_devices = SelectEpDevicesForGpu(sorted_devices);
      break;
    case mojom::Device::kNpu:
      selected_devices = SelectEpDevicesForNpu(sorted_devices);
      break;
  }

  CHECK_LE(selected_devices.size(), 3u);
  return selected_devices;
}

}  // namespace webnn::ort
