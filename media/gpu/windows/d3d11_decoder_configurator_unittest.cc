// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "media/base/media_util.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/gpu/windows/av1_guids.h"
#include "media/gpu/windows/d3d11_decoder_configurator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Values;

namespace media {

class D3D11DecoderConfiguratorUnittest : public ::testing::Test {
 public:
  VideoDecoderConfig CreateDecoderConfig(VideoCodecProfile profile,
                                         gfx::Size size,
                                         bool encrypted) {
    VideoDecoderConfig result;
    result.Initialize(
        VideoCodec::kUnknown,  // It doesn't matter because it won't
                               // be used.
        profile, VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
        kNoTransformation, size, {}, {}, {},
        encrypted ? EncryptionScheme::kCenc : EncryptionScheme::kUnencrypted);
    return result;
  }

  std::unique_ptr<D3D11DecoderConfigurator> CreateWithDefaultGPUInfo(
      const VideoDecoderConfig& config,
      bool zero_copy_enabled = true,
      uint8_t bit_depth = 8) {
    gpu::GpuPreferences prefs;
    prefs.enable_zero_copy_dxgi_video = zero_copy_enabled;
    gpu::GpuDriverBugWorkarounds workarounds;
    workarounds.disable_dxgi_zero_copy_video = false;
    VideoChromaSampling chroma_sampling = VideoChromaSampling::k420;
    auto media_log = std::make_unique<NullMediaLog>();
    return D3D11DecoderConfigurator::Create(
        prefs, workarounds, config, bit_depth, chroma_sampling, media_log.get(),
        false /*use_shared_handle*/);
  }
};

TEST_F(D3D11DecoderConfiguratorUnittest, VP9Profile0RightFormats) {
  auto configurator = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE0, {0, 0}, false));

  EXPECT_EQ(configurator->DecoderGuid(),
            D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0);
  EXPECT_EQ(configurator->DecoderDescriptor()->OutputFormat, DXGI_FORMAT_NV12);
}

TEST_F(D3D11DecoderConfiguratorUnittest, VP9Profile2RightFormats) {
  auto configurator = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE2, {0, 0}, false), false, 10);

  EXPECT_EQ(configurator->DecoderGuid(),
            D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2);
  EXPECT_EQ(configurator->DecoderDescriptor()->OutputFormat, DXGI_FORMAT_P010);
}

TEST_F(D3D11DecoderConfiguratorUnittest, AV1ProfileRightFormats) {
  auto configurator = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(AV1PROFILE_PROFILE_MAIN, {0, 0}, false), false, 8);
  EXPECT_EQ(configurator->DecoderGuid(), DXVA_ModeAV1_VLD_Profile0);
  EXPECT_EQ(configurator->DecoderDescriptor()->OutputFormat, DXGI_FORMAT_NV12);

  configurator = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(AV1PROFILE_PROFILE_MAIN, {0, 0}, false), false, 10);
  EXPECT_EQ(configurator->DecoderGuid(), DXVA_ModeAV1_VLD_Profile0);
  EXPECT_EQ(configurator->DecoderDescriptor()->OutputFormat, DXGI_FORMAT_P010);
}

TEST_F(D3D11DecoderConfiguratorUnittest, SupportsDeviceNoProfiles) {
  auto configurator = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE0, {0, 0}, false));

  auto vd_mock = MakeComPtr<D3D11VideoDeviceMock>();
  EXPECT_CALL(*vd_mock.Get(), GetVideoDecoderProfileCount())
      .Times(1)
      .WillOnce(Return(0));

  EXPECT_FALSE(configurator->SupportsDevice(vd_mock));
}

TEST_F(D3D11DecoderConfiguratorUnittest, SupportsDeviceWrongProfiles) {
  auto configurator = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE0, {0, 0}, false));

  auto vd_mock = MakeComPtr<D3D11VideoDeviceMock>();
  EXPECT_CALL(*vd_mock.Get(), GetVideoDecoderProfileCount())
      .Times(1)
      .WillOnce(Return(2));
  EXPECT_CALL(*vd_mock.Get(), GetVideoDecoderProfile(0, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(D3D11_DECODER_PROFILE_HEVC_VLD_MAIN),
                      Return(S_OK)));
  EXPECT_CALL(*vd_mock.Get(), GetVideoDecoderProfile(1, _))
      .Times(1)
      .WillOnce(
          DoAll(SetArgPointee<1>(D3D11_DECODER_PROFILE_VC1_VLD), Return(S_OK)));

  EXPECT_FALSE(configurator->SupportsDevice(vd_mock));
}

TEST_F(D3D11DecoderConfiguratorUnittest, SupportsDeviceCorrectProfile) {
  auto configurator = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE0, {0, 0}, false));

  auto vd_mock = MakeComPtr<D3D11VideoDeviceMock>();
  EXPECT_CALL(*vd_mock.Get(), GetVideoDecoderProfileCount())
      .Times(1)
      .WillOnce(Return(5));
  EXPECT_CALL(*vd_mock.Get(), GetVideoDecoderProfile(4, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0),
                      Return(S_OK)));

  EXPECT_TRUE(configurator->SupportsDevice(vd_mock));
}

}  // namespace media
