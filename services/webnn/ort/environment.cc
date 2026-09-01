// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/environment.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_span.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split_win.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "services/webnn/ort/logging.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/ort/ort_session_options.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/trivial_model.h"
#include "services/webnn/public/cpp/webnn_device_util.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom-forward.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_ep_device_ep_metadata_keys.h"

namespace webnn::ort {

namespace {

// Returns paired spans of keys and values from OrtKeyValuePairs. The spans are
// valid for the lifetime of `key_value_pairs`.
std::pair<base::span<const char* const>, base::span<const char* const>>
GetKeyValueSpans(const OrtApi* ort_api,
                 const OrtKeyValuePairs* key_value_pairs) {
  size_t num_entries = 0;
  const char* const* keys = nullptr;
  const char* const* values = nullptr;
  ort_api->GetKeyValuePairs(key_value_pairs, &keys, &values, &num_entries);
  // SAFETY: ORT guarantees that `keys` and `values` are valid arrays
  // containing `num_entries` elements.
  return {UNSAFE_BUFFERS(base::span(keys, num_entries)),
          UNSAFE_BUFFERS(base::span(values, num_entries))};
}

// Returns a span of registered execution provider devices in `env`. The span is
// guaranteed to be valid until `env` is released or the list of execution
// providers is modified.
base::span<const OrtEpDevice* const> GetRegisteredEpDevicesImpl(
    const OrtApi* ort_api,
    const OrtEnv* env) {
  size_t num_ep_devices = 0;
  const OrtEpDevice* const* ep_devices = nullptr;
  CHECK_STATUS(ort_api->GetEpDevices(env, &ep_devices, &num_ep_devices));
  // SAFETY: ORT guarantees that `ep_devices` is valid and contains
  // `num_ep_devices` elements.
  return UNSAFE_BUFFERS(base::span(ep_devices, num_ep_devices));
}

bool IsExecutionProviderRegistered(const OrtApi* ort_api,
                                   const OrtEnv* env,
                                   std::string_view ep_name) {
  base::span<const OrtEpDevice* const> ep_devices =
      GetRegisteredEpDevicesImpl(ort_api, env);
  for (const auto* ep_device : ep_devices) {
    CHECK(ep_device);
    std::string_view registered_ep_name = ort_api->EpDevice_EpName(ep_device);
    if (ep_name == registered_ep_name) {
      return true;
    }
  }
  return false;
}

std::string_view OrtLoggingLevelToString(OrtLoggingLevel logging_level) {
  switch (logging_level) {
    case ORT_LOGGING_LEVEL_VERBOSE:
      return "VERBOSE";
    case ORT_LOGGING_LEVEL_INFO:
      return "INFO";
    case ORT_LOGGING_LEVEL_WARNING:
      return "WARNING";
    case ORT_LOGGING_LEVEL_ERROR:
      return "ERROR";
    case ORT_LOGGING_LEVEL_FATAL:
      return "FATAL";
  }
}

// This function is passed to ORT so that it can print logs within the sandbox.
void ORT_API_CALL OrtCustomLoggingFunction(void* /*param*/,
                                           OrtLoggingLevel severity,
                                           const char* category,
                                           const char* /*logid*/,
                                           const char* code_location,
                                           const char* message) {
  // Here all the logs are treated as errors for simplicity, which will not
  // cause the spam since the default logging level is set to
  // ORT_LOGGING_LEVEL_ERROR, and only when the user specifies a lower logging
  // level via `--webnn-ort-logging-level`, ORT will print the verbose logs.
  LOG(ERROR) << "[ORT] [" << OrtLoggingLevelToString(severity) << ": "
             << category << ", " << code_location << "] " << message;
}

bool MatchesEpVendor(const OrtEpDevice* ep_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  const char* ep_name = ort_api->EpDevice_EpName(ep_device);
  const auto iter = kKnownEPs.find(ep_name);
  if (iter == kKnownEPs.end()) {
    // Unknown EP, no matching possibility.
    return false;
  }

  // Returns true if the hardware device vendor id matches the EP vendor id.
  uint32_t hardware_device_vendor_id =
      ort_api->HardwareDevice_VendorId(ort_api->EpDevice_Device(ep_device));
  return iter->second.vendor_id == hardware_device_vendor_id;
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

  auto [keys, values] = GetKeyValueSpans(ort_api, device_metadata);
  for (auto [key, value] : std::views::zip(keys, values)) {
    if (std::string_view(key) == "Discrete") {
      return std::string_view(value) == "1";
    }
  }

  return false;
}

bool IsSoftwareGpu(const OrtEpDevice* device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  const OrtHardwareDevice* hardware_device = ort_api->EpDevice_Device(device);
  if (ort_api->HardwareDevice_Type(hardware_device) !=
      OrtHardwareDeviceType_GPU) {
    return false;
  }

  // Starting with Windows 8, an adapter called the "Microsoft Basic Render
  // Driver" is always present. This adapter has a VendorId of 0x1414 and a
  // DeviceID of 0x8c.
  // https://docs.microsoft.com/en-us/windows/desktop/direct3ddxgi/d3d10-graphics-programming-guide-dxgi#new-info-about-enumerating-adapters-for-windows-8
  return ort_api->HardwareDevice_VendorId(hardware_device) == 0x1414 &&
         ort_api->HardwareDevice_DeviceId(hardware_device) == 0x8c;
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

// Returns true if the EP name and hardware vendor id of both devices match.
// Used for selecting a device that is compatible with another device.
// Note: The order of lhs_device and rhs_device does not matter.
bool MatchEpNameAndHardwareVendor(const OrtEpDevice* lhs_device,
                                  const OrtEpDevice* rhs_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  std::string_view lhs_ep_name = ort_api->EpDevice_EpName(lhs_device);
  std::string_view rhs_ep_name = ort_api->EpDevice_EpName(rhs_device);
  if (lhs_ep_name != rhs_ep_name) {
    return false;
  }

  uint32_t lhs_vendor_id =
      ort_api->HardwareDevice_VendorId(ort_api->EpDevice_Device(lhs_device));
  uint32_t rhs_vendor_id =
      ort_api->HardwareDevice_VendorId(ort_api->EpDevice_Device(rhs_device));
  return lhs_vendor_id == rhs_vendor_id;
}

// If `primary_device` is nullptr, selects the first CPU device.
// If `primary_device` is not nullptr, selects the first CPU device that matches
// the hardware vendor id and EP name of `primary_device`.
// In both cases, also appends the default CPU EP device if the selected device
// is not the default one.
std::vector<const OrtEpDevice*> SelectEpDevicesForCpu(
    base::span<const OrtEpDevice* const> sorted_devices,
    const OrtEpDevice* primary_device = nullptr) {
  std::vector<const OrtEpDevice*> selected_devices;

  const OrtEpDevice* first_cpu = SelectFirstEpDeviceForDeviceType(
      sorted_devices, OrtHardwareDeviceType_CPU);

  // Having no CPU EP is expected since `sorted_devices` for the compiler
  // process filters out the default CPU EP.
  if (!first_cpu) {
    VLOG(2) << "[WebNN] No CPU execution provider available.";
    return selected_devices;
  }

  if (!primary_device || (primary_device && MatchEpNameAndHardwareVendor(
                                                primary_device, first_cpu))) {
    selected_devices.push_back(first_cpu);
  }

  // Add the default CPU EP device to ensure maximum coverage of opsets and
  // operators.
  if (!Environment::IsEpDevice(first_cpu, {kCPUExecutionProvider}) &&
      Environment::IsEpDevice(sorted_devices.back(), {kCPUExecutionProvider})) {
    selected_devices.push_back(sorted_devices.back());
  }

  return selected_devices;
}

// Select the first GPU device with CPU fallback.
std::vector<const OrtEpDevice*> SelectEpDevicesForGpu(
    base::span<const OrtEpDevice* const> sorted_devices) {
  const OrtEpDevice* first_gpu = SelectFirstEpDeviceForDeviceType(
      sorted_devices, OrtHardwareDeviceType_GPU);

  // Fall back to CPU when there is no GPU, or when the only GPU is a software
  // (CPU-emulated) adapter such as the Microsoft Basic Render Driver (WARP).
  // Software GPUs perform poorly and are not worth targeting. The DirectML EP
  // in particular throws and crashes the GPU process on them. See
  // crbug.com/466848120.
  if (!first_gpu || IsSoftwareGpu(first_gpu)) {
    return SelectEpDevicesForCpu(sorted_devices);
  }

  std::vector<const OrtEpDevice*> selected_devices;
  selected_devices.push_back(first_gpu);

  // To ensure the maximum compatibility of CPU fallback, always add the ORT CPU
  // EP, but only add an additional CPU EP from the same vendor as the GPU
  // device.
  std::vector<const OrtEpDevice*> cpu_fallback_devices =
      SelectEpDevicesForCpu(sorted_devices, first_gpu);
  selected_devices.insert(selected_devices.end(), cpu_fallback_devices.begin(),
                          cpu_fallback_devices.end());

  return selected_devices;
}

// Queries the OS driver version from the EP device metadata. Returns an
// empty string view if the driver version metadata is not found.
std::string_view GetOsDriverVersion(const OrtEpDevice* ep_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  const OrtKeyValuePairs* ep_metadata = ort_api->EpDevice_EpMetadata(ep_device);
  CHECK(ep_metadata);

  auto [keys, values] = GetKeyValueSpans(ort_api, ep_metadata);

  // For now, redefine the key for the EP OS driver version here according to
  // https://github.com/microsoft/onnxruntime/blob/56c984ffc417987eafcd9efb252ab2c65f24398a/include/onnxruntime/core/session/onnxruntime_ep_device_ep_metadata_keys.h#L13
  // TODO(crbug.com/474141335): Use the key from
  // onnxruntime_ep_device_ep_metadata_keys.h once it's available.
  constexpr std::string_view kOrtEpDeviceEpMetadataKeyOSDriverVersion =
      "os_driver_version";
  for (auto [key, value] : std::views::zip(keys, values)) {
    if (key == kOrtEpDeviceEpMetadataKeyOSDriverVersion) {
      return std::string_view(value);
    }
  }

  return std::string_view();
}

// Returns the compute architecture the EP published for `ep_device`, or an
// empty string view if it published nothing usable. EPs publish this where the
// driver is reachable so that the Compiler process, which cannot reach it, can
// still name the compilation target.
std::string_view GetTargetArchitecture(const OrtEpDevice* ep_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  const OrtKeyValuePairs* ep_metadata = ort_api->EpDevice_EpMetadata(ep_device);
  CHECK(ep_metadata);

  auto [keys, values] = GetKeyValueSpans(ort_api, ep_metadata);

  // TODO(crbug.com/552751647): Use a key from
  // onnxruntime_ep_device_ep_metadata_keys.h once ORT defines one.
  constexpr std::string_view kOrtEpDeviceEpMetadataKeyTargetArchitecture =
      "target_architecture";
  for (auto [key, value] : std::views::zip(keys, values)) {
    if (key == kOrtEpDeviceEpMetadataKeyTargetArchitecture) {
      std::string_view published(value);
      if (IsValidEpTargetArchitecture(published)) {
        return published;
      }
    }
  }

  return std::string_view();
}

// Returns whether the NPU driver version is blocked based on the known EPs
// info and the queried driver version from the EP device metadata.
bool IsNpuDriverVersionBlocked(const OrtEpDevice* npu_ep_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  std::string_view ep_name = ort_api->EpDevice_EpName(npu_ep_device);
  const auto iter = kKnownEPs.find(ep_name);
  // Currently, the NPU device must belong to a known EP.
  CHECK(iter != kKnownEPs.end());

  const EpInfo& ep_info = iter->second;
  if (ep_info.min_npu_driver_version.empty()) {
    // No minimum NPU driver version specified, allow all versions.
    return false;
  }

  OrtHardwareDeviceType device_type =
      ort_api->HardwareDevice_Type(ort_api->EpDevice_Device(npu_ep_device));
  CHECK_EQ(device_type, OrtHardwareDeviceType_NPU);

  // The min_npu_driver_version is in 4-part dot-separated format (e.g.,
  // "32.0.100.4404").
  base::Version min_version(ep_info.min_npu_driver_version);
  CHECK(min_version.IsValid());
  CHECK_EQ(min_version.components().size(), 4u);

  base::Version actual_version(GetOsDriverVersion(npu_ep_device));
  if (!actual_version.IsValid()) {
    // Unable to get or parse the driver version, consider it blocked.
    return true;
  }

  // The actual driver version from the EP may be in either the legacy
  // concatenated format (e.g., "1004404", formed by concatenating the last two
  // parts of the 4-part version) or the 4-part dot-separated format (e.g.,
  // "32.0.100.4404").
  if (actual_version.components().size() == 1) {
    // TODO(crbug.com/507885058): Remove this legacy path once the OV EP
    // reports os_driver_version in 4-part dot-separated format.
    //
    // Convert the min version to concatenated format by concatenating its
    // last two components for comparison.
    if (!ep_info.workarounds.npu_concatenated_driver_version) {
      // Unexpected single-component version from an EP that doesn't use
      // the legacy concatenated format, consider it blocked.
      return true;
    }
    std::string min_concatenated =
        base::StrCat({base::NumberToString(min_version.components()[2]),
                      base::NumberToString(min_version.components()[3])});
    min_version = base::Version(min_concatenated);
    CHECK(min_version.IsValid());
    CHECK_EQ(min_version.components().size(), 1u);
  } else if (actual_version.components().size() != 4) {
    // Only 4-part and legacy concatenated formats are expected.
    return true;
  }

  return actual_version < min_version;
}

// Select the first NPU device with CPU fallback. If no NPU device is found or
// the NPU driver version is blocked, delegate to GPU device selection logic
// which selects the first GPU device with CPU fallback.
std::vector<const OrtEpDevice*> SelectEpDevicesForNpu(
    base::span<const OrtEpDevice* const> sorted_devices) {
  const OrtEpDevice* first_npu = SelectFirstEpDeviceForDeviceType(
      sorted_devices, OrtHardwareDeviceType_NPU);

  if (!first_npu) {
    return SelectEpDevicesForGpu(sorted_devices);
  }

  if (IsNpuDriverVersionBlocked(first_npu)) {
    LOG(WARNING) << "[WebNN] [WARNING] The NPU driver version is blocked "
                 << "(actual: " << GetOsDriverVersion(first_npu)
                 << "). Falling back to GPU.";
    return SelectEpDevicesForGpu(sorted_devices);
  }

  std::vector<const OrtEpDevice*> selected_devices;
  selected_devices.push_back(first_npu);

  // To ensure the maximum compatibility of CPU fallback, always add the ORT CPU
  // EP, but only add an additional CPU EP from the same vendor as the NPU
  // device.
  std::vector<const OrtEpDevice*> cpu_fallback_devices =
      SelectEpDevicesForCpu(sorted_devices, first_npu);
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

        bool a_is_default_cpu =
            Environment::IsEpDevice(a, {kCPUExecutionProvider});
        bool b_is_default_cpu =
            Environment::IsEpDevice(b, {kCPUExecutionProvider});
        CHECK(!(a_is_default_cpu && b_is_default_cpu))
            << "Default CPU EP should be unique.";

        // If neither are default CPU EP and both do/don't match vendor, sort by
        // EP name.
        if (!a_is_default_cpu && !b_is_default_cpu) {
          std::string_view ep_name_a = ort_api->EpDevice_EpName(a);
          std::string_view ep_name_b = ort_api->EpDevice_EpName(b);

          // WebGPU EP > DML EP
          bool a_is_webgpu = (ep_name_a == kWebGpuExecutionProvider);
          bool b_is_webgpu = (ep_name_b == kWebGpuExecutionProvider);
          bool a_is_dml = (ep_name_a == kDmlExecutionProvider);
          bool b_is_dml = (ep_name_b == kDmlExecutionProvider);

          if (a_is_webgpu && b_is_dml) {
            return true;
          }
          if (a_is_dml && b_is_webgpu) {
            return false;
          }

          // Arbitrarily sort for tie-breaking.
          // TODO(crbug.com/444049495): Implement a sophisticated tie-breaker
          // for this scenario.
          return ep_name_a < ep_name_b;
        }

        // Default CPU EP placed last.
        return !a_is_default_cpu;
      });

