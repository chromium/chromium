// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/gpu_blocklist.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "gpu/config/webgpu_blocklist_impl.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace ml {

namespace {

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

DISABLE_CFI_DLSYM
GpuBlockedReason IsGpuBlockedInternal(const ChromeMLAPI& api) {
  struct QueryData {
    bool blocklisted;
    bool is_blocklisted_cpu_adapter;
  };

  QueryData query_data;
  if (!api.QueryGPUAdapter(
          [](WGPUAdapter cAdapter, void* data) {
            wgpu::Adapter adapter(cAdapter);
            auto* query_data = static_cast<QueryData*>(data);

            query_data->blocklisted =
                gpu::IsWebGPUAdapterBlocklisted(
                    adapter,
                    {
                        .blocklist_string = kGpuBlockList.Get(),
                        .ignores = WebGPUBlocklistReason::
                                       IndirectComputeRootConstants |
                                   WebGPUBlocklistReason::Consteval22ndBit |
                                   WebGPUBlocklistReason::WindowsARM,
                    })
                    .blocked;
            if (query_data->blocklisted) {
              wgpu::AdapterInfo info;
              adapter.GetInfo(&info);
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
