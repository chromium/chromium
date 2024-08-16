// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_test_config.h"

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_test_expectations_parser.h"
#include "ui/gl/gl_utils.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace gpu {

namespace {

GPUTestConfig::OS GetCurrentOS() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return GPUTestConfig::kOsChromeOS;
#elif (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    BUILDFLAG(IS_OPENBSD)
  return GPUTestConfig::kOsLinux;
#elif BUILDFLAG(IS_WIN)
  int32_t major_version = 0;
  int32_t minor_version = 0;
  int32_t bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  if (major_version == 10)
    return GPUTestConfig::kOsWin10;
  return GPUTestConfig::kOsUnknown;
#elif BUILDFLAG(IS_MAC)
  int32_t major_version = 0;
  int32_t minor_version = 0;
  int32_t bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &major_version, &minor_version, &bugfix_version);
  switch (major_version) {
    case 10:
      switch (minor_version) {
        case 6:
          return GPUTestConfig::kOsMacSnowLeopard;
        case 7:
          return GPUTestConfig::kOsMacLion;
        case 8:
          return GPUTestConfig::kOsMacMountainLion;
        case 9:
          return GPUTestConfig::kOsMacMavericks;
        case 10:
          return GPUTestConfig::kOsMacYosemite;
        case 11:
          return GPUTestConfig::kOsMacElCapitan;
        case 12:
          return GPUTestConfig::kOsMacSierra;
        case 13:
          return GPUTestConfig::kOsMacHighSierra;
        case 14:
          return GPUTestConfig::kOsMacMojave;
        case 15:
          return GPUTestConfig::kOsMacCatalina;
      }
      break;
    case 11:
      return GPUTestConfig::kOsMacBigSur;
    case 12:
      return GPUTestConfig::kOsMacMonterey;
    case 13:
      return GPUTestConfig::kOsMacVentura;
    case 14:
      return GPUTestConfig::kOsMacSonoma;
    case 15:
      return GPUTestConfig::kOsMacSequoia;
  }
  return GPUTestConfig::kOsUnknown;
#elif BUILDFLAG(IS_ANDROID)
  return GPUTestConfig::kOsAndroid;
#elif BUILDFLAG(IS_FUCHSIA)
  return GPUTestConfig::kOsFuchsia;
#elif BUILDFLAG(IS_IOS)
  return GPUTestConfig::kOsIOS;
#else
#error "unknown os"
#endif
}

}  // namespace anonymous

GPUTestConfig::GPUTestConfig()
    : os_(kOsUnknown),
      gpu_device_id_(0),
      build_type_(kBuildTypeUnknown),
      api_(kAPIUnknown),
      command_decoder_(kCommandDecoderUnknown) {}

GPUTestConfig::GPUTestConfig(const GPUTestConfig& other) = default;

GPUTestConfig::~GPUTestConfig() = default;

void GPUTestConfig::set_os(int32_t os) {
  DCHECK_EQ(0, os & ~(kOsAndroid | kOsWin | kOsMac | kOsLinux | kOsChromeOS |
                      kOsFuchsia | kOsIOS));
  os_ = os;
}

void GPUTestConfig::AddGPUVendor(uint32_t gpu_vendor) {
  DCHECK_NE(0u, gpu_vendor);
  for (size_t i = 0; i < gpu_vendor_.size(); ++i)
    DCHECK_NE(gpu_vendor_[i], gpu_vendor);
  gpu_vendor_.push_back(gpu_vendor);
}

void GPUTestConfig::set_gpu_device_id(uint32_t id) {
  gpu_device_id_ = id;
}

void GPUTestConfig::set_build_type(int32_t build_type) {
  DCHECK_EQ(0, build_type & ~(kBuildTypeRelease | kBuildTypeDebug));
  build_type_ = build_type;
}

void GPUTestConfig::set_api(int32_t api) {
  DCHECK_EQ(0, api & ~(kAPID3D9 | kAPID3D11 | kAPIGLDesktop | kAPIGLES));
  api_ = api;
}

void GPUTestConfig::set_command_decoder(int32_t command_decoder) {
  DCHECK_EQ(0, command_decoder &
                   ~(kCommandDecoderPassthrough | kCommandDecoderValidating));
  command_decoder_ = command_decoder;
}

bool GPUTestConfig::IsValid() const {
  if (gpu_device_id_ != 0 && (gpu_vendor_.size() != 1 || gpu_vendor_[0] == 0))
    return false;
  return true;
}

bool GPUTestConfig::OverlapsWith(const GPUTestConfig& config) const {
  DCHECK(IsValid());
  DCHECK(config.IsValid());
  if (config.os_ != kOsUnknown && os_ != kOsUnknown &&
      (os_ & config.os_) == 0)
    return false;
  if (config.gpu_vendor_.size() > 0 && gpu_vendor_.size() > 0) {
    bool shared = false;
    for (size_t i = 0; i < config.gpu_vendor_.size() && !shared; ++i) {
      for (size_t j = 0; j < gpu_vendor_.size(); ++j) {
        if (config.gpu_vendor_[i] == gpu_vendor_[j]) {
          shared = true;
          break;
        }
      }
    }
    if (!shared)
      return false;
  }
  if (config.gpu_device_id_ != 0 && gpu_device_id_ != 0 &&
      gpu_device_id_ != config.gpu_device_id_)
    return false;
  if (config.build_type_ != kBuildTypeUnknown &&
      build_type_ != kBuildTypeUnknown &&
      (build_type_ & config.build_type_) == 0)
    return false;
  if (config.api() != kAPIUnknown && api_ != kAPIUnknown && api_ != config.api_)
    return false;
  return true;
}

