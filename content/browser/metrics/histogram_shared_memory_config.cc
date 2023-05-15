// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/metrics/histogram_shared_memory_config.h"

#include "content/public/common/process_type.h"

namespace content {

absl::optional<base::HistogramSharedMemoryConfig>
GetHistogramSharedMemoryConfig(int process_type) {
  using Config = base::HistogramSharedMemoryConfig;

  // Determine the correct parameters based on the process type.
  switch (process_type) {
    case PROCESS_TYPE_RENDERER:
      // Create persistent/shared memory and allow histograms to be stored in
      // it. Memory that is not actually used won't be physically mapped by the
      // system. RendererMetrics usage, as reported in UMA, peaked around 0.7MiB
      // as of 2016-12-20.
      return Config{"RendererMetrics", 2 << 20};  // 2 MiB

    case PROCESS_TYPE_UTILITY:
      return Config{"UtilityMetrics", 256 << 10};  // 256 KiB

    case PROCESS_TYPE_ZYGOTE:
      return Config{"ZygoteMetrics", 64 << 10};  // 64 KiB

    case PROCESS_TYPE_SANDBOX_HELPER:
      return Config{"SandboxHelperMetrics", 64 << 10};  // 64 KiB

    case PROCESS_TYPE_GPU:
      return Config{"GpuMetrics", 256 << 10};  // 256 KiB

    case PROCESS_TYPE_PPAPI_PLUGIN:
      return Config{"PpapiPluginMetrics", 64 << 10};  // 64 KiB

    case PROCESS_TYPE_PPAPI_BROKER:
      return Config{"PpapiBrokerMetrics", 64 << 10};  // 64 KiB

    default:
      return absl::nullopt;
  }
}

}  // namespace content
