// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GPUTestConfigTest : public testing::Test {
 public:
  GPUTestConfigTest() = default;

  ~GPUTestConfigTest() override = default;

 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(GPUTestConfigTest, EmptyValues) {
  GPUTestConfig config;
  EXPECT_EQ(GPUTestConfig::kOsUnknown, config.os());
  EXPECT_EQ(0u, config.gpu_vendor().size());
  EXPECT_EQ(0u, config.gpu_device_id());
  EXPECT_EQ(GPUTestConfig::kBuildTypeUnknown, config.build_type());
  EXPECT_EQ(GPUTestConfig::kAPIUnknown, config.api());
  EXPECT_EQ(GPUTestConfig::kCommandDecoderUnknown, config.command_decoder());
}

TEST_F(GPUTestConfigTest, SetGPUInfo) {
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  GPUTestBotConfig config;
  EXPECT_TRUE(config.SetGPUInfo(gpu_info));
  EXPECT_EQ(1u, config.gpu_vendor().size());
  EXPECT_EQ(gpu_info.gpu.vendor_id, config.gpu_vendor()[0]);
  EXPECT_EQ(gpu_info.gpu.device_id, config.gpu_device_id());

  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.device_id = 0x0046;
  EXPECT_TRUE(config.SetGPUInfo(gpu_info));
  EXPECT_EQ(1u, config.gpu_vendor().size());
  EXPECT_EQ(gpu_info.gpu.vendor_id, config.gpu_vendor()[0]);
  EXPECT_EQ(gpu_info.gpu.device_id, config.gpu_device_id());
}

TEST_F(GPUTestConfigTest, IsValid) {
  {
    GPUTestConfig config;
    config.set_gpu_device_id(0x0640);
    EXPECT_FALSE(config.IsValid());
    config.AddGPUVendor(0x10de);
    EXPECT_TRUE(config.IsValid());
  }

  {
    GPUTestBotConfig config;
    config.set_build_type(GPUTestConfig::kBuildTypeRelease);
    config.set_os(GPUTestConfig::kOsWin10);
    config.set_gpu_device_id(0x0640);
    EXPECT_FALSE(config.IsValid());
    config.AddGPUVendor(0x10de);
    EXPECT_TRUE(config.IsValid());

    // Device ID of 0 is valid only on macOS.
    config.set_gpu_device_id(0);
    config.set_os(GPUTestConfig::kOsMacBigSur);
    EXPECT_TRUE(config.IsValid());
    config.set_os(GPUTestConfig::kOsWin10);
    EXPECT_FALSE(config.IsValid());

    config.set_gpu_device_id(0x0640);
    EXPECT_TRUE(config.IsValid());

    config.set_os(GPUTestConfig::kOsWin10);
    EXPECT_TRUE(config.IsValid());

    config.set_build_type(GPUTestConfig::kBuildTypeUnknown);
    EXPECT_FALSE(config.IsValid());
    config.set_build_type(GPUTestConfig::kBuildTypeRelease);
    EXPECT_TRUE(config.IsValid());
  }
}

