// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_
#define GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_

#include <cstdint>
#include <string>

namespace wgpu {
class Adapter;
struct AdapterInfo;
}

// Bitmask of blocklist reasons.
enum class WebGPUBlocklistReason : uint64_t {
  None = 0,
  StringPattern = 1,
  DynamicArrayIndexInStruct = 1 << 1,
  IndirectComputeRootConstants = 1 << 2,
  WindowsARM = 1 << 3,
  AndroidGLES = 1 << 4,
  AndroidLimitedSupport = 1 << 5,
  AMDMissingDrmFormatModifier = 1 << 6,
  CPUAdapter = 1 << 7,
  D3D11 = 1 << 8,
  Consteval22ndBit = 1 << 9,
  // When adding an enum, update kKnownReasons with a description.
};

inline constexpr WebGPUBlocklistReason operator|(WebGPUBlocklistReason l,
                                                 WebGPUBlocklistReason r) {
  return static_cast<WebGPUBlocklistReason>(uint64_t(l) | uint64_t(r));
}

inline constexpr WebGPUBlocklistReason operator&(WebGPUBlocklistReason l,
                                                 WebGPUBlocklistReason r) {
  return static_cast<WebGPUBlocklistReason>(uint64_t(l) & uint64_t(r));
}

inline constexpr WebGPUBlocklistReason operator~(WebGPUBlocklistReason v) {
  return static_cast<WebGPUBlocklistReason>(~uint64_t(v));
}

inline constexpr WebGPUBlocklistReason operator&=(WebGPUBlocklistReason& l,
                                                  WebGPUBlocklistReason r) {
  l = static_cast<WebGPUBlocklistReason>(uint64_t(l) & uint64_t(r));
  return static_cast<WebGPUBlocklistReason>(uint64_t(l));
}

namespace gpu {

struct WebGPUBlocklistResultImpl {
  bool blocked;
  std::string reason;
};

struct WebGPUBlocklistOptions {
  std::string_view blocklist_string = "";
  WebGPUBlocklistReason ignores = WebGPUBlocklistReason::None;
};

namespace detail {
WebGPUBlocklistReason GetWebGPUAdapterBlocklistReason(
    const wgpu::AdapterInfo& info,
    const WebGPUBlocklistOptions& options);
}  // namespace detail

WebGPUBlocklistResultImpl IsWebGPUAdapterBlocklisted(
    const wgpu::Adapter& adapter,
    WebGPUBlocklistOptions options = {});

}  // namespace gpu

#endif  // GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_