  return sorted_devices;
}

// Indicates the information of a user-specified execution provider device
// parsed from the --webnn-ort-ep-device command line switch.
struct SpecifiedEpDeviceInfo {
  std::string ep_name;
  uint32_t hardware_vendor_id;
  uint32_t hardware_device_id;
};

// Parses the value of --webnn-ort-ep-device switch into a
// SpecifiedEpDeviceInfo. Returns an error string if the value is invalid.
base::expected<SpecifiedEpDeviceInfo, std::string> ParseEpDeviceSwitch(
    std::string_view value) {
  std::vector<std::string> parts = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() != 3) {
    return base::unexpected(
        "Invalid format: Expected "
        "<ep_name>,<hardware_vendor_id>,<hardware_device_id>, both "
        "hardware_vendor_id and hardware_device_id are hexadecimal strings.");
  }

  SpecifiedEpDeviceInfo info;
  info.ep_name = parts[0];

  if (!base::HexStringToUInt(parts[1], &info.hardware_vendor_id) ||
      !base::HexStringToUInt(parts[2], &info.hardware_device_id)) {
    return base::unexpected(
        "Failed to parse hardware_vendor_id or hardware_device_id as "
        "uint32_t.");
  }

  return info;
}

// Returns true if the device matches the user-specified SpecifiedEpDeviceInfo.
bool MatchSpecifiedEpDevice(const OrtEpDevice* ep_device,
                            const SpecifiedEpDeviceInfo& ep_device_info,
                            const OrtApi* ort_api) {
  std::string_view ep_name = ort_api->EpDevice_EpName(ep_device);
  uint32_t hardware_vendor_id =
      ort_api->HardwareDevice_VendorId(ort_api->EpDevice_Device(ep_device));
  uint32_t hardware_device_id =
      ort_api->HardwareDevice_DeviceId(ort_api->EpDevice_Device(ep_device));
  return ep_name == ep_device_info.ep_name &&
         hardware_vendor_id == ep_device_info.hardware_vendor_id &&
         hardware_device_id == ep_device_info.hardware_device_id;
}