TEST_F(GPUTestConfigTest, Matches) {
  GPUTestBotConfig config;
  config.set_os(GPUTestConfig::kOsWin10);
  config.set_build_type(GPUTestConfig::kBuildTypeRelease);
  config.AddGPUVendor(0x10de);
  config.set_gpu_device_id(0x0640);
  config.set_api(GPUTestConfig::kAPID3D11);
  config.set_command_decoder(GPUTestConfig::kCommandDecoderPassthrough);
  EXPECT_TRUE(config.IsValid());

  {  // os matching
    GPUTestConfig config2;
    EXPECT_TRUE(config.Matches(config2));
    config2.set_os(GPUTestConfig::kOsWin);
    EXPECT_TRUE(config.Matches(config2));
    config2.set_os(GPUTestConfig::kOsWin10);
    EXPECT_TRUE(config.Matches(config2));
    config2.set_os(GPUTestConfig::kOsMac);
    EXPECT_FALSE(config.Matches(config2));
    config2.set_os(GPUTestConfig::kOsWin10 | GPUTestConfig::kOsLinux);
    EXPECT_TRUE(config.Matches(config2));
  }

  {  // gpu vendor matching
    {
      GPUTestConfig config2;
      config2.AddGPUVendor(0x10de);
      EXPECT_TRUE(config.Matches(config2));
      config2.AddGPUVendor(0x1004);
      EXPECT_TRUE(config.Matches(config2));
    }
    {
      GPUTestConfig config2;
      config2.AddGPUVendor(0x8086);
      EXPECT_FALSE(config.Matches(config2));
    }
  }

  {  // build type matching
    GPUTestConfig config2;
    config2.set_build_type(GPUTestConfig::kBuildTypeRelease);
    EXPECT_TRUE(config.Matches(config2));
    config2.set_build_type(GPUTestConfig::kBuildTypeRelease |
                           GPUTestConfig::kBuildTypeDebug);
    EXPECT_TRUE(config.Matches(config2));
    config2.set_build_type(GPUTestConfig::kBuildTypeDebug);
    EXPECT_FALSE(config.Matches(config2));
  }

  {  // exact matching
    GPUTestConfig config2;
    config2.set_os(GPUTestConfig::kOsWin10);
    config2.set_build_type(GPUTestConfig::kBuildTypeRelease);
    config2.AddGPUVendor(0x10de);
    config2.set_gpu_device_id(0x0640);
    EXPECT_TRUE(config.Matches(config2));
    config2.set_gpu_device_id(0x0641);
    EXPECT_FALSE(config.Matches(config2));
  }

  {  // api matching
    {
      GPUTestConfig config2;
      config2.set_api(GPUTestConfig::kAPID3D11);
      EXPECT_TRUE(config.Matches(config2));
      config2.set_api(config2.api() | GPUTestConfig::kAPID3D9);
      EXPECT_TRUE(config.Matches(config2));
    }
    {
      GPUTestConfig config2;
      config2.set_api(GPUTestConfig::kAPID3D9);
      EXPECT_FALSE(config.Matches(config2));
    }
  }
  {  // command decoder matching
    {
      GPUTestConfig config2;
      config2.set_command_decoder(GPUTestConfig::kCommandDecoderPassthrough);
      EXPECT_TRUE(config.Matches(config2));
    }
    {
      GPUTestConfig config2;
      config2.set_command_decoder(GPUTestConfig::kCommandDecoderValidating);
      EXPECT_FALSE(config.Matches(config2));
    }
  }
}

TEST_F(GPUTestConfigTest, StringMatches) {
  GPUTestBotConfig config;
  config.set_os(GPUTestConfig::kOsWin10);
  config.set_build_type(GPUTestConfig::kBuildTypeRelease);
  config.AddGPUVendor(0x10de);
  config.set_gpu_device_id(0x0640);
  config.set_api(GPUTestConfig::kAPID3D11);
  config.set_command_decoder(GPUTestConfig::kCommandDecoderPassthrough);
  EXPECT_TRUE(config.IsValid());

  EXPECT_TRUE(config.Matches(std::string()));

  // os matching
  EXPECT_TRUE(config.Matches("WIN"));
  EXPECT_TRUE(config.Matches("WIN10"));
  EXPECT_FALSE(config.Matches("MAC"));
  EXPECT_TRUE(config.Matches("WIN10 LINUX"));

  // gpu vendor matching
  EXPECT_TRUE(config.Matches("NVIDIA"));
  EXPECT_TRUE(config.Matches("NVIDIA AMD"));
  EXPECT_FALSE(config.Matches("INTEL"));

  // build type matching
  EXPECT_TRUE(config.Matches("RELEASE"));
  EXPECT_TRUE(config.Matches("RELEASE DEBUG"));
  EXPECT_FALSE(config.Matches("DEBUG"));

  // exact matching
  EXPECT_TRUE(config.Matches("WIN10 RELEASE NVIDIA 0X0640"));
  EXPECT_FALSE(config.Matches("WIN10 RELEASE NVIDIA 0X0641"));

  // api matching
  EXPECT_TRUE(config.Matches("D3D11"));
  EXPECT_FALSE(config.Matches("D3D9 OPENGL GLES"));

  // command decoder matching
  EXPECT_TRUE(config.Matches("PASSTHROUGH"));
  EXPECT_FALSE(config.Matches("VALIDATING"));
}

