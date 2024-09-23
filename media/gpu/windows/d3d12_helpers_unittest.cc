// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/windows/d3d12_helpers.h"

#include <numeric>
#include <vector>

#include "base/rand_util.h"
#include "media/base/video_codecs.h"
#include "media/base/win/d3d12_mocks.h"
#include "media/gpu/windows/format_utils.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace media {

class D3D12Helpers : public ::testing::Test {
 public:
  void SetUp() override {
    device_ = MakeComPtr<NiceMock<D3D12DeviceMock>>();
    ON_CALL(*device_.Get(), OpenSharedHandle(_, _, _))
        .WillByDefault(Invoke([this](HANDLE handle, REFIID riid, void** ppv) {
          Microsoft::WRL::ComPtr<D3D12ResourceMock> d3d12_resource =
              MakeComPtr<NiceMock<D3D12ResourceMock>>();
          ON_CALL(*d3d12_resource.Get(), GetDesc())
              .WillByDefault(Return(D3D12_RESOURCE_DESC{
                  .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                  .Width = width_,
                  .Height = height_,
                  .DepthOrArraySize = 1,
                  .MipLevels = 1,
                  .Format = format_,
                  .SampleDesc = {1},
              }));
          *ppv = d3d12_resource.Detach();
          return S_OK;
        }));
  }

  ComD3D12Resource CreateD3D12Resource() {
    // D3D12DeviceMock can open an empty handle
    ComD3D12Resource d3d12_resource;
    HRESULT hr =
        device_->OpenSharedHandle(nullptr, IID_PPV_ARGS(&d3d12_resource));
    EXPECT_EQ(hr, S_OK);
    EXPECT_TRUE(d3d12_resource);
    return d3d12_resource;
  }

 protected:
  const UINT width_ = 1280;
  const UINT height_ = 720;
  const VideoCodecProfile profile_ = H264PROFILE_MAIN;
  const uint8_t bitdepth_ = 8;
  const VideoChromaSampling chroma_sampling_ = VideoChromaSampling::k420;
  const GUID guid_ =
      GetD3D12VideoDecodeGUID(profile_, bitdepth_, chroma_sampling_);
  const DXGI_FORMAT format_ = GetOutputDXGIFormat(bitdepth_, chroma_sampling_);
  Microsoft::WRL::ComPtr<NiceMock<D3D12DeviceMock>> device_;
};

TEST_F(D3D12Helpers, D3D12ReferenceFrameList) {
  D3D12ReferenceFrameList reference_frame_list(nullptr);
  // A random order of indices, but 0 goes first.
  std::vector<int> indices(8);
  std::iota(indices.begin(), indices.end(), 0);
  base::RandomShuffle(indices.begin() + 1, indices.end());
  for (size_t index : indices) {
    ComD3D12Resource resource = CreateD3D12Resource();
    reference_frame_list.emplace(index, resource.Get(), 0);
    D3D12_VIDEO_DECODE_REFERENCE_FRAMES reference_frames;
    reference_frame_list.WriteTo(&reference_frames);
    EXPECT_GT(reference_frames.NumTexture2Ds, index);
    EXPECT_EQ(reference_frames.ppTexture2Ds[index], resource.Get());
  }
}

TEST_F(D3D12Helpers, CreateD3D12TransitionBarriersForAllPlanes) {
  ComD3D12Resource resource = CreateD3D12Resource();
  const size_t num_planes = GetFormatPlaneCount(format_);
  auto barriers = CreateD3D12TransitionBarriersForAllPlanes(
      resource.Get(), 0, D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_VIDEO_DECODE_READ);
  EXPECT_EQ(barriers.size(), num_planes);
  for (size_t i = 0; i < num_planes; i++) {
    D3D12_RESOURCE_BARRIER barrier = barriers[i];
    EXPECT_EQ(barrier.Type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
    EXPECT_EQ(barrier.Transition.pResource, resource.Get());
    EXPECT_EQ(barrier.Transition.Subresource, i);
    EXPECT_EQ(barrier.Transition.StateBefore, D3D12_RESOURCE_STATE_COMMON);
    EXPECT_EQ(barrier.Transition.StateAfter,
              D3D12_RESOURCE_STATE_VIDEO_DECODE_READ);
  }
}

TEST_F(D3D12Helpers, GetD3D12VideoDecodeGUID) {
  EXPECT_EQ(GetD3D12VideoDecodeGUID(VP9PROFILE_PROFILE0, 8,
                                    VideoChromaSampling::k420),
            D3D12_VIDEO_DECODE_PROFILE_VP9);
  EXPECT_EQ(GetD3D12VideoDecodeGUID(AV1PROFILE_PROFILE_MAIN, 8,
                                    VideoChromaSampling::k420),
            D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE0);
  EXPECT_EQ(
      GetD3D12VideoDecodeGUID(H264PROFILE_MAIN, 8, VideoChromaSampling::k420),
      D3D12_VIDEO_DECODE_PROFILE_H264);
  EXPECT_EQ(
      GetD3D12VideoDecodeGUID(H264PROFILE_HIGH, 8, VideoChromaSampling::k420),
      D3D12_VIDEO_DECODE_PROFILE_H264);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  EXPECT_EQ(
      GetD3D12VideoDecodeGUID(HEVCPROFILE_MAIN, 8, VideoChromaSampling::k420),
      D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN);
  EXPECT_EQ(GetD3D12VideoDecodeGUID(HEVCPROFILE_MAIN10, 10,
                                    VideoChromaSampling::k420),
            D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN10);
  EXPECT_EQ(GetD3D12VideoDecodeGUID(HEVCPROFILE_MAIN_STILL_PICTURE, 8,
                                    VideoChromaSampling::k420),
            D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN);
  EXPECT_EQ(
      GetD3D12VideoDecodeGUID(HEVCPROFILE_REXT, 8, VideoChromaSampling::k422),
      DXVA_ModeHEVC_VLD_Main422_10_Intel);
  EXPECT_EQ(
      GetD3D12VideoDecodeGUID(HEVCPROFILE_REXT, 10, VideoChromaSampling::k444),
      DXVA_ModeHEVC_VLD_Main444_10_Intel);
  EXPECT_EQ(
      GetD3D12VideoDecodeGUID(HEVCPROFILE_REXT, 12, VideoChromaSampling::k420),
      DXVA_ModeHEVC_VLD_Main12_Intel);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
}

}  // namespace media
