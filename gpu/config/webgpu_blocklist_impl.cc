// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/webgpu_blocklist_impl.h"

#include <sstream>

#include "base/strings/pattern.h"
#include "base/strings/string_split.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace gpu {

bool IsWebGPUAdapterBlocklisted(const WGPUAdapterProperties& properties,
                                const std::string& blocklist) {
#if BUILDFLAG(IS_MAC)
  constexpr uint32_t kAMDVendorID = 0x1002;
  // Blocklisted due to https://crbug.com/tint/1094
  if (base::mac::MacOSMajorVersion() < 13 &&
      properties.vendorID == kAMDVendorID &&
      properties.backendType == WGPUBackendType_Metal) {
    return true;
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  constexpr uint32_t kARMVendorID = 0x13B5;
  constexpr uint32_t kQualcommVendorID = 0x5143;

  const auto* build_info = base::android::BuildInfo::GetInstance();
  // Only Android 12 with an ARM or Qualcomm GPU is enabled for initially.
  // Other OS versions and GPU vendors may be fine, but have not had sufficient
  // testing yet.
  if (build_info->sdk_int() < base::android::SDK_VERSION_S ||
      (properties.vendorID != kARMVendorID &&
       properties.vendorID != kQualcommVendorID)) {
    return true;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // TODO(crbug.com/1266550): SwiftShader and CPU adapters are blocked until
  // fully tested.
  if (properties.adapterType == WGPUAdapterType_CPU) {
    return true;
  }

  // TODO(dawn:1705): d3d11 is not full implemented yet.
  if (properties.backendType == WGPUBackendType_D3D11) {
    return true;
  }

  auto U32ToHexString = [](uint32_t value) {
    std::ostringstream o;
    o << std::hex << value;
    return o.str();
  };

  auto blocked_patterns = base::SplitString(
      blocklist, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

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
    return true;
  }
  return false;
}

}  // namespace gpu