void GPUTestConfig::ClearGPUVendor() {
  gpu_vendor_.clear();
}

GPUTestBotConfig::~GPUTestBotConfig() = default;

void GPUTestBotConfig::AddGPUVendor(uint32_t gpu_vendor) {
  DCHECK_EQ(0u, GPUTestConfig::gpu_vendor().size());
  GPUTestConfig::AddGPUVendor(gpu_vendor);
}

bool GPUTestBotConfig::SetGPUInfo(const GPUInfo& gpu_info) {
  if (gpu_info.gpu.vendor_id == 0)
    return false;
#if !BUILDFLAG(IS_MAC)
  // ARM-based Mac GPUs do not have valid PCI device IDs.
  // https://crbug.com/1110421
  if (gpu_info.gpu.device_id == 0)
    return false;
#endif
  ClearGPUVendor();
  AddGPUVendor(gpu_info.gpu.vendor_id);
  set_gpu_device_id(gpu_info.gpu.device_id);
  if (gpu_info.passthrough_cmd_decoder) {
    set_command_decoder(kCommandDecoderPassthrough);
  } else {
    set_command_decoder(kCommandDecoderValidating);
  }
  return true;
}

bool GPUTestBotConfig::IsValid() const {
  switch (os()) {
    case kOsWin10:
    case kOsMacSnowLeopard:
    case kOsMacLion:
    case kOsMacMountainLion:
    case kOsMacMavericks:
    case kOsMacYosemite:
    case kOsMacElCapitan:
    case kOsMacSierra:
    case kOsMacHighSierra:
    case kOsMacMojave:
    case kOsMacCatalina:
    case kOsMacBigSur:
    case kOsMacMonterey:
    case kOsMacVentura:
    case kOsMacSonoma:
    case kOsMacSequoia:
    case kOsLinux:
    case kOsChromeOS:
    case kOsAndroid:
    case kOsFuchsia:
    case kOsIOS:
      break;
    default:
      return false;
  }
  if (gpu_vendor().size() != 1 || gpu_vendor()[0] == 0)
    return false;
  if (!(os() & gpu::GPUTestConfig::kOsMac)) {
    // ARM-based Mac GPUs do not have valid PCI device IDs.
    // https://crbug.com/1110421
    if (gpu_device_id() == 0)
      return false;
  }
  switch (build_type()) {
    case kBuildTypeRelease:
    case kBuildTypeDebug:
      break;
    default:
      return false;
  }
  return true;
}

bool GPUTestBotConfig::Matches(const GPUTestConfig& config) const {
  DCHECK(IsValid());
  DCHECK(config.IsValid());
  if (config.os() != kOsUnknown && (os() & config.os()) == 0)
    return false;
  if (config.gpu_vendor().size() > 0) {
    bool contained = false;
    for (size_t i = 0; i < config.gpu_vendor().size(); ++i) {
      if (config.gpu_vendor()[i] == gpu_vendor()[0]) {
        contained = true;
        break;
      }
    }
    if (!contained)
      return false;
  }
  if (config.gpu_device_id() != 0 &&
      gpu_device_id() != config.gpu_device_id())
    return false;
  if (config.build_type() != kBuildTypeUnknown &&
      (build_type() & config.build_type()) == 0)
    return false;
  if (config.api() != 0 && (api() & config.api()) == 0)
    return false;
  if (config.command_decoder() != 0 &&
      command_decoder() != config.command_decoder())
    return false;
  return true;
}

bool GPUTestBotConfig::Matches(const std::string& config_data) const {
  GPUTestExpectationsParser parser;
  GPUTestConfig config;

  if (!parser.ParseConfig(config_data, &config))
    return false;
  return Matches(config);
}

bool GPUTestBotConfig::LoadCurrentConfig(const GPUInfo* gpu_info) {
  bool rt;
  if (!gpu_info) {
    GPUInfo my_gpu_info;
    if (!CollectBasicGraphicsInfo(base::CommandLine::ForCurrentProcess(),
                                  &my_gpu_info)) {
      LOG(ERROR) << "Fail to identify GPU";
      rt = false;
    } else {
      rt = SetGPUInfo(my_gpu_info);
    }
  } else {
    rt = SetGPUInfo(*gpu_info);
  }
  set_os(GetCurrentOS());
  if (os() == kOsUnknown) {
    LOG(ERROR) << "Unknown OS";
    rt = false;
  }
#if defined(NDEBUG)
  set_build_type(kBuildTypeRelease);
#else
  set_build_type(kBuildTypeDebug);
#endif
  return rt;
}

// static
bool GPUTestBotConfig::CurrentConfigMatches(const std::string& config_data) {
  GPUTestBotConfig my_config;
  if (!my_config.LoadCurrentConfig(nullptr))
    return false;
  return my_config.Matches(config_data);
}

// static
bool GPUTestBotConfig::CurrentConfigMatches(
    const std::vector<std::string>& configs) {
  GPUTestBotConfig my_config;
  if (!my_config.LoadCurrentConfig(nullptr))
    return false;
  for (size_t i = 0 ; i < configs.size(); ++i) {
    if (my_config.Matches(configs[i]))
      return true;
  }
  return false;
}

// static
bool GPUTestBotConfig::GpuBlocklistedOnBot() {
  return false;
}

}  // namespace gpu
