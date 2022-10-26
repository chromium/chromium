// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "media/base/media_util.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/gpu/windows/d3d11_texture_selector.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
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
  class MockFormatSupportChecker : public FormatSupportChecker {
   public:
    MockFormatSupportChecker() : FormatSupportChecker(nullptr) {}
    ~MockFormatSupportChecker() override = default;
    bool Initialize() override { return true; }

    MOCK_CONST_METHOD1(CheckOutputFormatSupport, bool(DXGI_FORMAT));
  };

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

  enum class ZeroCopyEnabled { kFalse = 0, kTrue = 1 };
  enum class ZeroCopyDisabledByWorkaround { kFalse = 0, kTrue = 1 };

  std::unique_ptr<TextureSelector> CreateWithDefaultGPUInfo(
      DXGI_FORMAT decoder_output_format,
      ZeroCopyEnabled zero_copy_enabled = ZeroCopyEnabled::kTrue,
      TextureSelector::HDRMode hdr_mode = TextureSelector::HDRMode::kSDROnly,
      ZeroCopyDisabledByWorkaround zero_copy_disabled_by_workaround =
          ZeroCopyDisabledByWorkaround::kFalse) {
    gpu::GpuPreferences prefs;
    prefs.enable_zero_copy_dxgi_video =
        zero_copy_enabled == ZeroCopyEnabled::kTrue;
    gpu::GpuDriverBugWorkarounds workarounds;
    workarounds.disable_dxgi_zero_copy_video =
        zero_copy_disabled_by_workaround == ZeroCopyDisabledByWorkaround::kTrue;
    auto media_log = std::make_unique<NullMediaLog>();
    return TextureSelector::Create(prefs, workarounds, decoder_output_format,
                                   hdr_mode, &format_checker_, nullptr, nullptr,
                                   media_log.get(), gfx::ColorSpace());
  }

  // Set the format checker to succeed any check, except for |disallowed|.
  void AllowFormatCheckerSupportExcept(std::vector<DXGI_FORMAT> disallowed) {
    ON_CALL(format_checker_, CheckOutputFormatSupport(_))
        .WillByDefault(Return(true));
    for (auto format : disallowed) {
      ON_CALL(format_checker_, CheckOutputFormatSupport(format))
          .WillByDefault(Return(false));
    }
  }

  MockFormatSupportChecker format_checker_;
};

TEST_F(D3D11TextureSelectorUnittest, NV12BindsToNV12) {
  // Nothing should ask about VideoProcessor support, since we're binding.
  EXPECT_CALL(format_checker_, CheckOutputFormatSupport(_)).Times(0);
  auto tex_sel = CreateWithDefaultGPUInfo(DXGI_FORMAT_NV12);

  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_NV12);
  EXPECT_FALSE(tex_sel->WillCopyForTesting());
}

TEST_F(D3D11TextureSelectorUnittest, NV12CopiesToNV12WithoutSharingSupport) {
  AllowFormatCheckerSupportExcept({});
  auto tex_sel =
      CreateWithDefaultGPUInfo(DXGI_FORMAT_NV12, ZeroCopyEnabled::kFalse);

  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_NV12);
  EXPECT_TRUE(tex_sel->WillCopyForTesting());
}

TEST_F(D3D11TextureSelectorUnittest, NV12CopiesToNV12WithWorkaround) {
  AllowFormatCheckerSupportExcept({});
  auto tex_sel = CreateWithDefaultGPUInfo(
      DXGI_FORMAT_NV12, ZeroCopyEnabled::kTrue,
      TextureSelector::HDRMode::kSDROnly, ZeroCopyDisabledByWorkaround::kTrue);

  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_NV12);
  EXPECT_TRUE(tex_sel->WillCopyForTesting());
}

TEST_F(D3D11TextureSelectorUnittest, P010BindsP010InHDR) {
  // Allow all output formats, p010 should be the default.
  AllowFormatCheckerSupportExcept({});
  auto tex_sel =
      CreateWithDefaultGPUInfo(DXGI_FORMAT_P010, ZeroCopyEnabled::kTrue,
                               TextureSelector::HDRMode::kSDROrHDR);

  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_P016LE);
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_P010);
  EXPECT_FALSE(tex_sel->WillCopyForTesting());
  // TODO(liberato): Check output color space.
}

TEST_F(D3D11TextureSelectorUnittest, P010CopiesTo16bitRGBInHDR) {
  // 16 bit float should be the second choice after p010 zero copy.
  AllowFormatCheckerSupportExcept({DXGI_FORMAT_P010});
  auto tex_sel =
      CreateWithDefaultGPUInfo(DXGI_FORMAT_P010, ZeroCopyEnabled::kFalse,
                               TextureSelector::HDRMode::kSDROrHDR);

  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_RGBAF16);
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_R16G16B16A16_FLOAT);
  EXPECT_TRUE(tex_sel->WillCopyForTesting());
}

TEST_F(D3D11TextureSelectorUnittest, P010CopiesTo10BitRGBInHDR) {
  // 10 bit unorm should be the third and final choice.
  AllowFormatCheckerSupportExcept(
      {DXGI_FORMAT_P010, DXGI_FORMAT_R16G16B16A16_FLOAT});
  auto tex_sel =
      CreateWithDefaultGPUInfo(DXGI_FORMAT_P010, ZeroCopyEnabled::kFalse,
                               TextureSelector::HDRMode::kSDROrHDR);

  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_XB30);
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_R10G10B10A2_UNORM);
  EXPECT_TRUE(tex_sel->WillCopyForTesting());
}

TEST_F(D3D11TextureSelectorUnittest, P010CopiesTo8BitInSDR) {
  // Should copy to 8 bit RGB if the video processor can't handle P010, if we're
  // not in HDR mode.
  AllowFormatCheckerSupportExcept({DXGI_FORMAT_P010});
  auto tex_sel =
      CreateWithDefaultGPUInfo(DXGI_FORMAT_P010, ZeroCopyEnabled::kTrue,
                               TextureSelector::HDRMode::kSDROnly);

  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_ARGB);
  // Note that this might also produce 8 bit rgb, but for now always
  // tries for fp16.
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_B8G8R8A8_UNORM);
  EXPECT_TRUE(tex_sel->WillCopyForTesting());
  // TODO(liberato): Check output color space, somehow.
}

TEST_F(D3D11TextureSelectorUnittest, P010BindsToP010InSDR) {
  // Should bind P010 if the video processor can handle P010, if we're not
  // int HDR mode.
  AllowFormatCheckerSupportExcept({});
  auto tex_sel =
      CreateWithDefaultGPUInfo(DXGI_FORMAT_P010, ZeroCopyEnabled::kTrue,
                               TextureSelector::HDRMode::kSDROnly);

  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_P016LE);
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_P010);
  EXPECT_FALSE(tex_sel->WillCopyForTesting());
}

}  // namespace media
