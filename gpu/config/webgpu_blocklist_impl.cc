// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/webgpu_blocklist_impl.h"

#include <array>
#include <sstream>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/buildflags.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#endif

#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace gpu {

namespace detail {

#if BUILDFLAG(USE_DAWN)
// List of patterns, delimited by |. Each pattern is of the form:
// `vendorId(:deviceIdOrArchitecture(:driverDescription)?)?`
// Vendor and device ids should be in hexadecimal representation, without a
// leading `0x`.
// The FeatureParam may be overriden via Finch config, or via the command line
// with
// --force-fieldtrial-params=WebGPU.Enabled:AdapterBlockList/params
// where `params` is URL-encoded.
const base::FeatureParam<std::string> kAdapterBlockList{
    &features::kWebGPUService, "AdapterBlockList",
    // We aim to enable Qualcomm (0x4d4f4351) Windows starting from 0x36334330
    // (8380) onwards, with a driver version of 31.0.117.0 or higher.

    // Currently, Qualcomm Windows supports two architectures: adreno-6xx and
    // adreno-7xx. The adreno-6xx architecture includes 0x41333830 (7180),
    // 0x36334130 (7280), 0x41333430 (8180), and 0x36333630 (8280), while the
    // adreno-7xx architecture encompasses 0x37314430 (8340) and 0x36334330
    // (8380).

    "4d4f4351:41333830|4d4f4351:36334130|4d4f4351:41333430|4d4f4351:36333630|"
    "4d4f4351:37314430|"

    // Regarding driver versions, all of them are in 31.0.??.* or 31.0.???.*
    // format.

    "4d4f4351:36334330:*31.0.??.*|4d4f4351:36334330:*31.0.10?.*|"
    "4d4f4351:36334330:*31.0.110.*|4d4f4351:36334330:*31.0.111.*|"
    "4d4f4351:36334330:*31.0.112.*|4d4f4351:36334330:*31.0.113.*|"
    "4d4f4351:36334330:*31.0.114.*|4d4f4351:36334330:*31.0.115.*|"
    "4d4f4351:36334330:*31.0.116.*"};
#endif  // BUILDFLAG(USE_DAWN)

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

#if BUILDFLAG(IS_WIN)
  constexpr uint32_t kAMDVendorID = 0x1002;
  constexpr uint32_t kIntelVendorID = 0x8086;
  constexpr uint32_t kMicrosoftVendorID = 0x1414;
  constexpr uint32_t kNVIDIAVendorID = 0x10DE;
  constexpr uint32_t kQualcommVendorID = 0x4D4F4351;

