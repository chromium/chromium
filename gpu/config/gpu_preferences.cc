// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_preferences.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/gpu_preferences.mojom.h"

namespace gpu {

namespace {

#if !BUILDFLAG(IS_ANDROID)
size_t GetCustomGpuCacheSizeBytesIfExists(base::StringPiece switch_string) {
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

BASE_FEATURE(kDefaultGpuDiskCacheSize,
             "DefaultGpuDiskCacheSize",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kGpuDefaultMaxProgramCacheMemoryBytes{
    &kDefaultGpuDiskCacheSize, "GpuDefaultMaxProgramCacheMemoryBytes",
    kDefaultMaxProgramCacheMemoryBytes};

#if BUILDFLAG(IS_ANDROID)
constexpr base::FeatureParam<int> kGpuLowEndMaxProgramCacheMemoryBytes{
    &kDefaultGpuDiskCacheSize, "GpuLowEndMaxProgramCacheMemoryBytes",
    kLowEndMaxProgramCacheMemoryBytes};
#endif

}  // namespace

size_t GetDefaultGpuDiskCacheSize() {
#if !BUILDFLAG(IS_ANDROID)
  size_t custom_cache_size =
      GetCustomGpuCacheSizeBytesIfExists(switches::kGpuDiskCacheSizeKB);
  if (custom_cache_size)
    return custom_cache_size;
  return kGpuDefaultMaxProgramCacheMemoryBytes.Get();
#else   // !BUILDFLAG(IS_ANDROID)
  if (!base::SysInfo::IsLowEndDevice())
    return kGpuDefaultMaxProgramCacheMemoryBytes.Get();
  else
    return kGpuLowEndMaxProgramCacheMemoryBytes.Get();
#endif  // !BUILDFLAG(IS_ANDROID)
}

GpuPreferences::GpuPreferences() = default;

GpuPreferences::GpuPreferences(const GpuPreferences& other) = default;

GpuPreferences::~GpuPreferences() = default;

std::string GpuPreferences::ToSwitchValue() {
  std::vector<uint8_t> serialized = gpu::mojom::GpuPreferences::Serialize(this);

  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(serialized.data()),
                        serialized.size()),
      &encoded);
  return encoded;
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
