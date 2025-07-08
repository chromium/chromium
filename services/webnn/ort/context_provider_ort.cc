// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_provider_ort.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/strings/cstring_view.h"
#include "base/types/expected_macros.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
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

bool IsExternalDataSupported(const OrtApi* ort_api,
                             const OrtEnv* env,
                             mojom::Device webnn_device_type) {
  base::span<const OrtEpDevice* const> ep_devices =
      GetRegisteredEpDevices(ort_api, env);
  OrtHardwareDeviceType ort_device_type =
      GetOrtHardwareDeviceType(webnn_device_type);
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

}  // namespace

base::expected<std::unique_ptr<WebNNContextImpl>, mojom::ErrorPtr>
CreateContextFromOptions(mojom::CreateContextOptionsPtr options,
                         const gpu::GPUInfo& gpu_info,
                         mojo::PendingReceiver<mojom::WebNNContext> receiver,
                         WebNNContextProviderImpl* context_provider) {
  auto* platform_functions = PlatformFunctions::GetInstance();
  if (!platform_functions) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError,
        "WebNN is not supported in this ONNX Runtime version."));
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
  // `OrtEnv` is reference counted. The first `CreateEnv()` will create the
  // `OrtEnv` instance. The following invocations return a reference to the
  // same instance. It is released upon the last reference is removed via
  // `ReleaseEnv()`.
  // Creating and releasing `OrtEnv` are protected by a lock internally, so it
  // is sequence bound.
  ScopedOrtEnv env;
  if (ORT_CALL_FAILED(ort_api->CreateEnv(ort_logging_level, "WebNN",
                                         ScopedOrtEnv::Receiver(env).get()))) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "Failed to create the ONNX Runtime environment."));
  }

  // Register the execution provider based on the GPU/NPU vendor id if it's not
  // registered yet. Ultimately, ignore the failure of registering the EP.
  //
  // TODO(crbug.com/427242324): Add a flag to register an EP from location
  // passed in command line for testing development EP build.
  for (const auto& [ep_name, ep_info] : kKnownEPs) {
    if (!VendorIdExistsInGpuInfo(gpu_info, ep_info.vendor_id)) {
      continue;
    }

    if (IsExecutionProviderRegistered(ort_api, env.get(), ep_name)) {
      continue;
    }

    const std::optional<base::FilePath>& ep_package_path =
        platform_functions->InitializePackageDependency(
            ep_info.package_family_name, ep_info.package_version);
    if (ep_package_path) {
      CALL_ORT_FUNC(ort_api->RegisterExecutionProviderLibrary(
          env.get(), ep_name.c_str(),
          ep_package_path->Append(L"ExecutionProvider")
              .Append(ep_info.library_name)
              .value()
              .c_str()));
    }
  }

  // Some EPs like OpenVINO EP haven't supported in-memory external weights in
  // model yet and will throw error during session creation if it's used, so we
  // have to disable this feature for these EPs.
  // TODO(crbug.com/428740146): Remove this workaround once in-memory external
  // data is well supported.
  bool is_external_data_supported =
      IsExternalDataSupported(ort_api, env.get(), options->device);

  ASSIGN_OR_RETURN(scoped_refptr<SessionOptions> session_options,
                   SessionOptions::Create(options->device));
  return std::make_unique<ContextImplOrt>(
      std::move(receiver), context_provider, std::move(options), std::move(env),
      std::move(session_options), is_external_data_supported);
}

}  // namespace webnn::ort
