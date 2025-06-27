// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_provider_ort.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
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
  base::cstring_view name;
  base::wcstring_view package_family_name;
  base::wcstring_view library_name;
  PACKAGE_VERSION package_version;
  // Represents the vendor id of the hardware device used by the execution
  // provider.
  uint32_t vendor_id;
};

constexpr EpInfo kKnownEPs[] = {
    {.name = "OpenVINOExecutionProvider",
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
     .vendor_id = 0x8086},
};

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

bool IsExecutionProviderRegistered(const OrtApi* ort_api,
                                   OrtEnv* env,
                                   base::cstring_view ep_name) {
  size_t num_ep_devices = 0;
  const OrtEpDevice* const* ep_devices = nullptr;
  CHECK_STATUS(ort_api->GetEpDevices(env, &ep_devices, &num_ep_devices));

  // SAFETY: ORT guarantees that `ep_devices` is valid and contains
  // `num_ep_devices` elements.
  base::span<const OrtEpDevice* const> ep_devices_span =
      UNSAFE_BUFFERS(base::span(ep_devices, num_ep_devices));
  for (const OrtEpDevice* ep_device : ep_devices_span) {
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
  for (const auto& ep_info : kKnownEPs) {
    bool vendor_matched = VendorIdExistsInGpuInfo(gpu_info, ep_info.vendor_id);
    if (!vendor_matched) {
      continue;
    }

    bool ep_registered =
        IsExecutionProviderRegistered(ort_api, env.get(), ep_info.name);
    if (ep_registered) {
      continue;
    }

    const std::optional<base::FilePath>& ep_package_path =
        platform_functions->InitializePackageDependency(
            ep_info.package_family_name, ep_info.package_version);
    if (ep_package_path) {
      CALL_ORT_FUNC(ort_api->RegisterExecutionProviderLibrary(
          env.get(), ep_info.name.c_str(),
          ep_package_path->Append(L"ExecutionProvider")
              .Append(ep_info.library_name)
              .value()
              .c_str()));
    }
  }

  ASSIGN_OR_RETURN(scoped_refptr<SessionOptions> session_options,
                   SessionOptions::Create(options->device));
  return std::make_unique<ContextImplOrt>(std::move(receiver), context_provider,
                                          std::move(options), std::move(env),
                                          std::move(session_options));
}

}  // namespace webnn::ort
