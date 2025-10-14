// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/environment.h"

#include <set>
#include <string_view>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/memory/raw_span.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_split_win.h"
#include "base/strings/utf_string_conversions.h"
#include "services/webnn/ort/logging.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/webnn_switches.h"

namespace webnn::ort {

namespace {

struct EpInfo {
  base::wcstring_view package_family_name;
  base::wcstring_view library_name;
  PACKAGE_VERSION package_version;
  // Represents the vendor id of the hardware device used by the execution
  // provider.
  uint32_t vendor_id;
  EpWorkarounds workarounds;
  base::raw_span<const Environment::SessionConfigEntry> config_entries;
};

constexpr auto kKnownEPs = base::MakeFixedFlatMap<base::cstring_view, EpInfo>({
    // Intel
    {
        "OpenVINOExecutionProvider",
        {
            .package_family_name = L"MicrosoftCorporationII.WinML.Intel."
                                   L"OpenVINO.EP.1.8_8wekyb3d8bbwe",
            .library_name = L"onnxruntime_providers_openvino_plugin.dll",
            .package_version =
                {
                    .Major = 0,
                    .Minor = 0,
                    .Build = 0,
                    .Revision = 0,
                },
            .vendor_id = 0x8086,
            .workarounds =
                {
                    .resample2d_limit_to_nchw = true,
                },
            // OpenVINO EP configuration. Keys and values must align with the
            // ORT OpenVINO EP implementation. See:
            // https://github.com/microsoft/onnxruntime/blob/f46113d7b11af3fa0b3918029e442c3a14265522/onnxruntime/core/providers/openvino/openvino_provider_factory.cc#L459
            // and
            // https://onnxruntime.ai/docs/execution-providers/OpenVINO-ExecutionProvider.html#summary-of-options.
            //
            // To get more accurate inference results, WebNN requires the
            // accuracy execution mode on OpenVINO GPU/NPU to avoid lowering the
            // execution accuracy for performance reasons, maintain original
            // model precision (f32→f32, f16→f16) and disable dynamic
            // quantization. See:
            // https://docs.openvino.ai/2025/openvino-workflow/running-inference/optimize-inference/precision-control.html.
            //
            // On OpenVINO GPU, the default `fp16` precision specified by
            // `INFERENCE_PRECISION_HINT` can override the `ACCURACY` mode set
            // by `EXECUTION_MODE_HINT`. To improve robustness and ensure
            // accurate inference results, we explicitly set
            // `INFERENCE_PRECISION_HINT`
            //  to `dynamic`.
            .config_entries =
                (const Environment::SessionConfigEntry[]){
                    {.key = "ep.openvinoexecutionprovider.load_config",
                     .value = R"({
                            "GPU": {
                                "EXECUTION_MODE_HINT": "ACCURACY",
                                "INFERENCE_PRECISION_HINT": "dynamic"
                            },
                            "NPU": {
                                "EXECUTION_MODE_HINT": "ACCURACY"
                            }
                        })"},
                },
        },
    },
    // NVidia
    {
        "NvTensorRTRTXExecutionProvider",
        {
            .package_family_name = L"MicrosoftCorporationII.WinML.NVIDIA.TRT-"
                                   L"RTX.EP.1.8_8wekyb3d8bbwe",
            .library_name = L"onnxruntime_providers_nv_tensorrt_rtx.dll",
            .package_version =
                {
                    .Major = 0,
                    .Minor = 0,
                    .Build = 0,
                    .Revision = 0,
                },
            .vendor_id = 0x10de,
        },
    },
    // Qualcomm
    {
        "QNNExecutionProvider",
        {
            .package_family_name = L"MicrosoftCorporationII.WinML.Qualcomm.QNN."
                                   L"EP.1.8_8wekyb3d8bbwe",
            .library_name = L"onnxruntime_providers_qnn.dll",
            .package_version =
                {
                    .Major = 0,
                    .Minor = 0,
                    .Build = 0,
                    .Revision = 0,
                },
            .vendor_id = 0x4d4f4351,
        },
    },
});

// Returns true if the `vendor_id` exists in the `gpu_info`.
bool VendorIdExistsInGpuInfo(const gpu::GPUInfo& gpu_info, uint32_t vendor_id) {
  if (gpu_info.active_gpu().vendor_id == vendor_id) {
    return true;
  }
  for (const auto& secondary_gpu : gpu_info.secondary_gpus) {
    if (secondary_gpu.vendor_id == vendor_id) {
      return true;
    }
  }
  for (const auto& npu : gpu_info.npus) {
    if (npu.vendor_id == vendor_id) {
      return true;
    }
  }
  return false;
}

// Returns a span of registered execution provider devices in `env`. The span is
// guaranteed to be valid until `env` is released or the list of execution
// providers is modified.
base::span<const OrtEpDevice* const> GetRegisteredEpDevices(
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
                                   base::cstring_view ep_name) {
  base::span<const OrtEpDevice* const> ep_devices =
      GetRegisteredEpDevices(ort_api, env);
  for (const auto* ep_device : ep_devices) {
    CHECK(ep_device);
    const char* registered_ep_name = ort_api->EpDevice_EpName(ep_device);
    // SAFETY: ORT guarantees that `registered_ep_name` is valid and
    // null-terminated.
    if (registered_ep_name &&
        ep_name == UNSAFE_BUFFERS(base::cstring_view(registered_ep_name))) {
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

// Parses the value of `--webnn-ort-ep-library-path-for-testing` switch. Returns
// the ORT EP name and library path pair if the value is valid. Otherwise,
// returns the error message.
base::expected<std::pair<std::string, base::FilePath>, std::string>
ParseEpLibraryPathSwitch(std::wstring_view value) {
  std::vector<std::wstring> parts = base::SplitString(
      value, L"?", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() != 2) {
    return base::unexpected(
        "Invalid format of the specified EP library path. It should be in "
        "the format of <ep_name>?<ep_library_path>.");
  }
  std::string ep_name = base::WideToUTF8(parts[0]);
  base::FilePath ep_library_path(parts[1]);

  if (!kKnownEPs.contains(ep_name)) {
    return base::unexpected("The specified EP name is not recognized.");
  }

  return std::make_pair(ep_name, ep_library_path);
}

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

  const char* ep_name = ort_api->EpDevice_EpName(ep_device);
  // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
  const auto iter = kKnownEPs.find(UNSAFE_BUFFERS(base::cstring_view(ep_name)));
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

// Returns true if the EP name and hardware vendor id of both devices match.
// Used for selecting a device that is compatible with another device.
// Note: The order of lhs_device and rhs_device does not matter.
bool MatchEpNameAndHardwareVendor(const OrtEpDevice* lhs_device,
                                  const OrtEpDevice* rhs_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  const char* lhs_ep_name = ort_api->EpDevice_EpName(lhs_device);
  const char* rhs_ep_name = ort_api->EpDevice_EpName(rhs_device);
  // SAFETY: ORT guarantees that EP names are valid and null-terminated.
  base::cstring_view lhs_ep_name_view =
      UNSAFE_BUFFERS(base::cstring_view(lhs_ep_name));
  base::cstring_view rhs_ep_name_view =
      UNSAFE_BUFFERS(base::cstring_view(rhs_ep_name));
  if (lhs_ep_name_view != rhs_ep_name_view) {
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

  // Handle the rare case where no CPU EP device is available.
  if (!first_cpu) {
    LOG(ERROR) << "[WebNN] No CPU execution provider available.";
    return selected_devices;
  }

  if (!primary_device || (primary_device && MatchEpNameAndHardwareVendor(
                                                primary_device, first_cpu))) {
    selected_devices.push_back(first_cpu);
  }

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
  const OrtEpDevice* first_gpu = SelectFirstEpDeviceForDeviceType(
      sorted_devices, OrtHardwareDeviceType_GPU);

  if (!first_gpu) {
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
          return ep_name_a_view < ep_name_b_view;
        }

        // Default CPU EP placed last.
        return !a_is_default_cpu;
      });

  return sorted_devices;
}

}  // namespace

// static
base::expected<scoped_refptr<Environment>, std::string>
Environment::GetInstance(const gpu::GPUInfo& gpu_info) {
  base::AutoLock auto_lock(GetLock());
  if (instance_) {
    return base::WrapRefCounted(instance_);
  }
  return Create(gpu_info);
}

// static
base::expected<scoped_refptr<Environment>, std::string> Environment::Create(
    const gpu::GPUInfo& gpu_info) {
  auto* platform_functions = PlatformFunctions::GetInstance();
  if (!platform_functions) {
    return base::unexpected("Failed to get ONNX Runtime platform functions.");
  }

  OrtLoggingLevel ort_logging_level = GetOrtLoggingLevel();

  const OrtApi* ort_api = platform_functions->ort_api();
  ScopedOrtEnv env;
  if (ORT_CALL_FAILED(ort_api->CreateEnvWithCustomLogger(
          OrtCustomLoggingFunction, /*logger_param=*/nullptr, ort_logging_level,
          /*logid=*/"WebNN", ScopedOrtEnv::Receiver(env).get()))) {
    return base::unexpected("Failed to create the ONNX Runtime environment.");
  }

  // Get the ORT EP name and library path pair specified by
  // `kWebNNOrtEpLibraryPathForTesting` switch if it exists and the switch value
  // is valid.
  std::optional<std::pair<std::string, base::FilePath>> specified_ep_path_info;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtEpLibraryPathForTesting)) {
    std::wstring value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
            switches::kWebNNOrtEpLibraryPathForTesting);
    auto result = ParseEpLibraryPathSwitch(value);
    if (!result.has_value()) {
      LOG(WARNING) << "[WebNN] Invalid value of the switch "
                   << switches::kWebNNOrtEpLibraryPathForTesting << ": "
                   << result.error() << " The switch will be ignored.";
    } else {
      specified_ep_path_info = result.value();
    }
  }

  // Register the execution provider based on the GPU/NPU vendor id if it's not
  // registered yet. Ultimately, ignore the failure of registering the EP.
  for (const auto& [ep_name, ep_info] : kKnownEPs) {
    if (!VendorIdExistsInGpuInfo(gpu_info, ep_info.vendor_id)) {
      continue;
    }

    if (IsExecutionProviderRegistered(ort_api, env.get(), ep_name)) {
      continue;
    }

    // First try to load EP libraries from the specified path by
    // `kWebNNOrtEpLibraryPathForTesting` switch if the EP name matches the
    // specified EP name. Otherwise, try to load it from the EP package path.
    base::FilePath ep_library_path;
    if (specified_ep_path_info && ep_name == specified_ep_path_info->first) {
      ep_library_path = specified_ep_path_info->second;
    } else {
      const std::optional<base::FilePath>& ep_package_path =
          platform_functions->InitializePackageDependency(
              ep_info.package_family_name, ep_info.package_version);
      if (!ep_package_path) {
        continue;
      }
      ep_library_path = ep_package_path->Append(L"ExecutionProvider")
                            .Append(ep_info.library_name);
    }

    CALL_ORT_FUNC(ort_api->RegisterExecutionProviderLibrary(
        env.get(), ep_name.c_str(), ep_library_path.value().c_str()));
  }

  if (ort_logging_level == ORT_LOGGING_LEVEL_VERBOSE ||
      ort_logging_level == ORT_LOGGING_LEVEL_INFO) {
    // Logs all registered EP devices in this environment.
    LogEpDevices(ort_api, GetRegisteredEpDevices(ort_api, env.get()),
                 "Registered OrtEpDevice");
  }

  return base::MakeRefCounted<Environment>(base::PassKey<Environment>(),
                                           std::move(env));
}