// Selects the user-specified EP device from the available devices based on the
// switch value. Returns nullptr if no matching device is found or an error
// occurs during parsing the switch value.
const OrtEpDevice* SelectUserSpecifiedEpDevice(
    base::span<const OrtEpDevice* const> available_devices,
    std::string_view switch_value) {
  base::expected<SpecifiedEpDeviceInfo, std::string> ep_device_info_result =
      ParseEpDeviceSwitch(switch_value);
  if (!ep_device_info_result.has_value()) {
    LOG(ERROR)
        << "[WebNN] No EP device can be selected due to error in parsing "
        << switches::kWebNNOrtEpDevice << ": " << ep_device_info_result.error();
    return nullptr;
  }

  const SpecifiedEpDeviceInfo& specified_ep_device =
      ep_device_info_result.value();
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  // Find the first matching device.
  auto it = std::find_if(available_devices.begin(), available_devices.end(),
                         [&](const OrtEpDevice* ep_device) {
                           return MatchSpecifiedEpDevice(
                               ep_device, specified_ep_device, ort_api);
                         });

  if (it == available_devices.end()) {
    LOG(ERROR)
        << "[WebNN] No EP device can be selected due to no matching "
           "device for user-specified "
        << switches::kWebNNOrtEpDevice << ": " << switch_value
        << ". Please check the registered EP devices in the logs by setting "
        << switches::kWebNNOrtLoggingLevel << " to VERBOSE or INFO.";
    return nullptr;
  }

  return *it;
}

