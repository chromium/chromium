// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_h264_delegate.h"

#include "base/strings/stringprintf.h"
#include "media/base/win/d3d12_mocks.h"
#include "media/base/win/d3d12_video_mocks.h"
#include "media/gpu/windows/d3d12_video_encode_delegate_unittest.h"
#include "media/gpu/windows/mf_video_encoder_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace media {

class D3D12VideoEncodeH264DelegateTest
    : public D3D12VideoEncodeDelegateTestBase {
 protected:
  void SetUp() override {
    device_ = MakeComPtr<NiceMock<D3D12DeviceMock>>();
    video_device3_ = MakeComPtr<NiceMock<D3D12VideoDevice3Mock>>();
    ON_CALL(*video_device3_.Get(), QueryInterface(IID_ID3D12Device, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(device_.Get()));
    ON_CALL(*video_device3_.Get(), QueryInterface(IID_ID3D12VideoDevice1, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(video_device3_.Get()));
    ON_CALL(*video_device3_.Get(), CheckFeatureSupport)
        .WillByDefault([](D3D12_FEATURE_VIDEO feature, void*, UINT) {
          EXPECT_TRUE(false) << "Unexpected feature: " << feature;
          return E_INVALIDARG;
        });
    ON_CALL(*video_device3_.Get(),
            CheckFeatureSupport(D3D12_FEATURE_VIDEO_ENCODER_CODEC, _, _))
        .WillByDefault([](D3D12_FEATURE_VIDEO, void* data, UINT size) {
          EXPECT_EQ(size, sizeof(D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC));
          if (size != sizeof(D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC)) {
            return E_INVALIDARG;
          }
          auto* codec =
              static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC*>(data);
          EXPECT_EQ(codec->Codec, D3D12_VIDEO_ENCODER_CODEC_H264);
          codec->IsSupported = codec->Codec == D3D12_VIDEO_ENCODER_CODEC_H264;
          return S_OK;
        });
    ON_CALL(
        *video_device3_.Get(),
        CheckFeatureSupport(D3D12_FEATURE_VIDEO_ENCODER_PROFILE_LEVEL, _, _))
        .WillByDefault([](D3D12_FEATURE_VIDEO, void* data, UINT size) {
          EXPECT_EQ(size,
                    sizeof(D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL));
          if (size != sizeof(D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL)) {
            return E_INVALIDARG;
          }
          auto* profile_level =
              static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL*>(
                  data);
          EXPECT_EQ(profile_level->Codec, D3D12_VIDEO_ENCODER_CODEC_H264);
          EXPECT_EQ(profile_level->MinSupportedLevel.DataSize,
                    sizeof(D3D12_VIDEO_ENCODER_LEVELS_H264));
          EXPECT_EQ(profile_level->MaxSupportedLevel.DataSize,
                    sizeof(D3D12_VIDEO_ENCODER_LEVELS_H264));
          if (profile_level->MinSupportedLevel.DataSize !=
                  sizeof(D3D12_VIDEO_ENCODER_LEVELS_H264) ||
              profile_level->MaxSupportedLevel.DataSize !=
                  sizeof(D3D12_VIDEO_ENCODER_LEVELS_H264)) {
            return E_INVALIDARG;
          }
          profile_level->IsSupported =
              profile_level->Codec == D3D12_VIDEO_ENCODER_CODEC_H264;
          *profile_level->MinSupportedLevel.pH264LevelSetting =
              D3D12_VIDEO_ENCODER_LEVELS_H264_1;
          *profile_level->MaxSupportedLevel.pH264LevelSetting = kMaxLevel;
          return S_OK;
        });
    ON_CALL(*video_device3_.Get(),
            CheckFeatureSupport(
                D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT, _, _))
        .WillByDefault([](D3D12_FEATURE_VIDEO, void* data, UINT size) {
          EXPECT_EQ(
              size,
              sizeof(
                  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT));
          if (size !=
              sizeof(
                  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT)) {
            return E_INVALIDARG;
          }
          auto* config = static_cast<
              D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT*>(
              data);
          EXPECT_EQ(config->Codec, D3D12_VIDEO_ENCODER_CODEC_H264);
          config->IsSupported = config->Codec == D3D12_VIDEO_ENCODER_CODEC_H264;
          EXPECT_EQ(
              config->CodecSupportLimits.DataSize,
              sizeof(D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264));
          if (config->CodecSupportLimits.DataSize !=
              sizeof(D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264)) {
            return E_INVALIDARG;
          }
          config->CodecSupportLimits.pH264Support->SupportFlags =
              D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264_FLAG_NONE;
          config->CodecSupportLimits.pH264Support
              ->DisableDeblockingFilterSupportedModes =
              D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_SLICES_DEBLOCKING_MODE_FLAG_NONE;
          return S_OK;
        });
    ON_CALL(*video_device3_.Get(),
            CheckFeatureSupport(D3D12_FEATURE_VIDEO_ENCODER_SUPPORT, _, _))
        .WillByDefault([](D3D12_FEATURE_VIDEO, void* data, UINT size) {
          EXPECT_EQ(size, sizeof(D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT));
          if (size != sizeof(D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT)) {
            return E_INVALIDARG;
          }
          auto* support =
              static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT*>(data);
          if (support->SuggestedProfile.DataSize !=
                  sizeof(D3D12_VIDEO_ENCODER_PROFILE_H264) ||
              support->SuggestedLevel.DataSize !=
                  sizeof(D3D12_VIDEO_ENCODER_LEVELS_H264)) {
            return E_INVALIDARG;
          }
          EXPECT_EQ(support->Codec, D3D12_VIDEO_ENCODER_CODEC_H264);
          EXPECT_TRUE(support->InputFormat == DXGI_FORMAT_NV12 ||
                      support->InputFormat == DXGI_FORMAT_P010);
          support->SupportFlags =
              support->Codec == D3D12_VIDEO_ENCODER_CODEC_H264
                  ? D3D12_VIDEO_ENCODER_SUPPORT_FLAG_GENERAL_SUPPORT_OK
                  : D3D12_VIDEO_ENCODER_SUPPORT_FLAG_NONE;
          support->ValidationFlags =
              support->Codec != D3D12_VIDEO_ENCODER_CODEC_H264
                  ? D3D12_VIDEO_ENCODER_VALIDATION_FLAG_CODEC_NOT_SUPPORTED
                  : D3D12_VIDEO_ENCODER_VALIDATION_FLAG_NONE;
          *support->SuggestedProfile.pH264Profile =
              D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN;
          *support->SuggestedLevel.pH264LevelSetting = kMaxLevel;
          return S_OK;
        });

    encoder_delegate_ =
        std::make_unique<D3D12VideoEncodeH264Delegate>(video_device3_);
    encoder_delegate_->SetFactoriesForTesting(
        base::BindRepeating(&CreateVideoEncoderWrapper),
        base::BindRepeating(&CreateVideoProcessorWrapper));
  }

  static constexpr D3D12_VIDEO_ENCODER_LEVELS_H264 kMaxLevel =
      D3D12_VIDEO_ENCODER_LEVELS_H264_31;
  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
  Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> video_device3_;
};

TEST_F(D3D12VideoEncodeH264DelegateTest, UnsupportedCodec) {
  ON_CALL(*video_device3_.Get(),
          CheckFeatureSupport(D3D12_FEATURE_VIDEO_ENCODER_CODEC, _, _))
      .WillByDefault(Return(S_OK));  // Not setting |IsSupported| flag.
  EXPECT_EQ(encoder_delegate_->Initialize(GetDefaultH264Config()).code(),
            EncoderStatus::Codes::kEncoderUnsupportedCodec);
}

TEST_F(D3D12VideoEncodeH264DelegateTest, UnsupportedProfile) {
  VideoEncodeAccelerator::Config config = GetDefaultH264Config();
  config.output_profile = H264PROFILE_HIGH422PROFILE;
  EXPECT_EQ(encoder_delegate_->Initialize(config).code(),
            EncoderStatus::Codes::kEncoderUnsupportedProfile);
}

TEST_F(D3D12VideoEncodeH264DelegateTest, UnsupportedLevel) {
  VideoEncodeAccelerator::Config config = GetDefaultH264Config();
  config.h264_output_level = H264SPS::H264LevelIDC::kLevelIDC4p2;
  EXPECT_EQ(encoder_delegate_->Initialize(config).code(),
            EncoderStatus::Codes::kEncoderUnsupportedConfig);
}

TEST_F(D3D12VideoEncodeH264DelegateTest, EncodeFrame) {
  VideoEncodeAccelerator::Config config = GetDefaultH264Config();
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  auto input_frame =
      CreateResource(config.input_visible_size, config.input_format);
  constexpr size_t kBufferSize = 1024;
  constexpr size_t kStreamSize = 512;
  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
  BitstreamBuffer bitstream_buffer(0, shared_memory.Duplicate(), kBufferSize);
  EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncodedBitstreamWrittenBytesCount())
      .WillOnce(Return(kStreamSize));
  bool is_key_frame;
  EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
      .WillOnce([&](const D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS&
                        input_arguments,
                    const D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE&) {
        if (input_arguments.PictureControlDesc.PictureControlCodecData
                .DataSize !=
            sizeof(D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264)) {
          return EncoderStatus::Codes::kSystemAPICallError;
        }
        is_key_frame = input_arguments.PictureControlDesc
                           .PictureControlCodecData.pH264PicData->FrameType ==
                       D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_IDR_FRAME;
        return EncoderStatus::Codes::kOk;
      });
  auto result_or_error = encoder_delegate_->Encode(
      input_frame, 0, gfx::ColorSpace::CreateSRGB(), bitstream_buffer,
      VideoEncoder::EncodeOptions());
  ASSERT_TRUE(result_or_error.has_value());

  BitstreamBufferMetadata metadata =
      std::move(result_or_error).value().metadata_;
  EXPECT_EQ(metadata.key_frame, is_key_frame);
  if (encoder_delegate_->ReportsAverageQp()) {
    EXPECT_GE(metadata.qp, 0);
    EXPECT_LE(metadata.qp, kH26xMaxQp);
  } else {
    EXPECT_EQ(metadata.qp, -1);
  }

  // Make sure we have written h264 SPS/PPS headers.
  ASSERT_GT(metadata.payload_size_bytes, kStreamSize);
  ASSERT_LE(metadata.payload_size_bytes, kBufferSize);
  H264Parser parser;
  base::WritableSharedMemoryMapping map = shared_memory.Map();
  parser.SetStream(map.data(), map.size());
  H264NALU nalu;
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H264Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H264NALU::kSPS);
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H264Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H264NALU::kPPS);
}

