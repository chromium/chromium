// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/chrome_ml.h"

#include <string_view>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
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
    "on_device_model_gpu_block_list", ""};

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
  return std::make_unique<ChromeML>(base::PassKey<ChromeML>(),
                                    std::move(scoped_library), api);
}

bool ChromeML::IsGpuBlocked() const {
  GpuConfig gpu_config;
  if (!api().GetGpuConfig(gpu_config)) {
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
    LOG(ERROR) << "WebGPU blocked on this device";
    return true;
  }
  return false;
}

}  // namespace ml