std::vector<mojom::WebNNExecutionProviderDetailsPtr>
ConvertEpListForIntrospection(base::span<const OrtEpDevice* const> ep_devices) {
  std::vector<mojom::WebNNExecutionProviderDetailsPtr> ep_details_list;
  ep_details_list.reserve(ep_devices.size());
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  for (const OrtEpDevice* ep_device : ep_devices) {
    auto ep_details = mojom::WebNNExecutionProviderDetails::New();
    // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
    ep_details->name = UNSAFE_BUFFERS(ort_api->EpDevice_EpName(ep_device));
    // SAFETY: ORT guarantees that `ep_vendor` is valid and null-terminated.
    ep_details->vendor = UNSAFE_BUFFERS(ort_api->EpDevice_EpVendor(ep_device));
    const OrtHardwareDevice* hardware_device =
        ort_api->EpDevice_Device(ep_device);
    CHECK(hardware_device);
    ep_details->hardware_type = DeviceTypeToString(
        OrtToWebnnDeviceType(ort_api->HardwareDevice_Type(hardware_device)));
    ep_details->vendor_id = base::StringPrintf(
        "0x%04x", ort_api->HardwareDevice_VendorId(hardware_device));
    ep_details->device_id = base::StringPrintf(
        "0x%04x", ort_api->HardwareDevice_DeviceId(hardware_device));
    const OrtKeyValuePairs* ep_metadata =
        ort_api->EpDevice_EpMetadata(ep_device);
    CHECK(ep_metadata);

    auto [keys, values] = GetKeyValueSpans(ort_api, ep_metadata);
    for (auto [key, value] : std::views::zip(keys, values)) {
      if (std::string_view(key) == "version") {
        ep_details->version = value;
        break;
      }
    }
    ep_details->first_selected = false;
    ep_details_list.push_back(std::move(ep_details));
  }
  return ep_details_list;
}

// Returns true if the EP device supports offline compilation.
bool EpDeviceSupportsOfflineCompilation(const OrtEpDevice* ep_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  std::string_view ep_name = ort_api->EpDevice_EpName(ep_device);
  auto ep_it = kKnownEPs.find(ep_name);
  if (ep_it == kKnownEPs.end()) {
    return false;
  }

  const OrtHardwareDevice* hardware_device =
      ort_api->EpDevice_Device(ep_device);
  mojom::Device device_type =
      OrtToWebnnDeviceType(ort_api->HardwareDevice_Type(hardware_device));

  const auto& offline_support = ep_it->second.offline_compilation_support;
  auto support_it = std::ranges::find(offline_support, device_type,
                                      &OfflineCompilationSupport::device_type);
  if (support_it == offline_support.end()) {
    VLOG(2) << "[WebNN] [" << ep_name
            << "] does not support offline compilation for device type: "
            << DeviceTypeToString(device_type);
    return false;
  }
  // An empty `device_ids` span means all device IDs for this device type are
  // supported, so skip the concrete device ID allowlist check. This covers both
  // a vendor-agnostic EP backing one generic device (the WebGPU EP) and a
  // single-vendor EP for which every device it enumerates qualifies (the
  // TensorRT-RTX EP). Only the former is a generic virtual device;
  // EpSupportsGenericVirtualDevice() additionally requires `vendor_id` 0.
  if (support_it->device_ids.empty()) {
    return true;
  }

  uint32_t device_id = ort_api->HardwareDevice_DeviceId(hardware_device);
  if (!std::ranges::contains(support_it->device_ids, device_id)) {
    VLOG(2) << "[WebNN] [" << ep_name
            << "] does not support offline compilation for device ID: 0x"
            << std::hex << device_id;
    return false;
  }
  return true;
}

