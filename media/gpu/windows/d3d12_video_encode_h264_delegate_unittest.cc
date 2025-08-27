// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_h264_delegate.h"

#include <array>

#include "base/strings/stringprintf.h"
#include "media/base/media_switches.h"
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

class D3D12VideoEncodeH264ReferenceFrameManagerTest : public ::testing::Test {
 protected:
  void SetUp() override { device_ = MakeComPtr<NiceMock<D3D12DeviceMock>>(); }

  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
};

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
    ON_CALL(
        *video_device3_.Get(),
        CheckFeatureSupport(
            D3D12_FEATURE_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT, _, _))
        .WillByDefault([](D3D12_FEATURE_VIDEO, void* data, UINT size) {
          EXPECT_EQ(
              size,
              sizeof(
                  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT));
          if (size !=
              sizeof(
                  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT)) {
            return E_INVALIDARG;
          }
          auto* picture_control = static_cast<
              D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT*>(
              data);
          EXPECT_EQ(picture_control->Codec, D3D12_VIDEO_ENCODER_CODEC_H264);
          picture_control->IsSupported =
              picture_control->Codec == D3D12_VIDEO_ENCODER_CODEC_H264;
          EXPECT_EQ(
              picture_control->PictureSupport.DataSize,
              sizeof(D3D12_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT_H264));
          if (picture_control->PictureSupport.DataSize !=
              sizeof(D3D12_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT_H264)) {
            return E_INVALIDARG;
          }
          picture_control->PictureSupport.pH264Support->MaxLongTermReferences =
              1;
          picture_control->PictureSupport.pH264Support->MaxDPBCapacity = 16;
          return S_OK;
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
                  ? D3D12_VIDEO_ENCODER_SUPPORT_FLAG_GENERAL_SUPPORT_OK |
                        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RECONSTRUCTED_FRAMES_REQUIRE_TEXTURE_ARRAYS
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
        std::make_unique<D3D12VideoEncodeH264Delegate>(video_device3_, true);
    encoder_delegate_->SetFactoriesForTesting(
        base::BindRepeating(&CreateVideoEncoderWrapper),
        base::BindRepeating(&CreateVideoProcessorWrapper));
  }

  static constexpr D3D12_VIDEO_ENCODER_LEVELS_H264 kMaxLevel =
      D3D12_VIDEO_ENCODER_LEVELS_H264_31;
  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
  Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> video_device3_;
};

