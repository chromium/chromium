// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/supported_profile_helpers.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <initguid.h>
#include <map>
#include <utility>

#include "base/win/windows_version.h"
#include "media/base/test_helpers.h"
#include "media/base/win/d3d11_mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithArgs;

#define DONT_RUN_ON_WIN_7()                                  \
  do {                                                       \
    if (base::win::GetVersion() <= base::win::Version::WIN7) \
      return;                                                \
  } while (0)

HRESULT SetIfSizeLessThan(D3D11_VIDEO_DECODER_DESC* desc, UINT* count) {
  *count = 1;
  return S_OK;
}

namespace media {

class SupportedResolutionResolverTest : public ::testing::Test {
 public:
  const std::pair<uint16_t, uint16_t> LegacyIntelGPU = {0x8086, 0x102};
  const std::pair<uint16_t, uint16_t> RecentIntelGPU = {0x8086, 0x100};
  const std::pair<uint16_t, uint16_t> LegacyAMDGPU = {0x1022, 0x130f};
  const std::pair<uint16_t, uint16_t> RecentAMDGPU = {0x1022, 0x130e};

  const ResolutionPair ten_eighty = {{1920, 1080}, {1080, 1920}};
  const ResolutionPair zero = {{0, 0}, {0, 0}};
  const ResolutionPair tall4k = {{4096, 2304}, {2304, 4096}};
  const ResolutionPair eightKsquare = {{8192, 8192}, {8192, 8192}};

  void SetUp() override {
    gpu_workarounds_.disable_dxgi_zero_copy_video = false;
    mock_d3d11_device_ = CreateD3D11Mock<NiceMock<D3D11DeviceMock>>();

    mock_dxgi_device_ = CreateD3D11Mock<NiceMock<DXGIDeviceMock>>();
    ON_CALL(*mock_d3d11_device_.Get(), QueryInterface(IID_IDXGIDevice, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(mock_dxgi_device_.Get()));

    mock_d3d11_video_device_ =
        CreateD3D11Mock<NiceMock<D3D11VideoDeviceMock>>();
    ON_CALL(*mock_d3d11_device_.Get(), QueryInterface(IID_ID3D11VideoDevice, _))
        .WillByDefault(
            SetComPointeeAndReturnOk<1>(mock_d3d11_video_device_.Get()));

    mock_dxgi_adapter_ = CreateD3D11Mock<NiceMock<DXGIAdapterMock>>();
    ON_CALL(*mock_dxgi_device_.Get(), GetAdapter(_))
        .WillByDefault(SetComPointeeAndReturnOk<0>(mock_dxgi_adapter_.Get()));

    SetGPUProfile(RecentIntelGPU);
    SetMaxResolutionForGUID(D3D11_DECODER_PROFILE_H264_VLD_NOFGT, {4096, 4096});
  }

  void SetMaxResolutionForGUID(const GUID& g, const gfx::Size& max_res) {
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

  void SetGPUProfile(std::pair<uint16_t, uint16_t> vendor_and_gpu) {
    mock_adapter_desc_.DeviceId = static_cast<UINT>(vendor_and_gpu.second);
    mock_adapter_desc_.VendorId = static_cast<UINT>(vendor_and_gpu.first);

    ON_CALL(*mock_dxgi_adapter_.Get(), GetDesc(_))
        .WillByDefault(
            DoAll(SetArgPointee<0>(mock_adapter_desc_), Return(S_OK)));
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
  std::map<GUID, gfx::Size, GUIDComparison> max_size_for_guids_;
};

TEST_F(SupportedResolutionResolverTest, NoDeviceAllDefault) {
  DONT_RUN_ON_WIN_7();

  ResolutionPair h264_res_expected = {{1, 2}, {3, 4}};
  ResolutionPair h264_res = {{1, 2}, {3, 4}};
  ResolutionPair vp9_0_res;
  ResolutionPair vp9_2_res;
  GetResolutionsForDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT}, nullptr,
                            gpu_workarounds_, &h264_res, &vp9_0_res,
                            &vp9_2_res);

  ASSERT_EQ(h264_res, h264_res_expected);
  ASSERT_EQ(vp9_0_res, zero);
  ASSERT_EQ(vp9_0_res, zero);
}

TEST_F(SupportedResolutionResolverTest, LegacyGPUAllDefault) {
  DONT_RUN_ON_WIN_7();

  SetGPUProfile(LegacyIntelGPU);

  ResolutionPair h264_res_expected = {{1, 2}, {3, 4}};
  ResolutionPair h264_res = {{1, 2}, {3, 4}};
  ResolutionPair vp9_0_res;
  ResolutionPair vp9_2_res;
  GetResolutionsForDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT},
                            mock_d3d11_device_, gpu_workarounds_, &h264_res,
                            &vp9_0_res, &vp9_2_res);

  ASSERT_EQ(h264_res, h264_res_expected);
  ASSERT_EQ(vp9_2_res, zero);
  ASSERT_EQ(vp9_0_res, zero);
}

TEST_F(SupportedResolutionResolverTest, WorkaroundsDisableVpx) {
  DONT_RUN_ON_WIN_7();

  gpu_workarounds_.disable_dxgi_zero_copy_video = true;
  EnableDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT});

  ResolutionPair h264_res;
  ResolutionPair vp9_0_res;
  ResolutionPair vp9_2_res;
  GetResolutionsForDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT},
                            mock_d3d11_device_, gpu_workarounds_, &h264_res,
                            &vp9_0_res, &vp9_2_res);

  ASSERT_EQ(h264_res, tall4k);

  ASSERT_EQ(vp9_0_res, zero);

  ASSERT_EQ(vp9_2_res, zero);
}

TEST_F(SupportedResolutionResolverTest, VP9_0Supports8k) {
  DONT_RUN_ON_WIN_7();

  EnableDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
                  D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0});
  SetMaxResolutionForGUID(D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0, {8192, 8192});

  ResolutionPair h264_res;
  ResolutionPair vp9_0_res;
  ResolutionPair vp9_2_res;
  GetResolutionsForDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT},
                            mock_d3d11_device_, gpu_workarounds_, &h264_res,
                            &vp9_0_res, &vp9_2_res);

  ASSERT_EQ(h264_res, tall4k);

  ASSERT_EQ(vp9_0_res, eightKsquare);

  ASSERT_EQ(vp9_2_res, zero);
}

TEST_F(SupportedResolutionResolverTest, BothVP9ProfilesSupported) {
  DONT_RUN_ON_WIN_7();

  EnableDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
                  D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0,
                  D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2});
  SetMaxResolutionForGUID(D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0, {8192, 8192});
  SetMaxResolutionForGUID(D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2,
                          {8192, 8192});

  ResolutionPair h264_res;
  ResolutionPair vp9_0_res;
  ResolutionPair vp9_2_res;
  GetResolutionsForDecoders({D3D11_DECODER_PROFILE_H264_VLD_NOFGT},
                            mock_d3d11_device_, gpu_workarounds_, &h264_res,
                            &vp9_0_res, &vp9_2_res);

  ASSERT_EQ(h264_res, tall4k);

  ASSERT_EQ(vp9_0_res, eightKsquare);

  ASSERT_EQ(vp9_2_res, eightKsquare);
}

}  // namespace media
