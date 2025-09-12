// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/environment.h"

#include <set>
#include <sstream>
#include <string_view>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split_win.h"
#include "base/strings/utf_string_conversions.h"
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
    {
        "OpenVINOExecutionProvider",
        {
            .package_family_name =
                L"Microsoft.WindowsMLRuntime.Intel.OpenVINO.EP_8wekyb3d8bbwe",
            .library_name = L"onnxruntime_providers_openvino.dll",
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
                    .disable_external_data = true,
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
});

OrtHardwareDeviceType GetOrtHardwareDeviceType(mojom::Device device_type) {
  switch (device_type) {
    case mojom::Device::kCpu:
      return OrtHardwareDeviceType_CPU;
    case mojom::Device::kGpu:
      return OrtHardwareDeviceType_GPU;
    case mojom::Device::kNpu:
      return OrtHardwareDeviceType_NPU;
  }
}

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

// Helper function to convert a string to OrtLoggingLevel enum.
OrtLoggingLevel StringToOrtLoggingLevel(std::string_view logging_level) {
  if (logging_level == "VERBOSE") {
    return ORT_LOGGING_LEVEL_VERBOSE;
  } else if (logging_level == "INFO") {
    return ORT_LOGGING_LEVEL_INFO;
  } else if (logging_level == "WARNING") {
    return ORT_LOGGING_LEVEL_WARNING;
  } else if (logging_level == "ERROR") {
    return ORT_LOGGING_LEVEL_ERROR;
  } else if (logging_level == "FATAL") {
    return ORT_LOGGING_LEVEL_FATAL;
  }
  // Default to ERROR if the input is invalid.
  LOG(WARNING) << "[WebNN] Unrecognized logging level: " << logging_level
               << ". Default ERROR level will be used.";
  return ORT_LOGGING_LEVEL_ERROR;
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

std::string_view OrtHardwareDeviceTypeToString(
    OrtHardwareDeviceType device_type) {
  switch (device_type) {
    case OrtHardwareDeviceType_CPU:
      return "CPU";
    case OrtHardwareDeviceType_GPU:
      return "GPU";
    case OrtHardwareDeviceType_NPU:
      return "NPU";
  }
}

std::string OrtKeyValuePairsToString(
    const OrtApi* ort_api,
    const OrtKeyValuePairs* ort_key_value_pairs) {
  size_t num_entries = 0;
  const char* const* keys = nullptr;
  const char* const* values = nullptr;
  ort_api->GetKeyValuePairs(ort_key_value_pairs, &keys, &values, &num_entries);
  std::string result = "{";
  for (size_t i = 0; i < num_entries; ++i) {
    result = base::StrCat(
        {result,
         // SAFETY: ORT guarantees that `keys[i]` is valid and null-terminated.
         UNSAFE_BUFFERS(base::cstring_view(keys[i])), ": ",
         // SAFETY: ORT guarantees that `values[i]` is valid and
         // null-terminated.
         UNSAFE_BUFFERS(base::cstring_view(values[i])),
         i != num_entries - 1 ? ", " : ""});
  }
  return base::StrCat({result, "}"});
}

std::string Uint32ToHexString(uint32_t value) {
  std::stringstream ss;
  ss << "0x" << std::hex << value;
  return ss.str();
}

void LogRegisteredEpDevices(const OrtApi* ort_api, const OrtEnv* env) {
  base::span<const OrtEpDevice* const> ep_devices =
      GetRegisteredEpDevices(ort_api, env);
  int i = 0;
  for (const auto* ep_device : ep_devices) {
    CHECK(ep_device);
    std::string ep_device_info = base::StrCat(
        {"[INFO] OrtEpDevice #", base::NumberToString(i++), ": {ep_name: ",
         // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
         UNSAFE_BUFFERS(
             base::cstring_view(ort_api->EpDevice_EpName(ep_device))),
         ", ep_vendor: ",
         // SAFETY: ORT guarantees that `ep_vendor` is valid and
         // null-terminated.
         UNSAFE_BUFFERS(
             base::cstring_view(ort_api->EpDevice_EpVendor(ep_device)))});

    const OrtKeyValuePairs* ep_metadata =
        ort_api->EpDevice_EpMetadata(ep_device);
    CHECK(ep_metadata);
    base::StrAppend(
        &ep_device_info,
        {", ep_metadata: ", OrtKeyValuePairsToString(ort_api, ep_metadata)});

    const OrtKeyValuePairs* ep_options = ort_api->EpDevice_EpOptions(ep_device);
    CHECK(ep_options);
    base::StrAppend(
        &ep_device_info,
        {", ep_options: ", OrtKeyValuePairsToString(ort_api, ep_options), "}"});

    const OrtHardwareDevice* hardware_device =
        ort_api->EpDevice_Device(ep_device);
    CHECK(hardware_device);
    base::StrAppend(
        &ep_device_info,
        {", OrtHardwareDevice: {type: ",
         OrtHardwareDeviceTypeToString(
             ort_api->HardwareDevice_Type(hardware_device)),
         ", vendor: ",
         // SAFETY: ORT guarantees that `OrtHardwareDevice::vendor`
         // is valid and null-terminated.
         UNSAFE_BUFFERS(base::cstring_view(
             ort_api->HardwareDevice_Vendor(hardware_device))),
         ", vendor_id: ",
         Uint32ToHexString(ort_api->HardwareDevice_VendorId(hardware_device)),
         ", device_id: ",
         Uint32ToHexString(ort_api->HardwareDevice_DeviceId(hardware_device))});
    const OrtKeyValuePairs* device_metadata =
        ort_api->HardwareDevice_Metadata(hardware_device);
    CHECK(device_metadata);
    base::StrAppend(&ep_device_info,
                    {", device_metadata: ",
                     OrtKeyValuePairsToString(ort_api, device_metadata), "}"});
    LOG(ERROR) << ep_device_info;
  }
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

  OrtLoggingLevel ort_logging_level = ORT_LOGGING_LEVEL_ERROR;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtLoggingLevel)) {
    std::string user_logging_level =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kWebNNOrtLoggingLevel);
    ort_logging_level = StringToOrtLoggingLevel(user_logging_level);
  }

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
    LogRegisteredEpDevices(ort_api, env.get());
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

