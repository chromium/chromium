// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/environment.h"

#include <string_view>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/strings/cstring_view.h"
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
  // Indicates whether the execution provider supports in-memory external data.
  // TODO(crbug.com/429253567): Specify the minimum package version that
  // supports in-memory external data.
  bool is_external_data_supported;
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
            .is_external_data_supported = false,
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

}  // namespace

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

  // Get the ORT EP library path specified by `kWebNNOrtEpLibraryPathForTesting`
  // switch for testing development EP build.
  std::optional<base::FilePath> specified_ep_path;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtEpLibraryPathForTesting)) {
    base::FilePath base_path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kWebNNOrtEpLibraryPathForTesting);
    if (base_path.empty()) {
      return base::unexpected(
          "The specified ONNX Runtime EP library path is empty.");
    }
    specified_ep_path = base_path;
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
    // `kWebNNOrtEpLibraryPathForTesting` switch. Otherwise, try to load it from
    // the EP package path.
    base::FilePath ep_library_path;
    if (specified_ep_path) {
      ep_library_path = specified_ep_path->Append(ep_info.library_name);
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

  return base::MakeRefCounted<Environment>(base::PassKey<Environment>(),
                                           std::move(env));
}

Environment::Environment(base::PassKey<Environment> /*pass_key*/,
                         ScopedOrtEnv env)
    : env_(std::move(env)) {}

Environment::~Environment() = default;

// Some EPs like OpenVINO EP haven't supported in-memory external weights in
// model yet and will throw error during session creation if it's used, so we
// have to disable this feature for these EPs.
// TODO(crbug.com/428740146): Remove this workaround once in-memory external
// data is well supported.
bool Environment::IsExternalDataSupported(mojom::Device device_type) const {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  base::span<const OrtEpDevice* const> ep_devices =
      GetRegisteredEpDevices(ort_api, this->get());
  OrtHardwareDeviceType ort_device_type = GetOrtHardwareDeviceType(device_type);
  for (const auto* ep_device : ep_devices) {
    CHECK(ep_device);
    if (ort_api->HardwareDevice_Type(ort_api->EpDevice_Device(ep_device)) ==
        ort_device_type) {
      const char* ep_name = ort_api->EpDevice_EpName(ep_device);
      // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
      const auto& iter =
          kKnownEPs.find(UNSAFE_BUFFERS(base::cstring_view(ep_name)));
      // TODO(crbug.com/429859159): Decide whether the external data is
      // supported according to the first found EP once the EP devices returned
      // from `GetEpDevices()` are sorted in the selection order.
      if (iter != kKnownEPs.end() && !iter->second.is_external_data_supported) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace webnn::ort
