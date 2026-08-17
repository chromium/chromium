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
  constexpr size_t k1MB = 1 << 20;
  constexpr size_t k512KB = 512 << 10;
  constexpr size_t k256KB = 256 << 10;
  constexpr size_t k64KB = 64 << 10;

  // Determine the correct parameters based on the process type.
  switch (process_type) {
    case PROCESS_TYPE_RENDERER:
      // Chrome telemetry shows RendererMetrics reaching 50-55% of the previous
      // 2 MiB region at p99.9 on Windows. Use 1.5 MiB to retain headroom while
      // reducing per-renderer commit charge.
      return Config{PROCESS_TYPE_RENDERER, "RendererMetrics", k1MB + k512KB};

    case PROCESS_TYPE_UTILITY:
      return Config{PROCESS_TYPE_UTILITY, "UtilityMetrics", k512KB};

    case PROCESS_TYPE_ZYGOTE:
      return Config{PROCESS_TYPE_ZYGOTE, "ZygoteMetrics", k64KB};

    case PROCESS_TYPE_SANDBOX_HELPER:
      return Config{PROCESS_TYPE_SANDBOX_HELPER, "SandboxHelperMetrics", k64KB};

    case PROCESS_TYPE_GPU:
      return Config{PROCESS_TYPE_GPU, "GpuMetrics", k256KB};

    default:
      return std::nullopt;
  }
}

}  // namespace content
