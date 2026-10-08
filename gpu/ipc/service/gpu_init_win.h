// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_INIT_WIN_H_
#define GPU_IPC_SERVICE_GPU_INIT_WIN_H_

namespace base {
class CommandLine;
}

namespace gpu {

class DawnContextProvider;
struct GpuFeatureInfo;
struct GPUInfo;
struct GpuPreferences;

namespace gpu_init_internal {

bool PreloadDLLs(const base::CommandLine* command_line,
                 const GpuPreferences& gpu_preferences);
void InitializeDirectComposition(DawnContextProvider* dawn_context_provider);
void InitializeOverlaySettings(GPUInfo* gpu_info,
                               const GpuFeatureInfo& gpu_feature_info);

}  // namespace gpu_init_internal

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_INIT_WIN_H_
