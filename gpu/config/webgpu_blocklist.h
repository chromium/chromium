// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_WEBGPU_BLOCKLIST_H_
#define GPU_CONFIG_WEBGPU_BLOCKLIST_H_

#include <string>

#include "gpu/gpu_export.h"

struct WGPUAdapterProperties;

namespace gpu {

GPU_EXPORT bool IsWebGPUAdapterBlocklisted(const WGPUAdapterProperties&);
GPU_EXPORT bool IsWebGPUAdapterBlocklisted(const WGPUAdapterProperties&,
                                           const std::string& blocklist);

}  // namespace gpu

#endif  // GPU_CONFIG_WEBGPU_BLOCKLIST_H_
