// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_
#define GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_

#include <string>

#include "third_party/dawn/include/dawn/webgpu.h"

namespace gpu {

bool IsWebGPUAdapterBlocklisted(const WGPUAdapterProperties& properties,
                                const std::string& blocklist_string);

}  // namespace gpu

#endif  // GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_