TEST_F(D3D12VideoEncodeH264DelegateTest, EncodeFramesAndVerifyKeyFrameFlag) {
  VideoEncodeAccelerator::Config config = GetDefaultH264Config();
  config.gop_length = 5;
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  auto input_frame =
      CreateResource(config.input_visible_size, config.input_format);
  constexpr size_t kBufferSize = 1024;
  constexpr size_t kStreamSize = 512;
  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
  BitstreamBuffer bitstream_buffer(0, shared_memory.Duplicate(), kBufferSize);
  for (uint32_t i = 0; i < config.gop_length.value() * 2; i++) {
    SCOPED_TRACE(base::StringPrintf("Frame #%u", i));
    bool should_be_key_frame = i % config.gop_length.value() == 0;
    EXPECT_CALL(*GetVideoEncoderWrapper(),
                GetEncodedBitstreamWrittenBytesCount())
        .WillOnce(Return(kStreamSize));
    EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
        .WillOnce([&](const D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS&
                          input_arguments,
                      const D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE&) {
          if (input_arguments.PictureControlDesc.PictureControlCodecData
                  .DataSize !=
              sizeof(D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264)) {
            return EncoderStatus::Codes::kSystemAPICallError;
          }
          EXPECT_EQ(input_arguments.PictureControlDesc.PictureControlCodecData
                            .pH264PicData->FrameType ==
                        D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_IDR_FRAME,
                    should_be_key_frame);
          return EncoderStatus::Codes::kOk;
        });
    auto result_or_error = encoder_delegate_->Encode(
        input_frame, 0, gfx::ColorSpace::CreateSRGB(), bitstream_buffer,
        VideoEncoder::EncodeOptions());
    ASSERT_TRUE(result_or_error.has_value());
    Mock::VerifyAndClearExpectations(GetVideoEncoderWrapper());
  }
}

}  // namespace media
