// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_INIT_WIN_H_
#define GPU_IPC_SERVICE_GPU_INIT_WIN_H_

#include "gpu/config/gpu_preferences.h"

namespace base {
class CommandLine;
}

namespace gl {
class GLDisplay;
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
void SetupGLDisplayManagerEGL(const GPUInfo& gpu_info,
                              const GpuFeatureInfo& gpu_feature_info);
bool MaybeEnableANGLEDebugLayer(GrContextType gr_context_type,
                                gl::GLDisplay*& gl_display);

}  // namespace gpu_init_internal

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_INIT_WIN_H_