// Returns true if `ep_name` backs a generic virtual device for `device_type`,
// i.e. it is vendor-agnostic (`vendor_id` == 0) and its
// `OfflineCompilationSupport` allows all device IDs (empty `device_ids`).
bool EpSupportsGenericVirtualDevice(std::string_view ep_name,
                                    mojom::Device device_type) {
  auto ep_it = kKnownEPs.find(ep_name);
  if (ep_it == kKnownEPs.end()) {
    return false;
  }
  if (ep_it->second.vendor_id != 0) {
    return false;
  }
  const auto& offline_support = ep_it->second.offline_compilation_support;
  auto support_it = std::ranges::find(offline_support, device_type,
                                      &OfflineCompilationSupport::device_type);
  return support_it != offline_support.end() && support_it->device_ids.empty();
}

// Returns true if `ep_device` is backed by a virtual (non-hardware) device,
// as indicated by the ORT `is_virtual` hardware device metadata.
bool IsVirtualDevice(const OrtEpDevice* ep_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  const OrtHardwareDevice* hardware_device =
      ort_api->EpDevice_Device(ep_device);
  CHECK(hardware_device);
  const OrtKeyValuePairs* device_metadata =
      ort_api->HardwareDevice_Metadata(hardware_device);
  CHECK(device_metadata);

  auto [keys, values] = GetKeyValueSpans(ort_api, device_metadata);
  for (auto [key, value] : std::views::zip(keys, values)) {
    if (std::string_view(key) == kOrtHardwareDevice_MetadataKey_IsVirtual) {
      return std::string_view(value) == "1";
    }
  }
  return false;
}

}  // namespace

// static
std::optional<scoped_refptr<Environment>> Environment::GetInstance() {
  base::AutoLock auto_lock(GetLock());
  if (instance_) {
    return base::WrapRefCounted(instance_);
  }
  return std::nullopt;
}

// static
base::expected<scoped_refptr<Environment>, std::string>
Environment::GetOrCreateInstance(
    const base::flat_map<std::string, mojom::EpPackageInfoPtr>&
        ep_package_info_map) {
  base::AutoLock auto_lock(GetLock());
  if (instance_) {
    return base::WrapRefCounted(instance_);
  }
  return Create(ep_package_info_map);
}

// static
base::expected<scoped_refptr<Environment>, std::string> Environment::Create(
    const base::flat_map<std::string, mojom::EpPackageInfoPtr>&
        ep_package_info_map) {
  SCOPED_UMA_HISTOGRAM_TIMER("WebNN.ORT.TimingMs.CreateEnvironment");

  if (!PlatformFunctions::EnsureInitialized()) {
    return base::unexpected("Failed to get ONNX Runtime platform functions.");
  }

  const auto* platform_functions = PlatformFunctions::GetInstance();

  OrtLoggingLevel ort_logging_level = GetOrtLoggingLevel();

  const OrtApi* ort_api = platform_functions->ort_api();
  ScopedOrtEnv env;
  if (ORT_CALL_FAILED(ort_api->CreateEnvWithCustomLogger(
          OrtCustomLoggingFunction, /*logger_param=*/nullptr, ort_logging_level,
          /*logid=*/"WebNN", ScopedOrtEnv::Receiver(env).get()))) {
    return base::unexpected("Failed to create the ONNX Runtime environment.");
  }

  // Register EPs from `ep_package_info_map` if they are not registered yet.
  // Failure is ignored.
  for (const auto& [ep_name, package_info] : ep_package_info_map) {
    if (IsExecutionProviderRegistered(ort_api, env.get(), ep_name)) {
      continue;
    }

    // Skip the package dependency initialization for entries with an empty
    // family name (e.g. injected by `kWebNNOrtEpLibraryPathForTesting`).
    if (!package_info->family_name.empty() &&
        !GetDependentEpPackages().contains(package_info->family_name)) {
      if (platform_functions
              ->InitializePackageDependency(package_info->family_name,
                                            package_info->version)
              .empty()) {
        continue;
      }
      GetDependentEpPackages().insert(package_info->family_name);
    }

    CALL_ORT_FUNC(ort_api->RegisterExecutionProviderLibrary(
        env.get(), ep_name.c_str(),
        package_info->library_path.value().c_str()));
  }

  if (ort_logging_level == ORT_LOGGING_LEVEL_VERBOSE ||
      ort_logging_level == ORT_LOGGING_LEVEL_INFO) {
    // Logs all registered EP devices in this environment.
    LogEpDevices(ort_api, GetRegisteredEpDevicesImpl(ort_api, env.get()),
                 "Registered OrtEpDevice");
  }

  return base::MakeRefCounted<Environment>(base::PassKey<Environment>(),
                                           std::move(env));
}

// static
base::expected<scoped_refptr<Environment>, std::string>
Environment::InitializeForCompilerProcess(const base::FilePath& ep_library_path,
                                          const EpDeviceInfo& target_device) {
  auto env_result = CreateForCompilerProcess(ep_library_path, target_device);
  if (!env_result.has_value()) {
    return env_result;
  }
  // Ensure that the target device is registered in the environment.
  if (!env_result.value()->FindRegisteredEpDevice(target_device)) {
    return base::unexpected(base::StrCat(
        {"Target device not registered: ", target_device.ToSwitchValue()}));
  }
  // Warm up the target device for the compiler process to ensure that the
  // libraries required for offline compilation are preloaded.
  auto warmup_result =
      env_result.value()->WarmupEpDeviceForCompilerProcess(target_device);
  if (!warmup_result.has_value()) {
    return base::unexpected(std::move(warmup_result.error()));
  }
  return env_result;
}