EpWorkarounds Environment::GetEpWorkarounds(mojom::Device device_type) const {
  EpWorkarounds workarounds;
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  base::span<const OrtEpDevice* const> ep_devices =
      GetRegisteredEpDevices(ort_api, this->get());
  OrtHardwareDeviceType ort_device_type = GetOrtHardwareDeviceType(device_type);
  for (const auto* ep_device : ep_devices) {
    CHECK(ep_device);
    OrtHardwareDeviceType ep_device_type =
        ort_api->HardwareDevice_Type(ort_api->EpDevice_Device(ep_device));
    // Check the workarounds when the EP device type matches the selected device
    // type, or is CPU because CPU EPs might be selected by ORT as the fallback
    // EP.
    if (ep_device_type == ort_device_type ||
        ep_device_type == OrtHardwareDeviceType_CPU) {
      const char* ep_name = ort_api->EpDevice_EpName(ep_device);
      // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
      const auto iter =
          kKnownEPs.find(UNSAFE_BUFFERS(base::cstring_view(ep_name)));
      // TODO(crbug.com/429859159): Decide the workarounds according to the EPs
      // that will be actually selected.
      if (iter != kKnownEPs.end()) {
        workarounds |= iter->second.workarounds;
      }
    }
  }
  return workarounds;
}

// static
base::Lock& Environment::GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

raw_ptr<Environment> Environment::instance_ = nullptr;

std::vector<Environment::SessionConfigEntry> Environment::GetEpConfigEntries(
    mojom::Device device_type) const {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  base::span<const OrtEpDevice* const> ep_devices =
      GetRegisteredEpDevices(ort_api, this->get());
  std::vector<SessionConfigEntry> ep_config_entries;
  // Track processed EP names to avoid duplicates.
  std::set<base::cstring_view> processed_ep_names;
  OrtHardwareDeviceType requested_device_type =
      GetOrtHardwareDeviceType(device_type);

  for (const auto* ep_device : ep_devices) {
    CHECK(ep_device);

    // Only look for configuration when the requested device type matches
    // the registered EP device type.
    OrtHardwareDeviceType registered_device_type =
        ort_api->HardwareDevice_Type(ort_api->EpDevice_Device(ep_device));
    if (registered_device_type != requested_device_type) {
      continue;
    }

    const char* ep_name = ort_api->EpDevice_EpName(ep_device);
    if (!ep_name) {
      continue;
    }

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

}  // namespace webnn::ort
