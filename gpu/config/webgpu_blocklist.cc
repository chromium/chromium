// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/webgpu_blocklist.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/buildflags.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(USE_DAWN)
#include "third_party/dawn/include/dawn/webgpu.h"  // nogncheck
#endif

namespace gpu {

namespace {

// List of patterns, delimited by |. Each pattern is of the form:
// `vendorId(:deviceIdOrArchitecture(:driverDescription)?)?`
// Vendor and device ids should be in hexadecimal representation, without a
// leading `0x`.
// The FeatureParam may be overriden via Finch config, or via the command line
// with
// --force-fieldtrial-params=WebGPU.Enabled:AdapterBlockList/params
// where `params` is URL-encoded.
const base::FeatureParam<std::string> kAdapterBlockList{
    &features::kWebGPUService, "AdapterBlockList", ""};

}  // namespace

bool IsWebGPUAdapterBlocklisted(const WGPUAdapterProperties& properties) {
  return IsWebGPUAdapterBlocklisted(properties, kAdapterBlockList.Get());
}

bool IsWebGPUAdapterBlocklisted(const WGPUAdapterProperties& properties,
                                const std::string& blocklist) {
#if BUILDFLAG(USE_DAWN)
#if BUILDFLAG(IS_MAC)
  constexpr uint32_t kAMDVendorID = 0x1002;
  // Blocklisted due to https://crbug.com/tint/1094
  if (base::mac::MacOSMajorVersion() < 13 &&
      properties.vendorID == kAMDVendorID &&
      properties.backendType == WGPUBackendType_Metal) {
    return true;
  }
#endif

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
#else
  return true;
#endif
}

}  // namespace gpu
