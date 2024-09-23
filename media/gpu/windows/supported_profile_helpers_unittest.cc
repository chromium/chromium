// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/supported_profile_helpers.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <initguid.h>
#include <map>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "media/base/test_helpers.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/gpu/windows/av1_guids.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithArgs;

namespace {

using PciId = std::pair<uint16_t, uint16_t>;
constexpr PciId kLegacyIntelGpu = {0x8086, 0x102};
constexpr PciId kRecentIntelGpu = {0x8086, 0x100};
constexpr PciId kLegacyAmdGpu = {0x1022, 0x130f};
constexpr PciId kRecentAmdGpu = {0x1022, 0x130e};

constexpr gfx::Size kMinResolution(64, 64);
constexpr gfx::Size kFullHd(1920, 1088);
constexpr gfx::Size kSquare4k(4096, 4096);
constexpr gfx::Size kSquare8k(8192, 8192);

}  // namespace

namespace media {

constexpr VideoCodecProfile kSupportedH264Profiles[] = {
    H264PROFILE_BASELINE, H264PROFILE_MAIN, H264PROFILE_HIGH};

class SupportedResolutionResolverTest : public ::testing::Test {
 public:
  void SetUp() override {
    gpu_workarounds_.disable_dxgi_zero_copy_video = false;
    mock_d3d11_device_ = MakeComPtr<NiceMock<D3D11DeviceMock>>();

    mock_dxgi_device_ = MakeComPtr<NiceMock<DXGIDeviceMock>>();
    ON_CALL(*mock_d3d11_device_.Get(), QueryInterface(IID_IDXGIDevice, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(mock_dxgi_device_.Get()));

    mock_d3d11_video_device_ = MakeComPtr<NiceMock<D3D11VideoDeviceMock>>();
    ON_CALL(*mock_d3d11_device_.Get(), QueryInterface(IID_ID3D11VideoDevice, _))
        .WillByDefault(
            SetComPointeeAndReturnOk<1>(mock_d3d11_video_device_.Get()));

    mock_dxgi_adapter_ = MakeComPtr<NiceMock<DXGIAdapterMock>>();
    ON_CALL(*mock_dxgi_device_.Get(), GetAdapter(_))
        .WillByDefault(SetComPointeeAndReturnOk<0>(mock_dxgi_adapter_.Get()));

    SetGpuProfile(kRecentIntelGpu);
    SetMaxResolution(D3D11_DECODER_PROFILE_H264_VLD_NOFGT, kSquare4k);
  }

  void SetMaxResolution(const GUID& g, const gfx::Size& max_res) {
    max_size_for_guids_[g] = max_res;
    ON_CALL(*mock_d3d11_video_device_.Get(), GetVideoDecoderConfigCount(_, _))
        .WillByDefault(
            WithArgs<0, 1>(Invoke([this](const D3D11_VIDEO_DECODER_DESC* desc,
                                         UINT* count) -> HRESULT {
              *count = 0;
              const auto& itr = this->max_size_for_guids_.find(desc->Guid);
              if (itr == this->max_size_for_guids_.end())
                return E_FAIL;
              const gfx::Size max = itr->second;
              if (max.height() < 0 || max.width() < 0)
                return E_FAIL;
              if (static_cast<UINT>(max.height()) < desc->SampleHeight)
                return E_FAIL;
              if (static_cast<UINT>(max.width()) < desc->SampleWidth)
                return S_OK;
              *count = 1;
              return S_OK;
            })));
  }

  void EnableDecoders(const std::vector<GUID>& decoder_guids) {
    ON_CALL(*mock_d3d11_video_device_.Get(), GetVideoDecoderProfileCount())
        .WillByDefault(Return(decoder_guids.size()));

    // Note that we don't check if the guid in the config actually matches
    // |decoder_profile|.  Perhaps we should.
    ON_CALL(*mock_d3d11_video_device_.Get(), GetVideoDecoderProfile(_, _))
        .WillByDefault(WithArgs<0, 1>(
            Invoke([decoder_guids](UINT p_idx, GUID* guid) -> HRESULT {
              if (p_idx >= decoder_guids.size())
                return E_FAIL;
              *guid = decoder_guids.at(p_idx);
              return S_OK;
            })));
  }

  void SetGpuProfile(std::pair<uint16_t, uint16_t> vendor_and_gpu) {
    mock_adapter_desc_.DeviceId = static_cast<UINT>(vendor_and_gpu.second);
    mock_adapter_desc_.VendorId = static_cast<UINT>(vendor_and_gpu.first);

    ON_CALL(*mock_dxgi_adapter_.Get(), GetDesc(_))
        .WillByDefault(
            DoAll(SetArgPointee<0>(mock_adapter_desc_), Return(S_OK)));
  }

  void AssertDefaultSupport(
      const SupportedResolutionRangeMap& supported_resolutions,
      size_t expected_size = 3u) {
    ASSERT_EQ(expected_size, supported_resolutions.size());
    for (const auto profile : kSupportedH264Profiles) {
      auto it = supported_resolutions.find(profile);
      ASSERT_NE(it, supported_resolutions.end());
      EXPECT_EQ(kMinResolution, it->second.min_resolution);
      EXPECT_EQ(kFullHd, it->second.max_landscape_resolution);
      EXPECT_EQ(gfx::Size(), it->second.max_portrait_resolution);
    }
  }

  void TestDecoderSupport(const GUID& decoder,
                          VideoCodecProfile profile,
                          const gfx::Size& max_res = kSquare4k,
                          const gfx::Size& max_landscape_res = kSquare4k,
                          const gfx::Size& max_portrait_res = kSquare4k) {
    EnableDecoders({decoder});
    SetMaxResolution(decoder, max_res);

    const auto supported_resolutions = GetSupportedD3D11VideoDecoderResolutions(
        mock_d3d11_device_, gpu_workarounds_);
    AssertDefaultSupport(supported_resolutions,
                         std::size(kSupportedH264Profiles) + 1);

    auto it = supported_resolutions.find(profile);
    ASSERT_NE(it, supported_resolutions.end());
    EXPECT_EQ(kMinResolution, it->second.min_resolution);
    EXPECT_EQ(max_landscape_res, it->second.max_landscape_resolution);
    EXPECT_EQ(max_portrait_res, it->second.max_portrait_resolution);
  }

  Microsoft::WRL::ComPtr<D3D11DeviceMock> mock_d3d11_device_;
  Microsoft::WRL::ComPtr<DXGIAdapterMock> mock_dxgi_adapter_;
  Microsoft::WRL::ComPtr<DXGIDeviceMock> mock_dxgi_device_;
  Microsoft::WRL::ComPtr<D3D11VideoDeviceMock> mock_d3d11_video_device_;
  DXGI_ADAPTER_DESC mock_adapter_desc_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;

  struct GUIDComparison {
    bool operator()(const GUID& a, const GUID& b) const {
      return memcmp(&a, &b, sizeof(GUID)) < 0;
    }
  };
  base::flat_map<GUID, gfx::Size, GUIDComparison> max_size_for_guids_;
};

TEST_F(SupportedResolutionResolverTest, WorkaroundsDisableAv1) {
  // Enable the av1 decoder.
  EnableDecoders({DXVA_ModeAV1_VLD_Profile0});
  SetMaxResolution(DXVA_ModeAV1_VLD_Profile0, kSquare8k);

  gpu_workarounds_.disable_accelerated_av1_decode = true;
  const auto supported_resolutions = GetSupportedD3D11VideoDecoderResolutions(
      mock_d3d11_device_, gpu_workarounds_);
  auto av1_supported_res = supported_resolutions.find(AV1PROFILE_PROFILE_MAIN);

  // There should be no supported av1 resolutions.
  ASSERT_EQ(av1_supported_res, supported_resolutions.end());
}

TEST_F(SupportedResolutionResolverTest, HasH264SupportByDefault) {
  AssertDefaultSupport(
      GetSupportedD3D11VideoDecoderResolutions(nullptr, gpu_workarounds_));

  SetGpuProfile(kLegacyIntelGpu);
  AssertDefaultSupport(GetSupportedD3D11VideoDecoderResolutions(
      mock_d3d11_device_, gpu_workarounds_));

  SetGpuProfile(kLegacyAmdGpu);
  AssertDefaultSupport(GetSupportedD3D11VideoDecoderResolutions(
      mock_d3d11_device_, gpu_workarounds_));
}

TEST_F(SupportedResolutionResolverTest, WorkaroundsDisableVpx) {
  gpu_workarounds_.disable_accelerated_vp8_decode = true;
  gpu_workarounds_.disable_accelerated_vp9_decode = true;
  EnableDecoders({D3D11_DECODER_PROFILE_VP8_VLD,
                  D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0,
                  D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2});

  AssertDefaultSupport(GetSupportedD3D11VideoDecoderResolutions(
      mock_d3d11_device_, gpu_workarounds_));
}

TEST_F(SupportedResolutionResolverTest, WorkaroundsDisableVp92) {
  gpu_workarounds_.disable_accelerated_vp9_profile2_decode = true;
  EnableDecoders({D3D11_DECODER_PROFILE_VP8_VLD,
                  D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0,
                  D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2});
  const auto supported_resolutions = GetSupportedD3D11VideoDecoderResolutions(
      mock_d3d11_device_, gpu_workarounds_);

  // There should be no supported vp9.2 resolutions.
  ASSERT_EQ(supported_resolutions.find(VP9PROFILE_PROFILE2),
            supported_resolutions.end());

  // vp9.0 should still be available.
  ASSERT_NE(supported_resolutions.find(VP9PROFILE_PROFILE0),
            supported_resolutions.end());
}

TEST_F(SupportedResolutionResolverTest, H264Supports4k) {
  EnableDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT});
  const auto supported_resolutions = GetSupportedD3D11VideoDecoderResolutions(
      mock_d3d11_device_, gpu_workarounds_);