TEST_F(D3D12VideoEncodeH264ReferenceFrameManagerTest,
       ProcessMemoryManagementControlOperation) {
  // Initialization
  D3D12VideoEncodeH264ReferenceFrameManager reference_manager;
  ASSERT_TRUE(reference_manager.InitializeTextureResources(
      device_.Get(), {1280, 720}, DXGI_FORMAT_NV12, 4, true));
  EXPECT_EQ(reference_manager.GetMaxLongTermFrameIndexPlus1(), 0u);
  EXPECT_EQ(reference_manager.GetLongTermReferenceFrameResourceId(0),
            std::nullopt);
  EXPECT_EQ(reference_manager.ToReferencePictureDescriptors().size(), 0u);

  // IDR frame #0 with adaptive_ref_pic_marking_mode_flag = true
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 pic_params0{
      .FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_IDR_FRAME,
      .PictureOrderCountNumber = 0,
      .FrameDecodingOrderNumber = 0,
      .adaptive_ref_pic_marking_mode_flag = true,
  };
  reference_manager.ProcessMemoryManagementControlOperation(pic_params0);
  EXPECT_EQ(reference_manager.GetMaxLongTermFrameIndexPlus1(), 1u);
  EXPECT_NE(reference_manager.GetLongTermReferenceFrameResourceId(0),
            std::nullopt);
  base::span<D3D12_VIDEO_ENCODER_REFERENCE_PICTURE_DESCRIPTOR_H264>
      descriptors = reference_manager.ToReferencePictureDescriptors();
  ASSERT_EQ(descriptors.size(), 1u);
  EXPECT_TRUE(descriptors[0].IsLongTermReference);
  EXPECT_EQ(descriptors[0].LongTermPictureIdx, 0u);
  EXPECT_EQ(descriptors[0].ReconstructedPictureResourceIndex, 0u);
  EXPECT_EQ(descriptors[0].PictureOrderCountNumber,
            pic_params0.PictureOrderCountNumber);
  EXPECT_EQ(descriptors[0].FrameDecodingOrderNumber,
            pic_params0.FrameDecodingOrderNumber);

  // P frame #1 with mmco 4 and 6
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264_REFERENCE_PICTURE_MARKING_OPERATION
  mmco1[] = {
      {.memory_management_control_operation = 4,
       .max_long_term_frame_idx_plus1 = 2},
      {.memory_management_control_operation = 6, .long_term_frame_idx = 1},
      {.memory_management_control_operation = 0},
  };
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 pic_params1{
      .FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_P_FRAME,
      .PictureOrderCountNumber = 2,
      .FrameDecodingOrderNumber = 1,
      .adaptive_ref_pic_marking_mode_flag = true,
      .RefPicMarkingOperationsCommandsCount = std::size(mmco1),
      .pRefPicMarkingOperationsCommands = mmco1,
  };
  reference_manager.ProcessMemoryManagementControlOperation(pic_params1);
  EXPECT_EQ(reference_manager.GetMaxLongTermFrameIndexPlus1(), 2u);
  EXPECT_NE(reference_manager.GetLongTermReferenceFrameResourceId(1),
            std::nullopt);
  descriptors = reference_manager.ToReferencePictureDescriptors();
  ASSERT_EQ(descriptors.size(), 2u);
  EXPECT_TRUE(descriptors[1].IsLongTermReference);
  EXPECT_EQ(descriptors[1].LongTermPictureIdx, 1u);
  EXPECT_EQ(descriptors[1].ReconstructedPictureResourceIndex, 1u);
  EXPECT_EQ(descriptors[1].PictureOrderCountNumber,
            pic_params1.PictureOrderCountNumber);
  EXPECT_EQ(descriptors[1].FrameDecodingOrderNumber,
            pic_params1.FrameDecodingOrderNumber);

  // P frame #2 with mmco 2
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264_REFERENCE_PICTURE_MARKING_OPERATION
  mmco2[] = {
      {.memory_management_control_operation = 2, .long_term_pic_num = 0},
      {.memory_management_control_operation = 0},
  };
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 pic_params2{
      .FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_P_FRAME,
      .PictureOrderCountNumber = 4,
      .FrameDecodingOrderNumber = 2,
      .adaptive_ref_pic_marking_mode_flag = true,
      .RefPicMarkingOperationsCommandsCount = std::size(mmco2),
      .pRefPicMarkingOperationsCommands = mmco2,
  };
  reference_manager.ProcessMemoryManagementControlOperation(pic_params2);
  EXPECT_EQ(reference_manager.GetMaxLongTermFrameIndexPlus1(), 2u);
  EXPECT_EQ(reference_manager.GetLongTermReferenceFrameResourceId(0),
            std::nullopt);
  descriptors = reference_manager.ToReferencePictureDescriptors();
  ASSERT_EQ(descriptors.size(), 1u);
  EXPECT_EQ(descriptors[0].LongTermPictureIdx, 1u);
  EXPECT_EQ(descriptors[0].ReconstructedPictureResourceIndex, 0u);
  EXPECT_EQ(descriptors[0].PictureOrderCountNumber,
            pic_params1.PictureOrderCountNumber);
  EXPECT_EQ(descriptors[0].FrameDecodingOrderNumber,
            pic_params1.FrameDecodingOrderNumber);

  // P frame #3 with mmco 5
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264_REFERENCE_PICTURE_MARKING_OPERATION
  mmco3[] = {
      {.memory_management_control_operation = 5},
      {.memory_management_control_operation = 0},
  };
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 pic_params3{
      .FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_P_FRAME,
      .PictureOrderCountNumber = 6,
      .FrameDecodingOrderNumber = 3,
      .adaptive_ref_pic_marking_mode_flag = true,
      .RefPicMarkingOperationsCommandsCount = std::size(mmco3),
      .pRefPicMarkingOperationsCommands = mmco3,
  };
  reference_manager.ProcessMemoryManagementControlOperation(pic_params3);
  EXPECT_EQ(reference_manager.GetMaxLongTermFrameIndexPlus1(), 0u);
  EXPECT_EQ(reference_manager.GetLongTermReferenceFrameResourceId(1),
            std::nullopt);
  descriptors = reference_manager.ToReferencePictureDescriptors();
  EXPECT_EQ(descriptors.size(), 0u);
}

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
  EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata())
      .WillOnce(Return(GetEncoderOutputMetadataResourceMap(kStreamSize)));
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
      std::move(result_or_error).value().metadata;
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
    EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata())
        .WillOnce(Return(GetEncoderOutputMetadataResourceMap(kStreamSize)));
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

