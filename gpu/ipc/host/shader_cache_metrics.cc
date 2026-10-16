// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/shader_cache_metrics.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"

namespace gpu {

void RecordShaderCacheDiskSize(std::string_view cache_name,
                               const base::FilePath& cache_dir) {
  if (!base::DirectoryExists(cache_dir)) {
    return;
  }

  const int64_t size_kb = base::ComputeDirectorySize(cache_dir) / 1024;
  // 1 KB to 64 GB. The upper bound is deliberately large because unbounded
  // cache growth is one of the things this metric is meant to catch.
  base::UmaHistogramCustomCounts(
      base::StrCat({"GPU.ShaderCache.DiskSize.", cache_name}),
      base::saturated_cast<int>(size_kb), /*min=*/1,
      /*exclusive_max=*/64 * 1024 * 1024, /*buckets=*/100);
}

}  // namespace gpu
