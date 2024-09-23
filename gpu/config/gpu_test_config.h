// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_TEST_CONFIG_H_
#define GPU_CONFIG_GPU_TEST_CONFIG_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "gpu/gpu_export.h"

namespace gpu {

struct GPUInfo;

class GPU_EXPORT GPUTestConfig {
 public:
  enum OS {
    kOsUnknown = 0,
    kOsWin10 = 1 << 1,
    kOsWin = kOsWin10,
    // Jump over a few bits for future Windows versions.
    // Versions after Sonoma are now at the top of the list, replacing obsolete
    // versions.
    kOsMacSequoia = 1 << 10,
    kOsMacSnowLeopard = 1 << 11,
    kOsMacLion = 1 << 12,
    kOsMacMountainLion = 1 << 13,
    kOsMacMavericks = 1 << 14,
    kOsMacYosemite = 1 << 15,
    kOsMacElCapitan = 1 << 16,
    kOsMacSierra = 1 << 17,
    kOsMacHighSierra = 1 << 18,
    kOsMacMojave = 1 << 19,
    kOsMacCatalina = 1 << 20,
    kOsMacBigSur = 1 << 21,
    kOsMacMonterey = 1 << 22,
    kOsMacVentura = 1 << 23,
    kOsMacSonoma = 1 << 24,
    kOsMac = kOsMacSequoia | kOsMacSnowLeopard | kOsMacLion |
             kOsMacMountainLion | kOsMacMavericks | kOsMacYosemite |
             kOsMacElCapitan | kOsMacSierra | kOsMacHighSierra | kOsMacMojave |
             kOsMacCatalina | kOsMacBigSur | kOsMacMonterey | kOsMacVentura |
             kOsMacSonoma,
    kOsLinux = 1 << 25,
    kOsChromeOS = 1 << 26,
    kOsAndroid = 1 << 27,
    kOsFuchsia = 1 << 28,
    kOsIOS = 1 << 29,
    // If we run out of bits, please retire older OS versions, like WinXP,
    // MacSnowLeopard, etc., for which we no longer have bots.
  };

  enum BuildType {
    kBuildTypeUnknown = 0,
    kBuildTypeRelease = 1 << 0,
    kBuildTypeDebug = 1 << 1,
  };

  enum API {
    kAPIUnknown = 0,
    kAPID3D9 = 1 << 0,
    kAPID3D11 = 1 << 1,
    kAPIGLDesktop = 1 << 2,
    kAPIGLES = 1 << 3,
  };

  enum CommandDecoder {
    kCommandDecoderUnknown,
    kCommandDecoderPassthrough = 1 << 0,
    kCommandDecoderValidating = 1 << 1,
  };

  GPUTestConfig();
  GPUTestConfig(const GPUTestConfig& other);
  virtual ~GPUTestConfig();

  void set_os(int32_t os);
  void set_gpu_device_id(uint32_t id);
  void set_build_type(int32_t build_type);
  void set_api(int32_t api);
  void set_command_decoder(int32_t command_decoder);

  virtual void AddGPUVendor(uint32_t gpu_vendor);

  int32_t os() const { return os_; }
  const std::vector<uint32_t>& gpu_vendor() const { return gpu_vendor_; }
  uint32_t gpu_device_id() const { return gpu_device_id_; }
  int32_t build_type() const { return build_type_; }
  int32_t api() const { return api_; }
  int32_t command_decoder() const { return command_decoder_; }

  // Check if the config is valid. For example, if gpu_device_id_ is set, but
  // gpu_vendor_ is unknown, then it's invalid.
  virtual bool IsValid() const;

  // Check if two configs overlap, i.e., if there exists a config that matches
  // both configs.
  bool OverlapsWith(const GPUTestConfig& config) const;

 protected:
  void ClearGPUVendor();

 private:
  // operating system.
  int32_t os_;

  // GPU vendor.
  std::vector<uint32_t> gpu_vendor_;

  // GPU device id (unique to each vendor).
  uint32_t gpu_device_id_;

  // Release or Debug.
  int32_t build_type_;

  // Back-end rendering APIs.
  int32_t api_;

  // GPU process command decoder type
  int32_t command_decoder_;
};

class GPU_EXPORT GPUTestBotConfig : public GPUTestConfig {
 public:
  GPUTestBotConfig() = default;
  ~GPUTestBotConfig() override;

  // This should only be called when no gpu_vendor is added.
  void AddGPUVendor(uint32_t gpu_vendor) override;

  // Return false if gpu_info does not have valid vendor_id and device_id.
  bool SetGPUInfo(const GPUInfo& gpu_info);

  // Check if the bot config is valid, i.e., if it is one valid test-bot
  // environment. For example, if a field is unknown, or if OS is not one
  // fully defined OS, then it's valid.
  bool IsValid() const override;

  // Check if a bot config matches a test config, i.e., the test config is a
  // superset of the bot config.
  bool Matches(const GPUTestConfig& config) const;
  bool Matches(const std::string& config_data) const;

  // Setup the config with the current gpu testing environment.
  // If gpu_info is nullptr, collect GPUInfo first.
  bool LoadCurrentConfig(const GPUInfo* gpu_info);

  // Check if this bot's config matches |config_data| or any of the |configs|.
  static bool CurrentConfigMatches(const std::string& config_data);
  static bool CurrentConfigMatches(const std::vector<std::string>& configs);

  // Check if the bot has blocklisted all GPU features.
  static bool GpuBlocklistedOnBot();
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_TEST_CONFIG_H_