// static
base::expected<scoped_refptr<Environment>, std::string>
Environment::CreateForCompilerProcess(const base::FilePath& ep_library_path,
                                      const EpDeviceInfo& target_device) {
  base::AutoLock auto_lock(GetLock());
  CHECK(!instance_) << "Environment instance already exists.";

  if (!PlatformFunctions::EnsureInitialized()) {
    return base::unexpected("Failed to get ONNX Runtime platform functions.");
  }

  const auto* platform_functions = PlatformFunctions::GetInstance();
  const OrtApi* ort_api = platform_functions->ort_api();
  const OrtLoggingLevel ort_logging_level = GetOrtLoggingLevel();

  ScopedOrtKeyValuePairs config_entries;
  ort_api->CreateKeyValuePairs(
      ScopedOrtKeyValuePairs::Receiver(config_entries).get());

  // Skip the allow-virtual-devices config when `kWebNNOrtDisableVirtualDevices`
  // is set, so the Compiler process exercises the actual hardware devices
  // instead of virtual ones.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtDisableVirtualDevices)) {
    // Allow the virtual devices to enable offline compilation without requiring
    // the actual device.
    // https://github.com/microsoft/onnxruntime/blob/3874516/include/onnxruntime/core/session/onnxruntime_env_config_keys.h#L24
    ort_api->AddKeyValuePair(config_entries.get(), "allow_virtual_devices",
                             "1");
  }

  // Describe the device to compile for. The EP would normally learn these by
  // querying the driver while it enumerates devices, but the Compiler process
  // is locked down before it runs any code and cannot reach the driver. ORT
  // reserves environment config keys prefixed "ep_factory.<ep_name>." for the
  // EP registered under that name.
  const auto ep_it = kKnownEPs.find(target_device.ep_name);
  if (ep_it != kKnownEPs.end()) {
    // An EP either describes its compilation target through both keys or
    // through neither. One without the other would leave the EP naming a device
    // it cannot build for, or building for a device it cannot name.
    CHECK_EQ(ep_it->second.target_architecture_env_config_key.empty(),
             ep_it->second.hardware_device_id_env_config_key.empty());

    auto add_config_entry = [&](base::cstring_view key,
                                base::cstring_view value) {
      const std::string config_key =
          base::StrCat({"ep_factory.", target_device.ep_name, ".", key});
      ort_api->AddKeyValuePair(config_entries.get(), config_key.c_str(),
                               value.c_str());
      VLOG(1) << "[WebNN] [" << target_device.ep_name << "] compiling for "
              << config_key << "=" << value << ".";
    };

    if (!ep_it->second.target_architecture_env_config_key.empty() &&
        !target_device.target_architecture.empty()) {
      add_config_entry(ep_it->second.target_architecture_env_config_key,
                       target_device.target_architecture);
    }

    // The EP parses this in base 16.
    if (!ep_it->second.hardware_device_id_env_config_key.empty()) {
      add_config_entry(ep_it->second.hardware_device_id_env_config_key,
                       base::StringPrintf("%04x", target_device.device_id));
    }
  }

  OrtEnvCreationOptions env_options = {
      .version = ORT_API_VERSION,
      .logging_severity_level = static_cast<int32_t>(ort_logging_level),
      .log_id = "WebNN",
      .custom_logging_function = OrtCustomLoggingFunction,
      .custom_logging_param = nullptr,
      .threading_options = nullptr,
      .config_entries = config_entries.get(),
  };

  ScopedOrtEnv env;
  if (ORT_CALL_FAILED(ort_api->CreateEnvWithOptions(
          &env_options, ScopedOrtEnv::Receiver(env).get()))) {
    return base::unexpected("Failed to create the ONNX Runtime environment.");
  }
  if (ORT_CALL_FAILED(ort_api->RegisterExecutionProviderLibrary(
          env.get(), target_device.ep_name.c_str(),
          ep_library_path.value().c_str()))) {
    return base::unexpected(
        "Failed to register the execution provider library.");
  }

  if (ort_logging_level == ORT_LOGGING_LEVEL_VERBOSE ||
      ort_logging_level == ORT_LOGGING_LEVEL_INFO) {
    // Logs all registered EP devices in this environment.
    LogEpDevices(ort_api, GetRegisteredEpDevicesImpl(ort_api, env.get()),
                 "Registered OrtEpDevice");
  }
  return base::MakeRefCounted<Environment>(base::PassKey<Environment>(),
                                           std::move(env));
}

