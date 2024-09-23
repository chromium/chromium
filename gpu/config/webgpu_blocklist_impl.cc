// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/webgpu_blocklist_impl.h"

#include <sstream>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace gpu {

namespace detail {

WebGPUBlocklistReason GetWebGPUAdapterBlocklistReason(
    const wgpu::AdapterInfo& info,
    const WebGPUBlocklistOptions& options) {
  WebGPUBlocklistReason reason = WebGPUBlocklistReason::None;
#if BUILDFLAG(IS_MAC)
  constexpr uint32_t kAMDVendorID = 0x1002;
  if (base::mac::MacOSMajorVersion() < 13 && info.vendorID == kAMDVendorID &&
      info.backendType == wgpu::BackendType::Metal) {
    reason = reason | WebGPUBlocklistReason::DynamicArrayIndexInStruct;
  }
#endif

  if (info.backendType == wgpu::BackendType::D3D12) {
#if defined(ARCH_CPU_X86)
    constexpr uint32_t kNVIDIAVendorID = 0x10de;
    if (info.vendorID == kNVIDIAVendorID) {
      reason = reason | WebGPUBlocklistReason::IndirectComputeRootConstants;
    }
#endif  // defined(ARCH_CPU_X86)
#if defined(ARCH_CPU_ARM_FAMILY)
    reason = reason | WebGPUBlocklistReason::WindowsARM;
#endif  // defined(ARCH_CPU_ARM_FAMILY)
  }

#if BUILDFLAG(IS_ANDROID)
  if (info.backendType == wgpu::BackendType::OpenGLES) {
    reason = reason | WebGPUBlocklistReason::AndroidGLES;
  }

  constexpr uint32_t kARMVendorID = 0x13B5;
  constexpr uint32_t kQualcommVendorID = 0x5143;
  const auto* build_info = base::android::BuildInfo::GetInstance();
  // Only Android 12 with an ARM or Qualcomm GPU is enabled for initially.
  // Other OS versions and GPU vendors may be fine, but have not had
  // sufficient testing yet.
  if (build_info->sdk_int() < base::android::SDK_VERSION_S ||
      (info.vendorID != kARMVendorID && info.vendorID != kQualcommVendorID)) {
    reason = reason | WebGPUBlocklistReason::AndroidLimitedSupport;
  }

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  constexpr uint32_t kAMDVendorID = 0x1002;
  if (info.vendorID == kAMDVendorID && info.deviceID == 0x98e4) {
    reason = reason | WebGPUBlocklistReason::AMDMissingDrmFormatModifier;
  }
#endif

  if (info.adapterType == wgpu::AdapterType::CPU) {
    reason = reason | WebGPUBlocklistReason::CPUAdapter;
  }

  if (info.backendType == wgpu::BackendType::D3D11) {
    reason = reason | WebGPUBlocklistReason::D3D11;
  }

  for (auto* chain = info.nextInChain; chain != nullptr;
       chain = chain->nextInChain) {
    switch (chain->sType) {
      case wgpu::SType::AdapterPropertiesD3D:
#if defined(ARCH_CPU_X86)
        if (static_cast<const wgpu::AdapterPropertiesD3D*>(chain)
                ->shaderModel >= 60) {
          reason = reason | WebGPUBlocklistReason::Consteval22ndBit;
        }
#endif  // defined(ARCH_CPU_X86)
        break;
      default:
        break;
    }
  }

  auto U32ToHexString = [](uint32_t value) {
    std::ostringstream o;
    o << std::hex << value;
    return o.str();
  };

  auto blocked_patterns =
      base::SplitString(options.blocklist_string, "|", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);

  for (const auto& blocked_pattern : blocked_patterns) {
    std::vector<std::string> segments = base::SplitString(
        blocked_pattern, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    // Check if the vendorID matches the first segment.
    if (segments.size() >= 1) {
      if (!base::MatchPattern(U32ToHexString(info.vendorID), segments[0])) {
        continue;
      }
    }

    // Check if the deviceID or architecture matches the second segment.
    if (segments.size() >= 2) {
      if (!base::MatchPattern(U32ToHexString(info.deviceID), segments[1]) &&
          !base::MatchPattern(info.architecture, segments[1])) {
        continue;
      }
    }

    // Check if the driver description matches the third segment.
    if (segments.size() >= 3) {
      if (!base::MatchPattern(info.description, segments[2])) {
        continue;
      }
    }

    // Adapter is blocked.
    reason = reason | WebGPUBlocklistReason::StringPattern;
  }
  return reason;
}

std::string BlocklistReasonToString(WebGPUBlocklistReason reason) {
  std::string result;
  bool first = true;
  static constexpr std::array<
      std::pair<WebGPUBlocklistReason, std::string_view>, 10>
      kKnownReasons = {{
          {WebGPUBlocklistReason::Consteval22ndBit,
           "crbug.com/42250788: Invalid consteval interpretation of 22nd bit "
           "on Windows x86 with SM 6.0+."},
          {WebGPUBlocklistReason::D3D11,
           "crbug.com/41479539: D3D11 backend not fully implemented."},
          {WebGPUBlocklistReason::CPUAdapter,
           "crbug.com/40057808: CPU adapters not fully tested or conformant."},
          {WebGPUBlocklistReason::AMDMissingDrmFormatModifier,
           "b/331922614: AMD driver doesn't support "
           "VK_EXT_image_drm_format_modifier."},
          {WebGPUBlocklistReason::AndroidLimitedSupport,
           "crbug.com/40643150: Limited support / testing currently "
           "available on Android."},
          {WebGPUBlocklistReason::AndroidGLES,
           "crbug.com/333858788: OpenGLES not fully supported on Android."},
          {WebGPUBlocklistReason::WindowsARM,
           "crbug.com/42242119: Not supported on Windows arm yet."},
          {WebGPUBlocklistReason::IndirectComputeRootConstants,
           "crbug.com/42240193: Indirect root constants in compute pass "
           "broken on Windows NVIDIA x86."},
          {WebGPUBlocklistReason::DynamicArrayIndexInStruct,
           "crbug.com/40643701: Metal compiler errors for dynamic indexing "
           "of arrays in structures."},
          {WebGPUBlocklistReason::StringPattern,
           "Blocklisted by vendor/device/driver string pattern."},
      }};
  for (const auto& [flag, description] : kKnownReasons) {
    if ((reason & flag) != flag) {
      continue;
    }
    reason &= ~flag;
    if (!first) {
      result += " | ";
    }
    first = false;
    result += description;
  }
  // If this triggers you need to add an entry to kKnownReasons.
  CHECK(reason == WebGPUBlocklistReason::None);
  return result;
}

}  // namespace detail

WebGPUBlocklistResultImpl IsWebGPUAdapterBlocklisted(
    const wgpu::Adapter& adapter,
    WebGPUBlocklistOptions options) {
  wgpu::AdapterInfo info;
  wgpu::AdapterPropertiesD3D d3dProperties;
  if (adapter.HasFeature(wgpu::FeatureName::AdapterPropertiesD3D)) {
    info.nextInChain = &d3dProperties;
  }
  adapter.GetInfo(&info);

  auto blocklistReason = detail::GetWebGPUAdapterBlocklistReason(info, options);
  bool blocked =
      (~options.ignores & blocklistReason) != WebGPUBlocklistReason::None;
  if (!blocked) {
    return {.blocked = blocked, .reason = ""};
  }
  return {.blocked = blocked,
          .reason = detail::BlocklistReasonToString(blocklistReason)};
}

}  // namespace gpu
