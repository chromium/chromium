// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/chrome_ml.h"

#include <string_view>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#include "gpu/config/webgpu_blocklist_impl.h"
#include "services/on_device_model/ml/on_device_model_executor.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#endif

namespace ml {

namespace {

constexpr std::string_view kChromeMLLibraryName = "optimization_guide_internal";

const base::FeatureParam<std::string> kGpuBlockList{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_gpu_block_list",
    // These devices are nearly always crashing or have very low performance.
    "8086:412|8086:a16|8086:41e|8086:416|8086:402|8086:166|8086:1616|8086:22b1|"
    "8086:22b0|1414:8c|8086:*:*31.0.101.4824*|8086:*:*31.0.101.4676*"};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuBlockedReason {
  kGpuConfigError = 0,
  kBlocklisted = 1,
  kBlocklistedForCpuAdapter = 2,
  kNotBlocked = 3,
  kMaxValue = kNotBlocked,
};

void LogGpuBlocked(GpuBlockedReason reason) {
  base::UmaHistogramEnumeration("OnDeviceModel.GpuBlockedReason", reason);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuErrorReason {
  kOther = 0,
  kDxgiErrorDeviceHung = 1,
  kDxgiErrorDeviceRemoved = 2,
  kMaxValue = kDxgiErrorDeviceRemoved,
};

void FatalErrorFn(const char* msg) {
  SCOPED_CRASH_KEY_STRING1024("ChromeML", "error_msg", msg);
  std::string msg_str(msg);
  GpuErrorReason error_reason = GpuErrorReason::kOther;
  if (msg_str.find("DXGI_ERROR_DEVICE_HUNG") != std::string::npos) {
    error_reason = GpuErrorReason::kDxgiErrorDeviceHung;
  } else if (msg_str.find("DXGI_ERROR_DEVICE_REMOVED") != std::string::npos) {
    error_reason = GpuErrorReason::kDxgiErrorDeviceRemoved;
  }
  base::UmaHistogramEnumeration("OnDeviceModel.GpuErrorReason", error_reason);
  if (error_reason == GpuErrorReason::kOther) {
    // Collect crash reports on unknown errors.
    CHECK(false) << "ChromeML Error: " << msg;
  } else {
    base::Process::TerminateCurrentProcessImmediately(0);
  }
}

}  // namespace

ChromeML::ChromeML(base::PassKey<ChromeML>,
                   base::ScopedNativeLibrary library,
                   const ChromeMLAPI* api)
    : library_(std::move(library)), api_(api) {
  CHECK(api_);
}

ChromeML::~ChromeML() = default;

// static
ChromeML* ChromeML::Get() {
  static base::NoDestructor<std::unique_ptr<ChromeML>> chrome_ml{Create()};
  return chrome_ml->get();
}

// static
DISABLE_CFI_DLSYM
std::unique_ptr<ChromeML> ChromeML::Create() {
  // Log GPU info for crash reports.
  gpu::GPUInfo gpu_info;
  gpu::CollectBasicGraphicsInfo(&gpu_info);
  gpu::SetKeysForCrashLogging(gpu_info);

  base::NativeLibraryLoadError error;
  base::FilePath base_dir;
#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_MAC)
  if (base::apple::AmIBundled()) {
    base_dir = base::apple::FrameworkBundlePath().Append("Libraries");
  } else {
#endif  // BUILDFLAG(IS_MAC)
    CHECK(base::PathService::Get(base::DIR_MODULE, &base_dir));
#if BUILDFLAG(IS_MAC)
  }
#endif  // BUILDFLAG(IS_MAC)
#endif  // !BUILDFLAG(IS_ANDROID)
  base::NativeLibrary library = base::LoadNativeLibrary(
      base_dir.AppendASCII(base::GetNativeLibraryName(kChromeMLLibraryName)),
      &error);
  if (!library) {
    LOG(ERROR) << "Error loading native library: " << error.ToString();
    return {};
  }

  base::ScopedNativeLibrary scoped_library(library);
  auto get_api = reinterpret_cast<ChromeMLAPIGetter>(
      scoped_library.GetFunctionPointer("GetChromeMLAPI"));
  if (!get_api) {
    LOG(ERROR) << "Unable to resolve GetChromeMLAPI() symbol.";
    return {};
  }

  const ChromeMLAPI* api = get_api();
  if (!api) {
    return {};
  }

  api->InitDawnProcs(dawn::native::GetProcs());
  if (api->SetFatalErrorFn) {
    api->SetFatalErrorFn(&FatalErrorFn);
  }
  return std::make_unique<ChromeML>(base::PassKey<ChromeML>(),
                                    std::move(scoped_library), api);
}

DISABLE_CFI_DLSYM
bool ChromeML::IsGpuBlocked() const {
  GpuConfig gpu_config;
  if (!api().GetGpuConfig(gpu_config)) {
    LogGpuBlocked(GpuBlockedReason::kGpuConfigError);
    LOG(ERROR) << "Unable to get gpu config";
    return true;
  }
  WGPUAdapterProperties wgpu_adapter_properties = {};
  wgpu_adapter_properties.vendorID = gpu_config.vendor_id;
  wgpu_adapter_properties.deviceID = gpu_config.device_id;
  wgpu_adapter_properties.architecture = gpu_config.architecture;
  wgpu_adapter_properties.driverDescription = gpu_config.driver_description;
  wgpu_adapter_properties.adapterType = gpu_config.adapter_type;
  wgpu_adapter_properties.backendType = gpu_config.backend_type;
  if (gpu::IsWebGPUAdapterBlocklisted(wgpu_adapter_properties,
                                      kGpuBlockList.Get())) {
    if (gpu_config.adapter_type == WGPUAdapterType_CPU) {
      LogGpuBlocked(GpuBlockedReason::kBlocklistedForCpuAdapter);
    } else {
      LogGpuBlocked(GpuBlockedReason::kBlocklisted);
    }
    LOG(ERROR) << "WebGPU blocked on this device";
    return true;
  }
  LogGpuBlocked(GpuBlockedReason::kNotBlocked);
  return false;
}

}  // namespace ml