base::expected<void, std::string> Environment::WarmupEpDeviceForCompilerProcess(
    const EpDeviceInfo& target_device) {
  auto* platform_functions = PlatformFunctions::GetInstance();
  const OrtCompileApi* ort_compile_api = platform_functions->ort_compile_api();

  // Create the session options on the target device.
  auto session_options = SessionOptions::Create(target_device, this);
  ScopedOrtModelCompilationOptions compile_options;
  CHECK_STATUS(ort_compile_api->CreateModelCompilationOptionsFromSessionOptions(
      env_.get(), session_options->get(),
      ScopedOrtModelCompilationOptions::Receiver(compile_options).get()));
  CHECK_STATUS(ort_compile_api->ModelCompilationOptions_SetInputModelFromBuffer(
      compile_options.get(), kTrivialModel, sizeof(kTrivialModel)));

  // Embed EP context binary data into the output model buffer.
  CHECK_STATUS(ort_compile_api->ModelCompilationOptions_SetEpContextEmbedMode(
      compile_options.get(), /*embed_ep_context_in_model=*/true));

  const OrtApi* ort_api = platform_functions->ort_api();

  OrtAllocator* default_allocator = nullptr;
  CHECK_STATUS(ort_api->GetAllocatorWithDefaultOptions(&default_allocator));

  void* output_model_buffer = nullptr;
  size_t output_model_buffer_size = 0;
  CHECK_STATUS(ort_compile_api->ModelCompilationOptions_SetOutputModelBuffer(
      compile_options.get(), default_allocator, &output_model_buffer,
      &output_model_buffer_size));

  // This compilation step will trigger the EP to warm up and load the required
  // libraries. Unlike the calls above, which only configure the compilation,
  // this one exercises the EP and can legitimately fail, so report the failure
  // rather than terminating the process on it.
  if (ORT_CALL_FAILED(
          ort_compile_api->CompileModel(env_.get(), compile_options.get()))) {
    return base::unexpected(
        "Failed to compile the warmup model on the target device.");
  }
  CHECK(output_model_buffer);
  CHECK_GT(output_model_buffer_size, 0u);

  default_allocator->Free(default_allocator, output_model_buffer);
  return base::ok();
}

Environment::Environment(base::PassKey<Environment> /*pass_key*/,
                         ScopedOrtEnv env)
    : base::subtle::RefCountedThreadSafeBase(
          base::subtle::GetRefCountPreference<Environment>()),
      env_(std::move(env)),
      graph_compilation_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::USER_VISIBLE,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
               base::MayBlock()})) {
  CHECK_EQ(instance_, nullptr);
  instance_ = this;
}

Environment::~Environment() = default;

void Environment::AddRef() const {
  base::subtle::RefCountedThreadSafeBase::AddRefWithCheck();
}

void Environment::Release() const {
  base::AutoLock auto_lock(GetLock());
  if (base::subtle::RefCountedThreadSafeBase::Release()) {
    ANALYZER_SKIP_THIS_PATH();
    CHECK_EQ(instance_, this);
    instance_ = nullptr;
    delete this;
  }
}

// static
std::vector<const OrtEpDevice*> Environment::SelectEpDevices(
    base::span<const OrtEpDevice* const> available_devices,
    OrtHardwareDeviceType device_type) {
  // Try to select only one EP device by user switch first.
  std::vector<const OrtEpDevice*> selected_devices;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtEpDevice)) {
    std::string switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kWebNNOrtEpDevice);
    const OrtEpDevice* specified_ep_device =
        SelectUserSpecifiedEpDevice(available_devices, switch_value);
    if (specified_ep_device) {
      selected_devices.push_back(specified_ep_device);
    }
  } else {
    // Apply WebNN's custom sorting.
    std::vector<const OrtEpDevice*> sorted_devices =
        SortEpDevices(available_devices);
    // Select devices based on the requested device type.
    switch (device_type) {
      case OrtHardwareDeviceType_CPU:
        selected_devices = SelectEpDevicesForCpu(sorted_devices);
        break;
      case OrtHardwareDeviceType_GPU:
        selected_devices = SelectEpDevicesForGpu(sorted_devices);
        break;
      case OrtHardwareDeviceType_NPU:
        selected_devices = SelectEpDevicesForNpu(sorted_devices);
        break;
    }
    CHECK_LE(selected_devices.size(), 3u);
  }

  return selected_devices;
}

std::optional<EpDeviceInfo> Environment::SelectEpDeviceForCompiler(
    OrtHardwareDeviceType device_type) {
  if (device_type == OrtHardwareDeviceType_CPU) {
    VLOG(2) << "[WebNN] CPU device is not supported for offline compilation.";
    return std::nullopt;
  }

  base::span<const OrtEpDevice* const> registered_ep_devices =
      GetRegisteredEpDevices();

  // Filter out EP devices that don't support offline compilation before running
  // EP selection, so selection only considers compiler-eligible devices.
  // Skipped when testing online compilation on real hardware or when all
  // compiler devices are explicitly allowed.
  const bool allow_all_compiler_devices =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtDisableVirtualDevices) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtAllowAllCompilerDevices);
  std::vector<const OrtEpDevice*> candidate_devices;
  for (const OrtEpDevice* ep_device : registered_ep_devices) {
    if (allow_all_compiler_devices ||
        EpDeviceSupportsOfflineCompilation(ep_device)) {
      candidate_devices.push_back(ep_device);
    }
  }

  std::vector<const OrtEpDevice*> selected_devices =
      SelectEpDevices(candidate_devices, device_type);
  if (selected_devices.empty()) {
    VLOG(1) << "[WebNN] No suitable EP device found for compiler, device type: "
            << DeviceTypeToString(OrtToWebnnDeviceType(device_type));
    return std::nullopt;
  }
  const OrtEpDevice* selected_ep_device = selected_devices[0];

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  std::string_view selected_ep_name =
      ort_api->EpDevice_EpName(selected_ep_device);
  const OrtHardwareDevice* selected_hardware_device =
      ort_api->EpDevice_Device(selected_ep_device);
  mojom::Device selected_device_type = OrtToWebnnDeviceType(
      ort_api->HardwareDevice_Type(selected_hardware_device));
  uint32_t selected_device_id =
      ort_api->HardwareDevice_DeviceId(selected_hardware_device);
  uint32_t selected_vendor_id =
      ort_api->HardwareDevice_VendorId(selected_hardware_device);

  EpDeviceInfo selected_device_info = {
      .ep_name = std::string(selected_ep_name),
      .device_type = selected_device_type,
      .device_id = selected_device_id,
      .vendor_id = selected_vendor_id,
  };

  // Carry the architecture the EP published for this device, so that the
  // Compiler process can compile for it without the hardware query it cannot
  // make. Only EPs that accept the value back have a use for it.
  const auto ep_it = kKnownEPs.find(selected_ep_name);
  if (ep_it != kKnownEPs.end() &&
      !ep_it->second.target_architecture_env_config_key.empty()) {
    std::string_view published = GetTargetArchitecture(selected_ep_device);
    if (!published.empty()) {
      selected_device_info.target_architecture = std::string(published);
    }
  }

  VLOG(1) << "[WebNN] Selected EP device for compiler: "
          << selected_device_info.ToSwitchValue();

  return selected_device_info;
}

