// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "media/base/media_util.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/gpu/windows/d3d11_texture_selector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Values;

namespace media {

class D3D11TextureSelectorUnittest : public ::testing::Test {
 public:
  VideoDecoderConfig CreateDecoderConfig(VideoCodecProfile profile,
                                         gfx::Size size,
                                         bool encrypted) {
    VideoDecoderConfig result;
    result.Initialize(
        kUnknownVideoCodec,  // It doesn't matter because it won't be used.
        profile, VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
        kNoTransformation, size, {}, {}, {},
        encrypted ? EncryptionScheme::kCenc : EncryptionScheme::kUnencrypted);
    return result;
  }

  std::unique_ptr<TextureSelector> CreateWithDefaultGPUInfo(
      const VideoDecoderConfig& config,
      bool zero_copy_enabled = true) {
    gpu::GpuPreferences prefs;
    prefs.enable_zero_copy_dxgi_video = zero_copy_enabled;
    gpu::GpuDriverBugWorkarounds workarounds;
    workarounds.disable_dxgi_zero_copy_video = false;
    auto media_log = std::make_unique<NullMediaLog>();
    return TextureSelector::Create(prefs, workarounds, config, media_log.get());
  }
};

TEST_F(D3D11TextureSelectorUnittest, VP9Profile0RightFormats) {
  auto tex_sel = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE0, {0, 0}, false));

  EXPECT_EQ(tex_sel->DecoderGuid(), D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0);
  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(tex_sel->DecoderDescriptor()->OutputFormat, DXGI_FORMAT_NV12);
}

TEST_F(D3D11TextureSelectorUnittest, VP9Profile2RightFormats) {
  auto tex_sel = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE2, {0, 0}, false), false);

  EXPECT_EQ(tex_sel->DecoderGuid(),
            D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2);
  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(tex_sel->DecoderDescriptor()->OutputFormat, DXGI_FORMAT_P010);
}

TEST_F(D3D11TextureSelectorUnittest, SupportsDeviceNoProfiles) {
  auto tex_sel = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE0, {0, 0}, false));

  auto vd_mock = CreateD3D11Mock<D3D11VideoDeviceMock>();
  EXPECT_CALL(*vd_mock.Get(), GetVideoDecoderProfileCount())
      .Times(1)
      .WillOnce(Return(0));

  EXPECT_FALSE(tex_sel->SupportsDevice(vd_mock));
}

TEST_F(D3D11TextureSelectorUnittest, SupportsDeviceWrongProfiles) {
  auto tex_sel = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE0, {0, 0}, false));

  auto vd_mock = CreateD3D11Mock<D3D11VideoDeviceMock>();
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

  EXPECT_FALSE(tex_sel->SupportsDevice(vd_mock));
}

TEST_F(D3D11TextureSelectorUnittest, SupportsDeviceCorrectProfile) {
  auto tex_sel = CreateWithDefaultGPUInfo(
      CreateDecoderConfig(VP9PROFILE_PROFILE0, {0, 0}, false));

  auto vd_mock = CreateD3D11Mock<D3D11VideoDeviceMock>();
  EXPECT_CALL(*vd_mock.Get(), GetVideoDecoderProfileCount())
      .Times(1)
      .WillOnce(Return(5));
  EXPECT_CALL(*vd_mock.Get(), GetVideoDecoderProfile(4, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0),
                      Return(S_OK)));

  EXPECT_TRUE(tex_sel->SupportsDevice(vd_mock));
}

}  // namespace media
