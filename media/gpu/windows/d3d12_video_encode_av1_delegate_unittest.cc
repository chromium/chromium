// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_AV1_DELEGATE_UNITTEST_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_AV1_DELEGATE_UNITTEST_H_

#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"

#include "base/rand_util.h"
#include "media/base/win/d3d12_mocks.h"
#include "media/base/win/d3d12_video_mocks.h"
#include "media/gpu/windows/d3d12_video_encode_delegate_unittest.h"
#include "media/gpu/windows/format_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

namespace media {

constexpr uint32_t kInputFrameWidth = 1280;
constexpr uint32_t kInputFrameHeight = 720;
constexpr VideoCodecProfile kAV1Profile = AV1PROFILE_PROFILE_MAIN;

class D3D12VideoEncodeAV1DelegateTest
    : public D3D12VideoEncodeDelegateTestBase {
 public:
  D3D12VideoEncodeAV1DelegateTest() = default;
  ~D3D12VideoEncodeAV1DelegateTest() override = default;

  void SetUp() override {
    device_ = MakeComPtr<NiceMock<D3D12DeviceMock>>();
    video_device3_ = MakeComPtr<NiceMock<D3D12VideoDevice3Mock>>();
    ON_CALL(*video_device3_.Get(), QueryInterface(IID_ID3D12Device, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(device_.Get()));
    ON_CALL(*video_device3_.Get(), QueryInterface(IID_ID3D12VideoDevice1, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(video_device3_.Get()));
    ON_CALL(*video_device3_.Get(), CheckFeatureSupport(_, _, _))
        .WillByDefault(Invoke([](D3D12_FEATURE_VIDEO feature,
                                 void* pFeatureSupportData,
                                 UINT FeatureSupportDataSize) -> HRESULT {
          if (feature == D3D12_FEATURE_VIDEO_ENCODER_CODEC) {
            auto* feature_data =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC*>(
                    pFeatureSupportData);
            feature_data->IsSupported =
                feature_data->Codec == D3D12_VIDEO_ENCODER_CODEC_AV1;
          } else if (feature == D3D12_FEATURE_VIDEO_ENCODER_PROFILE_LEVEL) {
            auto* feature_data =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL*>(
                    pFeatureSupportData);
            CHECK_EQ(feature_data->Codec, D3D12_VIDEO_ENCODER_CODEC_AV1);
            CHECK(feature_data->Profile.pAV1Profile);
            feature_data->IsSupported = *feature_data->Profile.pAV1Profile ==
                                        D3D12_VIDEO_ENCODER_AV1_PROFILE_MAIN;
          } else if (feature == D3D12_FEATURE_VIDEO_ENCODER_INPUT_FORMAT) {
            auto* feature_data =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT*>(
                    pFeatureSupportData);
            CHECK_EQ(feature_data->Codec, D3D12_VIDEO_ENCODER_CODEC_AV1);
            CHECK_EQ(*feature_data->Profile.pAV1Profile,
                     D3D12_VIDEO_ENCODER_AV1_PROFILE_MAIN);
            feature_data->IsSupported = true;
          } else if (feature ==
                     D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT) {
            auto* feature_data = static_cast<
                D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT*>(
                pFeatureSupportData);
            CHECK_EQ(feature_data->Codec, D3D12_VIDEO_ENCODER_CODEC_AV1);
            CHECK_EQ(*feature_data->Profile.pAV1Profile,
                     D3D12_VIDEO_ENCODER_AV1_PROFILE_MAIN);
            auto* av1_support = feature_data->CodecSupportLimits.pAV1Support;
            av1_support->SupportedFeatureFlags =
                D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_CDEF_FILTERING |
                D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_ORDER_HINT_TOOLS |
                D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_REDUCED_TX_SET;
            feature_data->IsSupported = true;
          } else if (feature == D3D12_FEATURE_VIDEO_ENCODER_SUPPORT1) {
            auto* feature_data =
                static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT1*>(
                    pFeatureSupportData);
            CHECK_EQ(feature_data->Codec, D3D12_VIDEO_ENCODER_CODEC_AV1);
            feature_data->SupportFlags =
                D3D12_VIDEO_ENCODER_SUPPORT_FLAG_GENERAL_SUPPORT_OK;
          }
          return S_OK;
        }));

    encoder_delegate_ =
        std::make_unique<D3D12VideoEncodeAV1Delegate>(video_device3_);
    encoder_delegate_->SetFactoriesForTesting(
        base::BindRepeating(&CreateVideoEncoderWrapper),
        base::BindRepeating(&CreateVideoProcessorWrapper));
  }

  VideoEncodeAccelerator::Config GetDefaultConfig() const {
    VideoEncodeAccelerator::Config vea_config(
        PIXEL_FORMAT_NV12, gfx::Size(kInputFrameWidth, kInputFrameHeight),
        kAV1Profile, Bitrate::ConstantBitrate(300000u),
        VideoEncodeAccelerator::kDefaultFramerate,
        VideoEncodeAccelerator::Config::StorageType::kShmem,
        VideoEncodeAccelerator::Config::ContentType::kCamera);
    vea_config.framerate = 30;
    vea_config.gop_length = 3000;
    return vea_config;
  }

  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
  Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> video_device3_;
};

TEST_F(D3D12VideoEncodeAV1DelegateTest, GetSupportedProfiles) {
  std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
      expected_profiles = {
          {AV1PROFILE_PROFILE_MAIN, {PIXEL_FORMAT_NV12, PIXEL_FORMAT_P010LE}}};
  EXPECT_CALL(*video_device3_.Get(), CheckFeatureSupport).Times(6);
  auto profiles =
      D3D12VideoEncodeAV1Delegate::GetSupportedProfiles(video_device3_.Get());
  EXPECT_EQ(profiles, expected_profiles);
}

TEST_F(D3D12VideoEncodeAV1DelegateTest, UnsupportedProfile) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  config.output_profile = AV1PROFILE_PROFILE_HIGH;
  EXPECT_EQ(encoder_delegate_->Initialize(config).code(),
            EncoderStatus::Codes::kEncoderUnsupportedProfile);
}

TEST_F(D3D12VideoEncodeAV1DelegateTest, EncodeFrame) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  EXPECT_TRUE(encoder_delegate_->Initialize(config).is_ok());
  for (int i = 0; i < 3; i++) {
    auto input_frame = MakeComPtr<NiceMock<D3D12ResourceMock>>();
    EXPECT_CALL(*input_frame.Get(), GetDesc())
        .WillOnce(Return(D3D12_RESOURCE_DESC{
            .Width = static_cast<UINT64>(config.input_visible_size.width()),
            .Height = static_cast<UINT>(config.input_visible_size.height()),
            .Format = VideoPixelFormatToDxgiFormat(config.input_format),
        }));
    constexpr size_t kBufferSize = 1024;
    constexpr size_t kStreamSize = 52;
    auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
    BitstreamBuffer bitstream_buffer(base::RandInt(0, 7 /*MaxDPBSize - 1*/),
                                     shared_memory.Duplicate(), kBufferSize);
    EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
        .WillOnce(Return(EncoderStatus::Codes::kOk));
    EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncodedBitstreamWrittenBytesCount)
        .WillRepeatedly(Return(kStreamSize));

    auto result = encoder_delegate_->Encode(
        input_frame.Get(), 0 /*input_frame_subresource*/,
        gfx::ColorSpace::CreateSRGB(), bitstream_buffer,
        VideoEncoder::EncodeOptions());
    EXPECT_EQ(result.has_value(), true);
    auto [bitstream_buffer_id, metadata] = std::move(result).value();
    EXPECT_EQ(bitstream_buffer_id, bitstream_buffer.id());
    // The first frame of the sequence is expected as a keyframe.
    EXPECT_EQ(metadata.key_frame, (i == 0));
    EXPECT_GT(metadata.payload_size_bytes, kStreamSize);
    EXPECT_LE(metadata.payload_size_bytes, kBufferSize);
    EXPECT_GT(metadata.qp, 0);
  }
}

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_AV1_DELEGATE_UNITTEST_H_
