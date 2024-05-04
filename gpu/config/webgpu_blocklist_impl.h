// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_
#define GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_

#include <string>

namespace wgpu {
class Adapter;
struct AdapterProperties;
}

namespace gpu {

namespace detail {
bool IsWebGPUAdapterBlocklisted(const wgpu::AdapterProperties& properties,
                                const std::string& blocklist_string);
}  // namespace detail

bool IsWebGPUAdapterBlocklisted(const wgpu::Adapter& adapter,
                                const std::string& blocklist_string);

}  // namespace gpu

#endif  // GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_