Environment::Environment(base::PassKey<Environment> /*pass_key*/,
                         ScopedOrtEnv env)
    : base::subtle::RefCountedThreadSafeBase(
          base::subtle::GetRefCountPreference<Environment>()),
      env_(std::move(env)) {
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
std::vector<const OrtEpDevice*> Environment::SelectEpDevicesForDeviceType(
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

EpWorkarounds Environment::GetEpWorkarounds(mojom::Device device_type) const {
  EpWorkarounds workarounds;
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  base::span<const OrtEpDevice* const> registered_ep_devices =
      GetRegisteredEpDevices(ort_api, this->get());
  std::vector<const OrtEpDevice*> selected_ep_devices =
      SelectEpDevicesForDeviceType(registered_ep_devices, device_type);
  for (const auto* ep_device : selected_ep_devices) {
    CHECK(ep_device);
      const char* ep_name = ort_api->EpDevice_EpName(ep_device);
      // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
      const auto iter =
          kKnownEPs.find(UNSAFE_BUFFERS(base::cstring_view(ep_name)));
      if (iter != kKnownEPs.end()) {
        workarounds |= iter->second.workarounds;
      }
  }
  return workarounds;
}

std::vector<Environment::SessionConfigEntry> Environment::GetEpConfigEntries(
    mojom::Device device_type) const {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  base::span<const OrtEpDevice* const> registered_ep_devices =
      GetRegisteredEpDevices(ort_api, this->get());
  std::vector<const OrtEpDevice*> selected_ep_devices =
      SelectEpDevicesForDeviceType(registered_ep_devices, device_type);
  std::vector<SessionConfigEntry> ep_config_entries;
  // Track processed EP names to avoid duplicates.
  std::set<base::cstring_view> processed_ep_names;

  for (const auto* ep_device : selected_ep_devices) {
    CHECK(ep_device);


    const char* ep_name = ort_api->EpDevice_EpName(ep_device);
    // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
    base::cstring_view ep_name_view =
        UNSAFE_BUFFERS(base::cstring_view(ep_name));

    // Skip if we've already processed this EP
    if (processed_ep_names.contains(ep_name_view)) {
      continue;
    }
    processed_ep_names.insert(ep_name_view);

    const auto& ep_it = kKnownEPs.find(ep_name_view);
    if (ep_it == kKnownEPs.end()) {
      continue;
    }

    for (const auto& config_entry : ep_it->second.config_entries) {
      ep_config_entries.push_back(config_entry);
    }
  }

  return ep_config_entries;
}

// static
base::Lock& Environment::GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

raw_ptr<Environment> Environment::instance_ = nullptr;

}  // namespace webnn::ort