TEST_F(D3D12VideoEncodeH264DelegateTest,
       EncodeAtL1T3AndVerifyMaxNumOfRefFrames) {
  EnableFeature(kD3D12VideoEncodeAcceleratorL1T3);

  VideoEncodeAccelerator::Config config = GetDefaultH264Config();
  config.spatial_layers = {{
      .width = config.input_visible_size.width(),
      .height = config.input_visible_size.height(),
      .bitrate_bps = config.bitrate.target_bps(),
      .framerate = config.framerate,
      .num_of_temporal_layers = 3,
  }};

  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());
  // H.264 requires one extra DPB slot to be allocated for frame_num gap
  // handling; at the same time for the test, we disable non-reference
  // frames, so another DPB slot is reserved for storing the frame that
  // is not referenced.
  EXPECT_EQ(encoder_delegate_->GetMaxNumOfRefFrames(), 4u);

  constexpr uint32_t kFramesToEncode = 10;
  constexpr size_t kStreamSize = 512;
  constexpr size_t kBufferSize = 1024;
  auto input_frame =
      CreateResource(config.input_visible_size, config.input_format);
  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
  BitstreamBuffer bitstream_buffer(0, shared_memory.Duplicate(), kBufferSize);
  for (uint32_t i = 0; i < kFramesToEncode; i++) {
    static const std::array<uint32_t, 4> layer_pattern = {0, 2, 1, 2};
    uint32_t temporal_idx = layer_pattern[i % layer_pattern.size()];
    EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata())
        .WillOnce(Return(GetEncoderOutputMetadataResourceMap(kStreamSize)));
    auto result_or_error = encoder_delegate_->Encode(
        input_frame, 0, gfx::ColorSpace::CreateSRGB(), bitstream_buffer,
        VideoEncoder::EncodeOptions());
    ASSERT_TRUE(result_or_error.has_value());

    BitstreamBufferMetadata metadata =
        std::move(result_or_error).value().metadata;
    ASSERT_TRUE(metadata.h264.has_value());
    ASSERT_EQ(metadata.h264->temporal_idx, temporal_idx);
  }
}

TEST_F(D3D12VideoEncodeH264DelegateTest, EncodeWithManualReferenceControl) {
  VideoEncodeAccelerator::Config config = GetDefaultH264Config();
  config.manual_reference_buffer_control = true;
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  auto input_frame =
      CreateResource(config.input_visible_size, config.input_format);
  constexpr size_t kBufferSize = 1024;
  constexpr size_t kStreamSize = 512;
  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
  BitstreamBuffer bitstream_buffer(0, shared_memory.Duplicate(), kBufferSize);

  EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata)
      .WillRepeatedly(
          [&] { return GetEncoderOutputMetadataResourceMap(kStreamSize); });

  // Pass reference_buffers and update_buffer in EncodeOptions for emulation of
  // L1T2 encoding of 3 frames.
  VideoEncoder::EncodeOptions encode_opts;
  encode_opts.reference_buffers = {};
  encode_opts.update_buffer = 0;
  auto result_or_error =
      encoder_delegate_->Encode(input_frame, 0, gfx::ColorSpace::CreateSRGB(),
                                bitstream_buffer, encode_opts);
  ASSERT_TRUE(result_or_error.has_value());

  encode_opts.reference_buffers = {0};
  encode_opts.update_buffer = std::nullopt;
  result_or_error =
      encoder_delegate_->Encode(input_frame, 0, gfx::ColorSpace::CreateSRGB(),
                                bitstream_buffer, encode_opts);
  ASSERT_TRUE(result_or_error.has_value());

  encode_opts.reference_buffers = {0};
  encode_opts.update_buffer = 0;
  result_or_error =
      encoder_delegate_->Encode(input_frame, 0, gfx::ColorSpace::CreateSRGB(),
                                bitstream_buffer, encode_opts);
  ASSERT_TRUE(result_or_error.has_value());
}

}  // namespace media
