// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_WEBGPU_BLOCKLIST_H_
#define GPU_CONFIG_WEBGPU_BLOCKLIST_H_

#include <string>

#include "gpu/gpu_export.h"

namespace wgpu {
class Adapter;
}

namespace gpu {

struct WebGPUBlocklistResult {
  bool blocked;
  std::string reason;
};

GPU_EXPORT WebGPUBlocklistResult
IsWebGPUAdapterBlocklisted(const wgpu::Adapter& adapter);

}  // namespace gpu

#endif  // GPU_CONFIG_WEBGPU_BLOCKLIST_H_
