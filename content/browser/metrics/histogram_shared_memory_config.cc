// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/metrics/histogram_shared_memory_config.h"

#include "content/public/common/process_type.h"

namespace content {

namespace {
using Config = base::HistogramSharedMemory::Config;
}

std::optional<Config> GetHistogramSharedMemoryConfig(int process_type) {
  // Memory size constants used in the configurations.
  constexpr size_t k2MB = 2 << 20;
  constexpr size_t k512KB = 512 << 10;
  constexpr size_t k256KB = 256 << 10;
  constexpr size_t k64KB = 64 << 10;

  // Determine the correct parameters based on the process type.
  switch (process_type) {
    case PROCESS_TYPE_RENDERER:
      // Create persistent/shared memory and allow histograms to be stored in
      // it. Memory that is not actually used won't be physically mapped by the
      // system. RendererMetrics usage, as reported in UMA, peaked around 0.7MiB
      // as of 2016-12-20.
      return Config{PROCESS_TYPE_RENDERER, "RendererMetrics", k2MB};

    case PROCESS_TYPE_UTILITY:
      return Config{PROCESS_TYPE_UTILITY, "UtilityMetrics", k512KB};

    case PROCESS_TYPE_ZYGOTE:
      return Config{PROCESS_TYPE_ZYGOTE, "ZygoteMetrics", k64KB};

    case PROCESS_TYPE_SANDBOX_HELPER:
      return Config{PROCESS_TYPE_SANDBOX_HELPER, "SandboxHelperMetrics", k64KB};

    case PROCESS_TYPE_GPU:
      return Config{PROCESS_TYPE_GPU, "GpuMetrics", k256KB};

    case PROCESS_TYPE_PPAPI_PLUGIN:
      return Config{PROCESS_TYPE_PPAPI_PLUGIN, "PpapiPluginMetrics", k64KB};

    case PROCESS_TYPE_PPAPI_BROKER:
      return Config{PROCESS_TYPE_PPAPI_BROKER, "PpapiBrokerMetrics", k64KB};

    default:
      return std::nullopt;
  }
}

}  // namespace content
