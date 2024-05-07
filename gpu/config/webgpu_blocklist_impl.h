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

// Bitmask of blocklist reasons.
enum class WebGPUBlocklistReason : uint64_t {
  None = 0,
  // Blocklisted by vendor/device/driver string pattern
  StringPattern = 1,
  // crbug.com/40643701:
  // Metal compiler errors for dynamic indexing of arrays in structures
  DynamicArrayIndexInStruct = 1 << 1,
  // crbug.com/42240193:
  // Indirect root constants in compute pass broken on Windows NVIDIA x86
  IndirectComputeRootConstants = 1 << 2,
  // crbug.com/42242119:
  // Not supported on Windows arm yet.
  WindowsARM = 1 << 3,
  // crbug.com/333858788:
  // OpenGLES not fully supported on Android
  AndroidGLES = 1 << 4,
  // crbug.com/40643150:
  // Limited support / testing currently available on Android
  AndroidLimitedSupport = 1 << 5,
  // b/331922614:
  // AMD driver doesn't support VK_EXT_image_drm_format_modifier
  AMDMissingDrmFormatModifier = 1 << 6,
  // crbug.com/40057808:
  // CPU adapters not fully tested or conformant.
  CPUAdapter = 1 << 7,
  // crbug.com/41479539:
  // D3D11 backend not fully implemented.
  D3D11 = 1 << 8,
  // crbug.com/42250788:
  // Invalid consteval interpretation of 22nd bit on Windows x86 with SM 6.0+.
  Consteval22ndBit = 1 << 9,
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

namespace gpu {

struct WebGPUBlocklistOptions {
  std::string_view blocklist_string = "";
  WebGPUBlocklistReason ignores = WebGPUBlocklistReason::None;
};

namespace detail {
bool IsWebGPUAdapterBlocklisted(const wgpu::AdapterProperties& properties,
                                const WebGPUBlocklistOptions& options);
}  // namespace detail

bool IsWebGPUAdapterBlocklisted(const wgpu::Adapter& adapter,
                                WebGPUBlocklistOptions options = {});

}  // namespace gpu

#endif  // GPU_CONFIG_WEBGPU_BLOCKLIST_IMPL_H_