  ASSERT_EQ(3u, supported_resolutions.size());
  for (const auto profile : kSupportedH264Profiles) {
    auto it = supported_resolutions.find(profile);
    ASSERT_NE(it, supported_resolutions.end());
    EXPECT_EQ(kMinResolution, it->second.min_resolution);
    EXPECT_EQ(kSquare4k, it->second.max_landscape_resolution);
    EXPECT_EQ(kSquare4k, it->second.max_portrait_resolution);
  }
}

TEST_F(SupportedResolutionResolverTest, VP8Supports4k) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kMediaFoundationVP8Decoding);

  EnableDecoders({D3D11_DECODER_PROFILE_VP8_VLD});
  SetMaxResolution(D3D11_DECODER_PROFILE_VP8_VLD, kSquare4k);

  const auto supported_resolutions = GetSupportedD3D11VideoDecoderResolutions(
      mock_d3d11_device_, gpu_workarounds_);
  auto it = supported_resolutions.find(VP8PROFILE_ANY);
  ASSERT_NE(it, supported_resolutions.end());
  EXPECT_EQ(kSquare4k, it->second.max_landscape_resolution);
  EXPECT_EQ(kSquare4k, it->second.max_portrait_resolution);

  constexpr gfx::Size kMinVp8Resolution = gfx::Size(640, 480);
  EXPECT_EQ(kMinVp8Resolution, it->second.min_resolution);
}

