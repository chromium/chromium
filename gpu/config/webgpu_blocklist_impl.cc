// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/webgpu_blocklist_impl.h"

#include <sstream>

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

bool IsWebGPUAdapterBlocklisted(const wgpu::AdapterProperties& properties,
                                const WebGPUBlocklistOptions& options) {
  WebGPUBlocklistReason reason = WebGPUBlocklistReason::None;
#if BUILDFLAG(IS_MAC)
  constexpr uint32_t kAMDVendorID = 0x1002;
  if (base::mac::MacOSMajorVersion() < 13 &&
      properties.vendorID == kAMDVendorID &&
      properties.backendType == wgpu::BackendType::Metal) {
    reason = reason | WebGPUBlocklistReason::DynamicArrayIndexInStruct;
  }
#endif

  if (properties.backendType == wgpu::BackendType::D3D12) {
#if defined(ARCH_CPU_X86)
    constexpr uint32_t kNVIDIAVendorID = 0x10de;
    if (properties.vendorID == kNVIDIAVendorID) {
      reason = reason | WebGPUBlocklistReason::IndirectComputeRootConstants;
    }
#endif  // defined(ARCH_CPU_X86)
#if defined(ARCH_CPU_ARM_FAMILY)
    reason = reason | WebGPUBlocklistReason::WindowsARM;
#endif  // defined(ARCH_CPU_ARM_FAMILY)
  }

#if BUILDFLAG(IS_ANDROID)
  if (properties.backendType == wgpu::BackendType::OpenGLES) {
    reason = reason | WebGPUBlocklistReason::AndroidGLES;
  }

  constexpr uint32_t kARMVendorID = 0x13B5;
  constexpr uint32_t kQualcommVendorID = 0x5143;
  const auto* build_info = base::android::BuildInfo::GetInstance();
  // Only Android 12 with an ARM or Qualcomm GPU is enabled for initially.
  // Other OS versions and GPU vendors may be fine, but have not had
  // sufficient testing yet.
  if (build_info->sdk_int() < base::android::SDK_VERSION_S ||
      (properties.vendorID != kARMVendorID &&
       properties.vendorID != kQualcommVendorID)) {
    reason = reason | WebGPUBlocklistReason::AndroidLimitedSupport;
  }

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  constexpr uint32_t kAMDVendorID = 0x1002;
  if (properties.vendorID == kAMDVendorID && properties.deviceID == 0x98e4) {
    reason = reason | WebGPUBlocklistReason::AMDMissingDrmFormatModifier;
  }
#endif

  if (properties.adapterType == wgpu::AdapterType::CPU) {
    reason = reason | WebGPUBlocklistReason::CPUAdapter;
  }

  if (properties.backendType == wgpu::BackendType::D3D11) {
    reason = reason | WebGPUBlocklistReason::D3D11;
  }

  for (auto* chain = properties.nextInChain; chain != nullptr;
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
      if (!base::MatchPattern(U32ToHexString(properties.vendorID),
                              segments[0])) {
        continue;
      }
    }

    // Check if the deviceID or architecture matches the second segment.
    if (segments.size() >= 2) {
      if (!base::MatchPattern(U32ToHexString(properties.deviceID),
                              segments[1]) &&
          !base::MatchPattern(properties.architecture, segments[1])) {
        continue;
      }
    }

    // Check if the driver description matches the third segment.
    if (segments.size() >= 3) {
      if (!base::MatchPattern(properties.driverDescription, segments[2])) {
        continue;
      }
    }

    // Adapter is blocked.
    reason = reason | WebGPUBlocklistReason::StringPattern;
  }
  return (~options.ignores & reason) != WebGPUBlocklistReason::None;
}

}  // namespace detail

bool IsWebGPUAdapterBlocklisted(const wgpu::Adapter& adapter,
                                WebGPUBlocklistOptions options) {
  wgpu::AdapterProperties properties;
  wgpu::AdapterPropertiesD3D d3dProperties;
  if (adapter.HasFeature(wgpu::FeatureName::AdapterPropertiesD3D)) {
    properties.nextInChain = &d3dProperties;
  }
  adapter.GetProperties(&properties);

  return detail::IsWebGPUAdapterBlocklisted(properties, options);
}

}  // namespace gpu
