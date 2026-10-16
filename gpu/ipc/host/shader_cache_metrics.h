// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_HOST_SHADER_CACHE_METRICS_H_
#define GPU_IPC_HOST_SHADER_CACHE_METRICS_H_

#include <string_view>

#include "build/build_config.h"

namespace base {
class FilePath;
}  // namespace base

namespace gpu {

// Whether the GPU.ShaderCache.DiskSize histograms are recorded. They are only
// needed on Windows for now, for the investigation of GPU startup hangs in
// crbug.com/571158507. Limiting them to Windows also avoids skew from Android,
// where the browser process is often killed in the background and so would
// report many more samples per user.
inline constexpr bool kRecordShaderCacheDiskSize = BUILDFLAG(IS_WIN);

// Records the total on-disk size of `cache_dir` (including subdirectories) to
// the "GPU.ShaderCache.DiskSize.<cache_name>" histogram. Nothing is recorded if
// `cache_dir` does not exist. This performs blocking file I/O.
void RecordShaderCacheDiskSize(std::string_view cache_name,
                               const base::FilePath& cache_dir);

}  // namespace gpu

#endif  // GPU_IPC_HOST_SHADER_CACHE_METRICS_H_
