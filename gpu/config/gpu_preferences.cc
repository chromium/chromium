// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_preferences.h"

#include <string_view>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/gpu_preferences.mojom.h"

namespace gpu {

namespace {

#if !BUILDFLAG(IS_ANDROID)
size_t GetCustomGpuCacheSizeBytesIfExists(std::string_view switch_string) {
  const base::CommandLine& process_command_line =
      *base::CommandLine::ForCurrentProcess();
  size_t cache_size;
  if (process_command_line.HasSwitch(switch_string)) {
    if (base::StringToSizeT(
            process_command_line.GetSwitchValueASCII(switch_string),
            &cache_size)) {
      return cache_size * 1024;  // Bytes
    }
  }
  return 0;
}
#endif

}  // namespace

size_t GetDefaultGpuDiskCacheSize() {
  size_t cache_size = 0;
#if !BUILDFLAG(IS_ANDROID)
  size_t custom_cache_size =
      GetCustomGpuCacheSizeBytesIfExists(switches::kGpuDiskCacheSizeKB);
  if (custom_cache_size) {
    cache_size = custom_cache_size;
  } else {
    cache_size = kDefaultMaxProgramCacheMemoryBytes;
  }
#else   // !BUILDFLAG(IS_ANDROID)
  cache_size = base::SysInfo::IsLowEndDevice()
                   ? kLowEndMaxProgramCacheMemoryBytes
                   : kDefaultMaxProgramCacheMemoryBytes;
#endif  // !BUILDFLAG(IS_ANDROID)

  if (base::FeatureList::IsEnabled(::features::kAggressiveShaderCacheLimits)) {
    cache_size *= 2;
  }
  return cache_size;
}

std::string GrContextTypeToString(GrContextType type) {
  switch (type) {
    case GrContextType::kNone:
      return "None";
    case GrContextType::kGL:
      return "GaneshGL";
    case GrContextType::kVulkan:
      return "GaneshVulkan";
    case GrContextType::kGraphiteDawn:
      return "GraphiteDawn";
    case GrContextType::kGraphiteMetal:
      return "GraphiteMetal";
  }
  NOTREACHED() << "GrContextType=" << static_cast<int>(type);
}

std::string SkiaBackendTypeToString(SkiaBackendType type) {
  switch (type) {
    case SkiaBackendType::kUnknown:
      return "Unknown";
    case SkiaBackendType::kNone:
      return "None";
    case SkiaBackendType::kGaneshGL:
      return "GaneshGL";
    case SkiaBackendType::kGaneshVulkan:
      return "GaneshVulkan";
    case SkiaBackendType::kGraphiteDawnVulkan:
      return "GraphiteDawnVulkan";
    case SkiaBackendType::kGraphiteDawnMetal:
      return "GraphiteDawnMetal";
    case SkiaBackendType::kGraphiteDawnD3D11:
      return "GraphiteDawnD3D11";
    case SkiaBackendType::kGraphiteDawnD3D12:
      return "GraphiteDawnD3D12";
    case SkiaBackendType::kGraphiteMetal:
      return "GraphiteMetal";
  }
  NOTREACHED() << "SkiaBackendType=" << static_cast<int>(type);
}

GpuPreferences::GpuPreferences() = default;

GpuPreferences::GpuPreferences(const GpuPreferences& other) = default;

GpuPreferences::~GpuPreferences() = default;

std::string GpuPreferences::ToSwitchValue() {
  std::vector<uint8_t> serialized = gpu::mojom::GpuPreferences::Serialize(this);
  return base::Base64Encode(serialized);
}

bool GpuPreferences::FromSwitchValue(const std::string& data) {
  std::string decoded;
  if (!base::Base64Decode(data, &decoded))
    return false;
  if (!gpu::mojom::GpuPreferences::Deserialize(decoded.data(), decoded.size(),
                                               this)) {
    return false;
  }
  return true;
}

}  // namespace gpu