TEST_F(GPUTestConfigTest, OverlapsWith) {
  {  // os
      // win vs win10
      GPUTestConfig config;
      config.set_os(GPUTestConfig::kOsWin);
      GPUTestConfig config2;
      config2.set_os(GPUTestConfig::kOsWin10);
      EXPECT_TRUE(config.OverlapsWith(config2));
      EXPECT_TRUE(config2.OverlapsWith(config));
      // win vs win10+linux
      config2.set_os(GPUTestConfig::kOsWin10 | GPUTestConfig::kOsLinux);
      EXPECT_TRUE(config.OverlapsWith(config2));
      EXPECT_TRUE(config2.OverlapsWith(config));
      // win vs mac
      config2.set_os(GPUTestConfig::kOsMac);
      EXPECT_FALSE(config.OverlapsWith(config2));
      EXPECT_FALSE(config2.OverlapsWith(config));
      // win vs unknown
      config2.set_os(GPUTestConfig::kOsUnknown);
      EXPECT_TRUE(config.OverlapsWith(config2));
      EXPECT_TRUE(config2.OverlapsWith(config));
  }

  {  // gpu vendor
    GPUTestConfig config;
    config.AddGPUVendor(0x10de);
    // nvidia vs unknown
    GPUTestConfig config2;
    EXPECT_TRUE(config.OverlapsWith(config2));
    EXPECT_TRUE(config2.OverlapsWith(config));
    // nvidia vs intel
    config2.AddGPUVendor(0x1086);
    EXPECT_FALSE(config.OverlapsWith(config2));
    EXPECT_FALSE(config2.OverlapsWith(config));
    // nvidia vs nvidia+intel
    config2.AddGPUVendor(0x10de);
    EXPECT_TRUE(config.OverlapsWith(config2));
    EXPECT_TRUE(config2.OverlapsWith(config));
  }

  {  // build type
    // release vs debug
    GPUTestConfig config;
    config.set_build_type(GPUTestConfig::kBuildTypeRelease);
    GPUTestConfig config2;
    config2.set_build_type(GPUTestConfig::kBuildTypeDebug);
    EXPECT_FALSE(config.OverlapsWith(config2));
    EXPECT_FALSE(config2.OverlapsWith(config));
    // release vs release+debug
    config2.set_build_type(GPUTestConfig::kBuildTypeRelease |
                           GPUTestConfig::kBuildTypeDebug);
    EXPECT_TRUE(config.OverlapsWith(config2));
    EXPECT_TRUE(config2.OverlapsWith(config));
    // release vs unknown
    config2.set_build_type(GPUTestConfig::kBuildTypeUnknown);
    EXPECT_TRUE(config.OverlapsWith(config2));
    EXPECT_TRUE(config2.OverlapsWith(config));
  }

  {  // win10 vs nvidia
    GPUTestConfig config;
    config.set_os(GPUTestConfig::kOsWin10);
    GPUTestConfig config2;
    config2.AddGPUVendor(0x10de);
    EXPECT_TRUE(config.OverlapsWith(config2));
    EXPECT_TRUE(config2.OverlapsWith(config));
  }
}

TEST_F(GPUTestConfigTest, LoadCurrentConfig) {
  GPUTestBotConfig config;
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  EXPECT_TRUE(config.LoadCurrentConfig(&gpu_info));
  EXPECT_TRUE(config.IsValid());
}

}  // namespace gpu