TEST_F(SupportedResolutionResolverTest, VP9Profile0Supports8k) {
  TestDecoderSupport(D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0,
                     VP9PROFILE_PROFILE0, kSquare8k, kSquare8k, kSquare8k);
}

TEST_F(SupportedResolutionResolverTest, VP9Profile2Supports8k) {
  TestDecoderSupport(D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2,
                     VP9PROFILE_PROFILE2, kSquare8k, kSquare8k, kSquare8k);
}

TEST_F(SupportedResolutionResolverTest, MultipleCodecs) {
  SetGpuProfile(kRecentAmdGpu);

  // H.264 and VP9.0 are the most common supported codecs.
  EnableDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
                  D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0});
  SetMaxResolution(D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0, kSquare8k);

  const auto supported_resolutions = GetSupportedD3D11VideoDecoderResolutions(
      mock_d3d11_device_, gpu_workarounds_);

  ASSERT_EQ(std::size(kSupportedH264Profiles) + 1,
            supported_resolutions.size());
  for (const auto profile : kSupportedH264Profiles) {
    auto it = supported_resolutions.find(profile);
    ASSERT_NE(it, supported_resolutions.end());
    EXPECT_EQ(kMinResolution, it->second.min_resolution);
    EXPECT_EQ(kSquare4k, it->second.max_landscape_resolution);
    EXPECT_EQ(kSquare4k, it->second.max_portrait_resolution);
  }

  auto it = supported_resolutions.find(VP9PROFILE_PROFILE0);
  ASSERT_NE(it, supported_resolutions.end());
  EXPECT_EQ(kMinResolution, it->second.min_resolution);
  EXPECT_EQ(kSquare8k, it->second.max_landscape_resolution);
  EXPECT_EQ(kSquare8k, it->second.max_portrait_resolution);
}

TEST_F(SupportedResolutionResolverTest, AV1ProfileMainSupports8k) {
  TestDecoderSupport(DXVA_ModeAV1_VLD_Profile0, AV1PROFILE_PROFILE_MAIN,
                     kSquare8k, kSquare8k, kSquare8k);
}

TEST_F(SupportedResolutionResolverTest, AV1ProfileHighSupports8k) {
  TestDecoderSupport(DXVA_ModeAV1_VLD_Profile1, AV1PROFILE_PROFILE_HIGH,
                     kSquare8k, kSquare8k, kSquare8k);
}

TEST_F(SupportedResolutionResolverTest, AV1ProfileProSupports8k) {
  TestDecoderSupport(DXVA_ModeAV1_VLD_Profile2, AV1PROFILE_PROFILE_PRO,
                     kSquare8k, kSquare8k, kSquare8k);
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
TEST_F(SupportedResolutionResolverTest, H265Supports8kIfEnabled) {
  EnableDecoders({D3D11_DECODER_PROFILE_HEVC_VLD_MAIN});
  SetMaxResolution(D3D11_DECODER_PROFILE_HEVC_VLD_MAIN, kSquare8k);
  const auto resolutions_for_feature = GetSupportedD3D11VideoDecoderResolutions(
      mock_d3d11_device_, gpu_workarounds_);
  ASSERT_EQ(5u, resolutions_for_feature.size());
  const auto it = resolutions_for_feature.find(HEVCPROFILE_MAIN);
  ASSERT_NE(it, resolutions_for_feature.end());
  ASSERT_EQ(it->second.max_landscape_resolution, kSquare8k);
  ASSERT_EQ(it->second.max_portrait_resolution, kSquare8k);
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

}  // namespace media
