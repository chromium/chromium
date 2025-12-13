// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/gpu_blocklist.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#include "gpu/config/webgpu_blocklist_impl.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace ml {

namespace {

const base::FeatureParam<std::string> kGpuBlockList{
    &optimization_guide::features::kOnDeviceModelPerformanceParams,
    "on_device_model_gpu_block_list",
    // These devices are nearly always crashing or have very low performance.
#if BUILDFLAG(IS_LINUX)
    "8086:64a0|8086:e20b|"  // TODO(b/456603738): Remove when fixed.
#endif  // BUILDFLAG(IS_LINUX)
    "8086:412|8086:a16|8086:41e|8086:416|8086:402|8086:166|8086:1616|8086:22b1|"
    "8086:22b0|8086:1916|8086:5a84|8086:5a85|8086:416|1414:8c|"
    "8086:*:*31.0.101.4824*|8086:*:*31.0.101.4676*|8086:*:*20.19.15.4835*|"
    "8086:*:*25.20.100.*|8086:*:*26.20.100.*|8086:*:*27.20.100.*|"
    "8086:*:*30.0.101.3111*|8086:*:*31.0.101.4826*|8086:*:*31.0.101.4672*"};

void LogGpuBlocked(GpuBlockedReason reason) {
  base::UmaHistogramEnumeration("OnDeviceModel.GpuBlockedReason", reason);
}

DISABLE_CFI_DLSYM
DeviceInfo QueryDeviceInfoInternal(const ChromeMLAPI& api) {
  static crash_reporter::CrashKeyString<256> blocklist_key(
      "ChromeML-blocklist");
  blocklist_key.Set(kGpuBlockList.Get());

  constexpr WebGPUBlocklistReason kIgnoreReasons =
      WebGPUBlocklistReason::IndirectComputeRootConstants |
      WebGPUBlocklistReason::Consteval22ndBit |
      WebGPUBlocklistReason::QualcommWindows |
      WebGPUBlocklistReason::StringPatternQualcommWindows;

  // Take a first pass at checking the blocklist. Creating a wgpu::Adapter can
  // crash in some situations, so use gpu::GPUInfo to avoid this. Using
  // wgpu::Adapter should be more accurate, so also check that later.
  gpu::GPUInfo gpu_info;
  gpu::CollectBasicGraphicsInfo(&gpu_info);
  const gpu::GPUInfo::GPUDevice* device =
      gpu_info.GetGpuByPreference(gl::GpuPreference::kHighPerformance);
  if (!device) {
    device = &gpu_info.active_gpu();
  }
  DeviceInfo query_device_info;
  if (device->IsSoftwareRenderer()) {
    query_device_info.gpu_blocked_reason = GpuBlockedReason::kBlocklisted;
    return query_device_info;
  }
  if (device) {
    WGPUAdapterInfo adapter_info = {
        .description = {device->driver_version.c_str(),
                        device->driver_version.size()},
        .vendorID = device->vendor_id,
        .deviceID = device->device_id,
    };
    if (gpu::IsWebGPUAdapterBlocklisted(
            reinterpret_cast<const wgpu::AdapterInfo&>(adapter_info),
            {
                .blocklist_string = kGpuBlockList.Get(),
                .ignores = kIgnoreReasons,
            })
            .blocked) {
      query_device_info.gpu_blocked_reason = GpuBlockedReason::kBlocklisted;
      return query_device_info;
    }
  }

  if (!api.QueryGPUAdapter(
          [](WGPUAdapter cAdapter, void* data) {
            wgpu::Adapter adapter(cAdapter);
            auto* device_info = static_cast<DeviceInfo*>(data);
            device_info->supports_fp16 =
                adapter.HasFeature(wgpu::FeatureName::ShaderF16);
            wgpu::AdapterInfo info;
            adapter.GetInfo(&info);
            device_info->vendor_id = info.vendorID;
            device_info->device_id = info.deviceID;
            device_info->driver_version = info.description;
            static crash_reporter::CrashKeyString<32> device_key(
                "ChromeML-device");
            device_key.Set(base::NumberToString(info.vendorID) + ":" +
                           base::NumberToString(info.deviceID));
            if (gpu::IsWebGPUAdapterBlocklisted(
                    adapter,
                    {
                        .blocklist_string = kGpuBlockList.Get(),
                        .ignores = kIgnoreReasons,
                    })
                    .blocked) {
              device_info->gpu_blocked_reason = GpuBlockedReason::kBlocklisted;
            }
            if (device_info->gpu_blocked_reason ==
                    GpuBlockedReason::kBlocklisted &&
                info.adapterType == wgpu::AdapterType::CPU) {
              device_info->gpu_blocked_reason =
                  GpuBlockedReason::kBlocklistedForCpuAdapter;
            }
          },
          &query_device_info)) {
    LOG(ERROR) << "Unable to get gpu adapter";
    return query_device_info;
  }
  query_device_info.gpu_blocked_reason = GpuBlockedReason::kNotBlocked;
  return query_device_info;
}

}  // namespace

BASE_FEATURE(kOnDeviceModelAllowGpuForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
DeviceInfo QueryDeviceInfo(const ChromeMLAPI& api, bool log_histogram) {
  if (base::FeatureList::IsEnabled(kOnDeviceModelAllowGpuForTesting)) {
    // Each test can use its own override. Don't use cache.
    DeviceInfo query_device_info;
    query_device_info.gpu_blocked_reason = GpuBlockedReason::kNotBlocked;
    query_device_info.supports_fp16 = true;
    return query_device_info;
  }

  static base::NoDestructor<DeviceInfo> cache;
  if (cache->gpu_blocked_reason == GpuBlockedReason::kGpuConfigError) {
    *cache = QueryDeviceInfoInternal(api);
  }
  if (log_histogram) {
    LogGpuBlocked(cache->gpu_blocked_reason);
  }
  return *cache;
}

}  // namespace ml