// static
bool Environment::IsEpDevice(const OrtEpDevice* device,
                             base::span<const std::string_view> ep_names) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  std::string_view ep_name = ort_api->EpDevice_EpName(device);
  return std::ranges::contains(ep_names, ep_name);
}

base::span<const OrtEpDevice* const> Environment::GetRegisteredEpDevices()
    const {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  return GetRegisteredEpDevicesImpl(ort_api, this->get());
}

const OrtEpDevice* Environment::FindRegisteredEpDevice(
    const EpDeviceInfo& device_info) const {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  base::span<const OrtEpDevice* const> registered_ep_devices =
      GetRegisteredEpDevices();

  // Returns true if `ep_device`'s EP name and hardware device type both match
  // `device_info`.
  auto matches_ep_and_type = [&](const OrtEpDevice* ep_device) {
    return ort_api->EpDevice_EpName(ep_device) == device_info.ep_name &&
           ort_api->HardwareDevice_Type(ort_api->EpDevice_Device(ep_device)) ==
               WebnnToOrtDeviceType(device_info.device_type);
  };

  // Returns true if `ep_device` additionally matches the hardware vendor ID and
  // device ID of `device_info` (an exact hardware match).
  auto matches_hardware_ids = [&](const OrtEpDevice* ep_device) {
    const OrtHardwareDevice* hardware_device =
        ort_api->EpDevice_Device(ep_device);
    return ort_api->HardwareDevice_DeviceId(hardware_device) ==
               device_info.device_id &&
           ort_api->HardwareDevice_VendorId(hardware_device) ==
               device_info.vendor_id;
  };

  // Some EPs (e.g. the WebGPU EP) back a generic virtual device (vendor-
  // agnostic, with an empty `OfflineCompilationSupport::device_ids`). In the
  // sandboxed Compiler process only that virtual device is registered, and its
  // IDs won't match the real IDs in `device_info`, so prefer it and match on
  // EP name and device type only.
  //
  // A single-vendor EP like the TensorRT-RTX EP is not generic: it is handed
  // the device id to describe its virtual device with and it knows its own
  // vendor id, so that device matches on hardware IDs below like a discovered
  // one.
  const bool ep_supports_generic_virtual_device =
      EpSupportsGenericVirtualDevice(device_info.ep_name,
                                     device_info.device_type);

  const OrtEpDevice* exactly_matched_device = nullptr;
  for (const auto* ep_device : registered_ep_devices) {
    CHECK(ep_device);
    if (!matches_ep_and_type(ep_device)) {
      continue;
    }
    // Prefer the generic virtual device and return as soon as one is found.
    if (ep_supports_generic_virtual_device && IsVirtualDevice(ep_device)) {
      return ep_device;
    }
    // Otherwise remember the exact hardware match. This is the only match for
    // non-generic EPs, and the fallback for generic virtual EPs when no virtual
    // device is registered: either in the GPU process, whose environment never
    // enables virtual devices, or in the Compiler process when virtual devices
    // are disabled for testing via --webnn-ort-disable-virtual-devices (usually
    // together with --disable-webnn-compiler-sandbox, since reaching the actual
    // hardware is otherwise blocked by the compiler sandbox). Matching the
    // hardware IDs also correctly disambiguates multiple real devices of the
    // same type (e.g. integrated vs discrete GPU).
    if (matches_hardware_ids(ep_device)) {
      exactly_matched_device = ep_device;
    }
  }
  return exactly_matched_device;
}

std::vector<mojom::WebNNExecutionProviderDetailsPtr>
Environment::GetAvailableEpDetails() const {
  return ConvertEpListForIntrospection(GetRegisteredEpDevices());
}

std::vector<mojom::WebNNExecutionProviderDetailsPtr>
Environment::GetSelectedEpDetails(OrtHardwareDeviceType device_type) const {
  base::span<const OrtEpDevice* const> registered_ep_devices =
      GetRegisteredEpDevices();
  std::vector<const OrtEpDevice*> selected_ep_devices =
      Environment::SelectEpDevices(registered_ep_devices, device_type);
  auto ep_list = ConvertEpListForIntrospection(selected_ep_devices);
  // Mark the first EP as selected for introspection purposes.
  if (!ep_list.empty()) {
    ep_list.front()->first_selected = true;
  }
  return ep_list;
}

EpWorkarounds Environment::GetEpWorkarounds(
    OrtHardwareDeviceType device_type) const {
  EpWorkarounds workarounds;
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  base::span<const OrtEpDevice* const> registered_ep_devices =
      GetRegisteredEpDevices();
  std::vector<const OrtEpDevice*> selected_ep_devices =
      SelectEpDevices(registered_ep_devices, device_type);
  for (const auto* ep_device : selected_ep_devices) {
    CHECK(ep_device);
    std::string_view ep_name = ort_api->EpDevice_EpName(ep_device);
    const auto iter = kKnownEPs.find(ep_name);
    if (iter != kKnownEPs.end()) {
      workarounds |= iter->second.workarounds;
    }
  }
  return workarounds;
}

// static
base::Lock& Environment::GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

Environment* Environment::instance_ = nullptr;

// static
base::flat_set<std::wstring>& Environment::GetDependentEpPackages() {
  static base::NoDestructor<base::flat_set<std::wstring>> packages;
  return *packages;
}

}  // namespace webnn::ort
