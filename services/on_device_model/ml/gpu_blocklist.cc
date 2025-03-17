// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/gpu_blocklist.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "gpu/config/webgpu_blocklist_impl.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"

#if !BUILDFLAG(IS_IOS)
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#endif

namespace ml {

namespace {

const base::FeatureParam<std::string> kGpuBlockList{
    &optimization_guide::features::kOnDeviceModelPerformanceParams,
    "on_device_model_gpu_block_list",
    // These devices are nearly always crashing or have very low performance.
    "8086:412|8086:a16|8086:41e|8086:416|8086:402|8086:166|8086:1616|8086:22b1|"
    "8086:22b0|8086:1916|8086:5a84|8086:5a85|1414:8c|8086:*:*31.0.101.4824*|"
    "8086:*:*31.0.101.4676*"};

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

DISABLE_CFI_DLSYM
GpuBlockedReason IsGpuBlockedInternal(const ChromeMLAPI& api) {
  struct QueryData {
    bool blocklisted;
    bool is_blocklisted_cpu_adapter;
  };

  static crash_reporter::CrashKeyString<256> blocklist_key(
      "ChromeML-blocklist");
  blocklist_key.Set(kGpuBlockList.Get());

  constexpr WebGPUBlocklistReason kIgnoreReasons =
      WebGPUBlocklistReason::IndirectComputeRootConstants |
      WebGPUBlocklistReason::Consteval22ndBit |
      WebGPUBlocklistReason::WindowsARM;

#if !BUILDFLAG(IS_IOS)
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
      return GpuBlockedReason::kBlocklisted;
    }
  }
#endif

  QueryData query_data;
  if (!api.QueryGPUAdapter(
          [](WGPUAdapter cAdapter, void* data) {
            wgpu::Adapter adapter(cAdapter);
            auto* query_data = static_cast<QueryData*>(data);

            wgpu::AdapterInfo info;
            adapter.GetInfo(&info);
            static crash_reporter::CrashKeyString<32> device_key(
                "ChromeML-device");
            device_key.Set(base::NumberToString(info.vendorID) + ":" +
                           base::NumberToString(info.deviceID));

            query_data->blocklisted =
                gpu::IsWebGPUAdapterBlocklisted(
                    adapter,
                    {
                        .blocklist_string = kGpuBlockList.Get(),
                        .ignores = kIgnoreReasons,
                    })
                    .blocked;
            if (query_data->blocklisted) {
              query_data->is_blocklisted_cpu_adapter =
                  info.adapterType == wgpu::AdapterType::CPU;
            }
          },
          &query_data)) {
    LOG(ERROR) << "Unable to get gpu adapter";
    return GpuBlockedReason::kGpuConfigError;
  }

  if (query_data.blocklisted) {
    LOG(ERROR) << "WebGPU blocked on this device";
    if (query_data.is_blocklisted_cpu_adapter) {
      return GpuBlockedReason::kBlocklistedForCpuAdapter;
    }
    return GpuBlockedReason::kBlocklisted;
  }
  return GpuBlockedReason::kNotBlocked;
}

}  // namespace

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
bool IsGpuBlocked(const ChromeMLAPI& api) {
  auto reason = IsGpuBlockedInternal(api);
  LogGpuBlocked(reason);
  return reason != GpuBlockedReason::kNotBlocked;
}

}  // namespace ml