  if (info.backendType == wgpu::BackendType::D3D12) {
    switch (info.vendorID) {
      case kNVIDIAVendorID:
#if defined(ARCH_CPU_X86)
        reason = reason | WebGPUBlocklistReason::IndirectComputeRootConstants;
#endif  // defined(ARCH_CPU_X86)
        break;

      case kQualcommVendorID:
        if (!base::FeatureList::IsEnabled(features::kWebGPUQualcommWindows)) {
          reason = reason | WebGPUBlocklistReason::QualcommWindows;
        }
        break;

      case kAMDVendorID:
      case kIntelVendorID:
      case kMicrosoftVendorID:
        break;

      default:
        // Other OS versions/GPU vendor combinations may be fine, but have not
        // had sufficient testing yet.
        reason = reason | WebGPUBlocklistReason::WindowsLimitedSupport;
        break;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
  constexpr uint32_t kARMVendorID = 0x13B5;
  constexpr uint32_t kQualcommVendorID = 0x5143;
  constexpr uint32_t kIntelVendorID = 0x8086;
  constexpr uint32_t kImgTecVendorID = 0x1010;

  switch (info.vendorID) {
    case kARMVendorID:
    case kQualcommVendorID:
    case kIntelVendorID:
      // ARM, Qualcomm, and Intel GPUs are supported on Android 12+ on Vulkan
      if (info.backendType == wgpu::BackendType::Vulkan &&
          (base::android::android_info::sdk_int() <
           base::android::android_info::SDK_VERSION_S)) {
        reason = reason | WebGPUBlocklistReason::AndroidLimitedSupport;
      }
      // and Android 10+ on OpenGLES (currently Chrome's minimum, so no version
      // check here)
      break;

    case kImgTecVendorID:
      // Imagination GPUs are supported on Android 16+ on Vulkan
      if (info.backendType == wgpu::BackendType::Vulkan &&
          (base::android::android_info::sdk_int() <
           base::android::android_info::SDK_VERSION_BAKLAVA)) {
        reason = reason | WebGPUBlocklistReason::AndroidLimitedSupport;
      }
      // and Android 13+ on OpenGLES
      if (info.backendType == wgpu::BackendType::OpenGLES &&
          (base::android::android_info::sdk_int() <
           base::android::android_info::SDK_VERSION_T)) {
        reason = reason | WebGPUBlocklistReason::AndroidLimitedSupport;
      }
      break;

    default:
      // Other OS versions/GPU vendor combinations may be fine, but have not had
      // sufficient testing yet.
      reason = reason | WebGPUBlocklistReason::AndroidLimitedSupport;
      break;
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

  std::string blocklist_string;

#if BUILDFLAG(USE_DAWN)
  blocklist_string = kAdapterBlockList.Get();
#endif

  if (!options.blocklist_string.empty()) {
    if (!blocklist_string.empty()) {
      blocklist_string += "|";
    }
    blocklist_string += options.blocklist_string;
  }

  auto blocked_patterns = base::SplitString(
      blocklist_string, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

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
#if BUILDFLAG(IS_WIN)
    if (info.vendorID == kQualcommVendorID) {
      reason = reason | WebGPUBlocklistReason::StringPatternQualcommWindows;
    } else {
      reason = reason | WebGPUBlocklistReason::StringPatternOther;
    }
#else
    reason = reason | WebGPUBlocklistReason::StringPatternOther;
#endif

    break;
  }

  return reason;
}

std::string BlocklistReasonToString(WebGPUBlocklistReason reason) {
  std::string result;
  bool first = true;
  static constexpr std::array<
      std::pair<WebGPUBlocklistReason, std::string_view>, 11>
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
          {WebGPUBlocklistReason::WindowsLimitedSupport,
           "crbug.com/446254136: Limited support / testing currently "
           "available on Windows."},
          {WebGPUBlocklistReason::IndirectComputeRootConstants,
           "crbug.com/42240193: Indirect root constants in compute pass "
           "broken on Windows NVIDIA x86."},
          {WebGPUBlocklistReason::DynamicArrayIndexInStruct,
           "crbug.com/40643701: Metal compiler errors for dynamic indexing "
           "of arrays in structures."},
          {WebGPUBlocklistReason::StringPatternOther,
           "Blocklisted by vendor/device/driver string pattern."},
          {WebGPUBlocklistReason::QualcommWindows,
           "crbug.com/42242119: Limited support / testing currently "
           "available on Qualcomm Windows."},
          {WebGPUBlocklistReason::StringPatternQualcommWindows,
           "Blocklisted by vendor/device/driver string pattern on Qualcomm "
           "Windows."},
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

#if !BUILDFLAG(USE_DAWN)
static constexpr std::string_view kNotUseDawn = "BUILDFLAG(USE_DAWN) is false.";
#endif

WebGPUBlocklistResultImpl IsWebGPUAdapterBlocklisted(
    const wgpu::Adapter& adapter,
    WebGPUBlocklistOptions options) {
#if BUILDFLAG(USE_DAWN)
  wgpu::AdapterInfo info;
  wgpu::AdapterPropertiesD3D d3dProperties;
  if (adapter.HasFeature(wgpu::FeatureName::AdapterPropertiesD3D)) {
    info.nextInChain = &d3dProperties;
  }
  adapter.GetInfo(&info);
  return IsWebGPUAdapterBlocklisted(info, options);
#else
  return {.blocked = true, .reason = std::string(kNotUseDawn)};
#endif
}

WebGPUBlocklistResultImpl IsWebGPUAdapterBlocklisted(
    const wgpu::AdapterInfo& info,
    WebGPUBlocklistOptions options) {
#if BUILDFLAG(USE_DAWN)
  auto blocklistReason = detail::GetWebGPUAdapterBlocklistReason(info, options);
  bool blocked =
      (~options.ignores & blocklistReason) != WebGPUBlocklistReason::None;
  if (!blocked) {
    return {.blocked = false, .reason = ""};
  }
  return {.blocked = blocked,
          .reason = detail::BlocklistReasonToString(blocklistReason)};
#else
  return {.blocked = true, .reason = std::string(kNotUseDawn)};
#endif
}

}  // namespace gpu
