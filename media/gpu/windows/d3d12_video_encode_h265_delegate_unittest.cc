// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_h265_delegate.h"

#include "base/strings/stringprintf.h"
#include "media/base/win/d3d12_mocks.h"
#include "media/base/win/d3d12_video_mocks.h"
#include "media/gpu/windows/d3d12_video_encode_delegate_unittest.h"
#include "media/gpu/windows/format_utils.h"
#include "media/gpu/windows/mf_video_encoder_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace media {

class D3D12VideoEncodeH265ReferenceFrameManagerTest : public ::testing::Test {
 protected:
  void SetUp() override { device_ = MakeComPtr<NiceMock<D3D12DeviceMock>>(); }

  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
};

class D3D12VideoEncodeH265DelegateTest
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
          picture_control->Codec = D3D12_VIDEO_ENCODER_CODEC_HEVC;
          picture_control->IsSupported =
              picture_control->Codec == D3D12_VIDEO_ENCODER_CODEC_HEVC;
          EXPECT_EQ(
              picture_control->PictureSupport.DataSize,
              sizeof(D3D12_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT_HEVC));
          if (picture_control->PictureSupport.DataSize !=
              sizeof(D3D12_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT_HEVC)) {
            return E_INVALIDARG;
          }
          picture_control->PictureSupport.pHEVCSupport->MaxLongTermReferences =
              1;
          picture_control->PictureSupport.pHEVCSupport->MaxL0ReferencesForP = 3;
          picture_control->PictureSupport.pHEVCSupport->MaxDPBCapacity = 16;
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
          EXPECT_EQ(codec->Codec, D3D12_VIDEO_ENCODER_CODEC_HEVC);
          codec->IsSupported = codec->Codec == D3D12_VIDEO_ENCODER_CODEC_HEVC;
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
          EXPECT_EQ(profile_level->Codec, D3D12_VIDEO_ENCODER_CODEC_HEVC);
          EXPECT_EQ(profile_level->MinSupportedLevel.DataSize,
                    sizeof(D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC));
          EXPECT_EQ(profile_level->MaxSupportedLevel.DataSize,
                    sizeof(D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC));
          if (profile_level->MinSupportedLevel.DataSize !=
                  sizeof(D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC) ||
              profile_level->MaxSupportedLevel.DataSize !=
                  sizeof(D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC)) {
            return E_INVALIDARG;
          }
          profile_level->IsSupported =
              profile_level->Codec == D3D12_VIDEO_ENCODER_CODEC_HEVC;
          *profile_level->MinSupportedLevel.pHEVCLevelSetting = {
              D3D12_VIDEO_ENCODER_LEVELS_HEVC_1,
              D3D12_VIDEO_ENCODER_TIER_HEVC_MAIN};
          *profile_level->MaxSupportedLevel.pHEVCLevelSetting = {
              kMaxLevel, D3D12_VIDEO_ENCODER_TIER_HEVC_MAIN};
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
          auto* codec_config = static_cast<
              D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT*>(
              data);
          EXPECT_EQ(codec_config->Codec, D3D12_VIDEO_ENCODER_CODEC_HEVC);
          EXPECT_EQ(codec_config->Profile.DataSize,
                    sizeof(D3D12_VIDEO_ENCODER_PROFILE_HEVC));
          if (codec_config->Profile.DataSize !=
              sizeof(D3D12_VIDEO_ENCODER_PROFILE_HEVC)) {
            return E_INVALIDARG;
          }
          EXPECT_EQ(*codec_config->Profile.pHEVCProfile,
                    D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN);
          codec_config->IsSupported =
              codec_config->Codec == D3D12_VIDEO_ENCODER_CODEC_HEVC &&
              *codec_config->Profile.pHEVCProfile ==
                  D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN;
          EXPECT_EQ(
              codec_config->CodecSupportLimits.DataSize,
              sizeof(D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC));
          if (codec_config->CodecSupportLimits.DataSize !=
              sizeof(D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC)) {
            return E_INVALIDARG;
          }
          *codec_config->CodecSupportLimits.pHEVCSupport = {
              .SupportFlags =
                  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_NONE,
              .MinLumaCodingUnitSize =
                  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_8x8,
              .MaxLumaCodingUnitSize =
                  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_64x64,
              .MinLumaTransformUnitSize =
                  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_4x4,
              .MaxLumaTransformUnitSize =
                  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_32x32,
              .max_transform_hierarchy_depth_inter = 0,
              .max_transform_hierarchy_depth_intra = 0,
          };
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
                  sizeof(D3D12_VIDEO_ENCODER_PROFILE_HEVC) ||
              support->SuggestedLevel.DataSize !=
                  sizeof(D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC)) {
            return E_INVALIDARG;
          }
          EXPECT_EQ(support->Codec, D3D12_VIDEO_ENCODER_CODEC_HEVC);
          EXPECT_TRUE(support->InputFormat == DXGI_FORMAT_NV12 ||
                      support->InputFormat == DXGI_FORMAT_P010);
          support->SupportFlags =
              support->Codec == D3D12_VIDEO_ENCODER_CODEC_HEVC
                  ? D3D12_VIDEO_ENCODER_SUPPORT_FLAG_GENERAL_SUPPORT_OK
                  : D3D12_VIDEO_ENCODER_SUPPORT_FLAG_NONE;
          support->ValidationFlags =
              support->Codec != D3D12_VIDEO_ENCODER_CODEC_HEVC
                  ? D3D12_VIDEO_ENCODER_VALIDATION_FLAG_CODEC_NOT_SUPPORTED
                  : D3D12_VIDEO_ENCODER_VALIDATION_FLAG_NONE;
          *support->SuggestedProfile.pHEVCProfile =
              D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN;
          *support->SuggestedLevel.pHEVCLevelSetting = {
              kMaxLevel, D3D12_VIDEO_ENCODER_TIER_HEVC_MAIN};
          EXPECT_EQ(support->ResolutionsListCount, 1u);
          for (auto& limit :
               // SAFETY: callers should guarantee |pResolutionDependentSupport|
               // has at least |ResolutionsListCount| elements.
               UNSAFE_BUFFERS(base::span(support->pResolutionDependentSupport,
                                         support->ResolutionsListCount))) {
            limit.SubregionBlockPixelsSize = 16;
          }
          return S_OK;
        });

    gpu::GpuDriverBugWorkarounds gpu_workarounds{};
    encoder_delegate_ = std::make_unique<D3D12VideoEncodeH265Delegate>(
        video_device3_, gpu_workarounds);
    encoder_delegate_->SetFactoriesForTesting(
        base::BindRepeating(&CreateVideoEncoderWrapper),
        base::BindRepeating(&CreateVideoProcessorWrapper));
  }

  VideoEncodeAccelerator::Config GetDefaultH265Config() const {
    VideoEncodeAccelerator::Config config = GetDefaultH264Config();
    config.output_profile = HEVCPROFILE_MAIN;
    return config;
  }

  static constexpr D3D12_VIDEO_ENCODER_LEVELS_HEVC kMaxLevel =
      D3D12_VIDEO_ENCODER_LEVELS_HEVC_31;
  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
  Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> video_device3_;
};

TEST_F(D3D12VideoEncodeH265ReferenceFrameManagerTest,
       MarkReferenceFrameAndCheckDescriptors) {
  D3D12VideoEncodeH265ReferenceFrameManager reference_manager;
  ASSERT_TRUE(reference_manager.InitializeTextureResources(
      device_.Get(), {1280, 720}, DXGI_FORMAT_NV12, 4));
  EXPECT_EQ(reference_manager.GetReferenceFrameId(0), std::nullopt);

  std::vector<uint32_t> list0_reference_frames;
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_HEVC pic_params{};
  reference_manager.WriteReferencePictureDescriptorsToPictureParameters(
      &pic_params, list0_reference_frames);
  EXPECT_EQ(pic_params.ReferenceFramesReconPictureDescriptorsCount, 0u);

  // Mark frame #0 as short-term reference #0.
  reference_manager.MarkCurrentFrameReferenced(0, 0, false);
  EXPECT_EQ(reference_manager.GetReferenceFrameId(0), 0u);
  list0_reference_frames = {0};
  pic_params.List0ReferenceFramesCount = list0_reference_frames.size();
  pic_params.pList0ReferenceFrames = list0_reference_frames.data();
  reference_manager.WriteReferencePictureDescriptorsToPictureParameters(
      &pic_params, list0_reference_frames);
  ASSERT_EQ(pic_params.ReferenceFramesReconPictureDescriptorsCount, 1u);
  // SAFETY: |pReferenceFramesReconPictureDescriptors| is guaranteed to have
  // |ReferenceFramesReconPictureDescriptorsCount| elements.
  base::span<const D3D12_VIDEO_ENCODER_REFERENCE_PICTURE_DESCRIPTOR_HEVC>
      descriptors = UNSAFE_BUFFERS(
          base::span(pic_params.pReferenceFramesReconPictureDescriptors,
                     pic_params.ReferenceFramesReconPictureDescriptorsCount));
  EXPECT_EQ(descriptors[0].IsRefUsedByCurrentPic, true);
  EXPECT_EQ(descriptors[0].IsLongTermReference, false);
  EXPECT_EQ(descriptors[0].PictureOrderCountNumber, 0u);

  // Mark frame #1 as long-term reference #2.
  reference_manager.MarkCurrentFrameReferenced(1, 2, true);
  EXPECT_EQ(reference_manager.GetReferenceFrameId(2), 1u);
  list0_reference_frames = {1};
  pic_params.List0ReferenceFramesCount = list0_reference_frames.size();
  pic_params.pList0ReferenceFrames = list0_reference_frames.data();
  reference_manager.WriteReferencePictureDescriptorsToPictureParameters(
      &pic_params, list0_reference_frames);
  ASSERT_EQ(pic_params.ReferenceFramesReconPictureDescriptorsCount, 2u);
  // SAFETY: |pReferenceFramesReconPictureDescriptors| is guaranteed to have
  // |ReferenceFramesReconPictureDescriptorsCount| elements.
  descriptors = UNSAFE_BUFFERS(
      base::span(pic_params.pReferenceFramesReconPictureDescriptors,
                 pic_params.ReferenceFramesReconPictureDescriptorsCount));
  EXPECT_EQ(descriptors[0].IsRefUsedByCurrentPic, false);
  EXPECT_EQ(descriptors[1].IsRefUsedByCurrentPic, true);
  EXPECT_EQ(descriptors[1].IsLongTermReference, true);
  EXPECT_EQ(descriptors[1].PictureOrderCountNumber, 1u);

  // Mark frame #0 as not referenced.
  reference_manager.MarkFrameUnreferenced(0);
  EXPECT_EQ(reference_manager.GetReferenceFrameId(0), std::nullopt);
  EXPECT_EQ(reference_manager.GetReferenceFrameId(2), 0u);
  list0_reference_frames = {0};
  pic_params.List0ReferenceFramesCount = list0_reference_frames.size();
  pic_params.pList0ReferenceFrames = list0_reference_frames.data();
  reference_manager.WriteReferencePictureDescriptorsToPictureParameters(
      &pic_params, list0_reference_frames);
  ASSERT_EQ(pic_params.ReferenceFramesReconPictureDescriptorsCount, 1u);
  // SAFETY: |pReferenceFramesReconPictureDescriptors| is guaranteed to have
  // |ReferenceFramesReconPictureDescriptorsCount| elements.
  descriptors = UNSAFE_BUFFERS(
      base::span(pic_params.pReferenceFramesReconPictureDescriptors,
                 pic_params.ReferenceFramesReconPictureDescriptorsCount));
  EXPECT_EQ(descriptors[0].IsRefUsedByCurrentPic, true);
  EXPECT_EQ(descriptors[0].IsLongTermReference, true);
  EXPECT_EQ(descriptors[0].PictureOrderCountNumber, 1u);
}

TEST_F(D3D12VideoEncodeH265DelegateTest, UnsupportedCodec) {
  ON_CALL(*video_device3_.Get(),
          CheckFeatureSupport(D3D12_FEATURE_VIDEO_ENCODER_CODEC, _, _))
      .WillByDefault(Return(S_OK));  // Not setting |IsSupported| flag.
  EXPECT_EQ(encoder_delegate_->Initialize(GetDefaultH265Config()).code(),
            EncoderStatus::Codes::kEncoderUnsupportedCodec);
}

TEST_F(D3D12VideoEncodeH265DelegateTest, UnsupportedProfile) {
  VideoEncodeAccelerator::Config config = GetDefaultH265Config();
  config.output_profile = HEVCPROFILE_REXT;
  EXPECT_EQ(encoder_delegate_->Initialize(config).code(),
            EncoderStatus::Codes::kEncoderUnsupportedProfile);
}

TEST_F(D3D12VideoEncodeH265DelegateTest, EncodeFrame) {
  VideoEncodeAccelerator::Config config = GetDefaultH265Config();
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
            sizeof(D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_HEVC)) {
          return EncoderStatus::Codes::kSystemAPICallError;
        }
        is_key_frame = input_arguments.PictureControlDesc
                           .PictureControlCodecData.pHEVCPicData->FrameType ==
                       D3D12_VIDEO_ENCODER_FRAME_TYPE_HEVC_IDR_FRAME;
        return EncoderStatus::Codes::kOk;
      });
  EXPECT_CALL(*GetVideoEncoderWrapper(), ReadbackBitstream)
      .WillOnce([&](base::span<uint8_t> bitstream_buffer) {
        constexpr base::span kStartCode = base::span_from_cstring("\0\0\1");
        EXPECT_GE(bitstream_buffer.size(), kStartCode.size());
        std::ranges::copy(kStartCode, bitstream_buffer.begin());
        return EncoderStatus::Codes::kOk;
      });
  auto result_or_error = encoder_delegate_->Encode(
      input_frame, gfx::ColorSpace::CreateSRGB(), bitstream_buffer,
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

  // Make sure we have written HEVC SPS/PPS headers.
  ASSERT_GT(metadata.payload_size_bytes, kStreamSize);
  ASSERT_LE(metadata.payload_size_bytes, kBufferSize);
  H265Parser parser;
  base::WritableSharedMemoryMapping map = shared_memory.Map();
  parser.SetStream(map.GetMemoryAsSpan<uint8_t>());
  H265NALU nalu;
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::VPS_NUT);
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::SPS_NUT);
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::PPS_NUT);
}

TEST_F(D3D12VideoEncodeH265DelegateTest, EncodeMain10HDRFrame) {
  // Accept the Main10 profile in the codec configuration support query, since
  // the default fixture mock only accepts Main.
  ON_CALL(*video_device3_.Get(),
          CheckFeatureSupport(
              D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT, _, _))
      .WillByDefault([](D3D12_FEATURE_VIDEO, void* data, UINT size) {
        auto* codec_config = static_cast<
            D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT*>(
            data);
        EXPECT_EQ(*codec_config->Profile.pHEVCProfile,
                  D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN10);
        codec_config->IsSupported =
            codec_config->Codec == D3D12_VIDEO_ENCODER_CODEC_HEVC &&
            *codec_config->Profile.pHEVCProfile ==
                D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN10;
        *codec_config->CodecSupportLimits.pHEVCSupport = {
            .SupportFlags =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_NONE,
            .MinLumaCodingUnitSize =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_8x8,
            .MaxLumaCodingUnitSize =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_64x64,
            .MinLumaTransformUnitSize =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_4x4,
            .MaxLumaTransformUnitSize =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_32x32,
            .max_transform_hierarchy_depth_inter = 0,
            .max_transform_hierarchy_depth_intra = 0,
        };
        return S_OK;
      });

  VideoEncodeAccelerator::Config config = GetDefaultH265Config();
  config.output_profile = HEVCPROFILE_MAIN10;
  config.input_format = PIXEL_FORMAT_P010LE;
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  auto input_frame =
      CreateResource(config.input_visible_size, config.input_format);
  constexpr size_t kBufferSize = 1024;
  constexpr size_t kStreamSize = 512;
  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
  BitstreamBuffer bitstream_buffer(0, shared_memory.Duplicate(), kBufferSize);
  EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata())
      .WillOnce(Return(GetEncoderOutputMetadataResourceMap(kStreamSize)));
  EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
      .WillOnce(Return(EncoderStatus::Codes::kOk));
  EXPECT_CALL(*GetVideoEncoderWrapper(), ReadbackBitstream)
      .WillOnce([&](base::span<uint8_t> bitstream_buffer) {
        constexpr base::span kStartCode = base::span_from_cstring("\0\0\1");
        EXPECT_GE(bitstream_buffer.size(), kStartCode.size());
        std::ranges::copy(kStartCode, bitstream_buffer.begin());
        return EncoderStatus::Codes::kOk;
      });

  // BT.2020 PQ HDR10 input with mastering display and content light level.
  gfx::ColorSpace hdr_color_space(
      gfx::ColorSpace::PrimaryID::BT2020, gfx::ColorSpace::TransferID::PQ,
      gfx::ColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
  gfx::HDRMetadata hdr_metadata;
  skhdr::MasteringDisplayColorVolume mdcv;
  mdcv.fDisplayPrimaries = {0.680f, 0.320f, 0.265f,  0.690f,
                            0.150f, 0.060f, 0.3127f, 0.3290f};
  mdcv.fMaximumDisplayMasteringLuminance = 1000.0f;
  mdcv.fMinimumDisplayMasteringLuminance = 0.05f;
  hdr_metadata.SetMDCV(mdcv);
  hdr_metadata.SetCLLI(
      skhdr::ContentLightLevelInformation::MakeUint16(/*maxCLL=*/1000,
                                                      /*maxFALL=*/400));

  auto result_or_error =
      encoder_delegate_->Encode(input_frame, hdr_color_space, bitstream_buffer,
                                VideoEncoder::EncodeOptions(), hdr_metadata);
  ASSERT_TRUE(result_or_error.has_value());

  BitstreamBufferMetadata metadata =
      std::move(result_or_error).value().metadata;
  ASSERT_GT(metadata.payload_size_bytes, kStreamSize);
  ASSERT_LE(metadata.payload_size_bytes, kBufferSize);

  H265Parser parser;
  base::WritableSharedMemoryMapping map = shared_memory.Map();
  parser.SetStream(map.GetMemoryAsSpan<uint8_t>());

  // VPS.
  H265NALU nalu;
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::VPS_NUT);
  int vps_id;
  ASSERT_EQ(parser.ParseVPS(&vps_id), H265Parser::Result::kOk);

  // SPS: verify Main10 bit depth and HDR VUI colour signalling.
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::SPS_NUT);
  int sps_id;
  ASSERT_EQ(parser.ParseSPS(&sps_id), H265Parser::Result::kOk);
  const H265SPS* sps = parser.GetSPS(sps_id);
  ASSERT_TRUE(sps);
  EXPECT_EQ(sps->bit_depth_luma_minus8, 2);
  EXPECT_EQ(sps->bit_depth_chroma_minus8, 2);
  EXPECT_TRUE(sps->vui_parameters_present_flag);
  EXPECT_TRUE(sps->vui_parameters.colour_description_present_flag);
  EXPECT_EQ(sps->vui_parameters.colour_primaries, 9);           // BT.2020
  EXPECT_EQ(sps->vui_parameters.transfer_characteristics, 16);  // PQ
  EXPECT_EQ(sps->vui_parameters.matrix_coeffs, 9);              // BT.2020-NCL

  // PPS.
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::PPS_NUT);
  int pps_id;
  ASSERT_EQ(parser.ParsePPS(nalu, &pps_id), H265Parser::Result::kOk);

  // Prefix SEI carrying the HDR static metadata.
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::PREFIX_SEI_NUT);
  H265SEI sei;
  ASSERT_EQ(parser.ParseSEI(&sei), H265Parser::Result::kOk);
  bool found_mdcv = false;
  bool found_clli = false;
  for (const auto& sei_msg : sei.msgs) {
    if (const auto* info = std::get_if<H26xSEIMasteringDisplayInfo>(&sei_msg)) {
      found_mdcv = true;
      EXPECT_EQ(info->display_primaries[0][0], 13250u);  // G x
      EXPECT_EQ(info->display_primaries[0][1], 34500u);  // G y
      EXPECT_EQ(info->display_primaries[1][0], 7500u);   // B x
      EXPECT_EQ(info->display_primaries[1][1], 3000u);   // B y
      EXPECT_EQ(info->display_primaries[2][0], 34000u);  // R x
      EXPECT_EQ(info->display_primaries[2][1], 16000u);  // R y
      EXPECT_EQ(info->white_points[0], 15635u);
      EXPECT_EQ(info->white_points[1], 16450u);
      EXPECT_EQ(info->max_luminance, 10000000u);
      EXPECT_EQ(info->min_luminance, 500u);
    } else if (const auto* clli_info =
                   std::get_if<H26xSEIContentLightLevelInfo>(&sei_msg)) {
      found_clli = true;
      EXPECT_EQ(clli_info->max_content_light_level, 1000u);
      EXPECT_EQ(clli_info->max_picture_average_light_level, 400u);
    }
  }
  EXPECT_TRUE(found_mdcv);
  EXPECT_TRUE(found_clli);
}

TEST_F(D3D12VideoEncodeH265DelegateTest, EncodeMain10HDRFrameFromRGBInput) {
  // Accept the Main10 profile in the codec configuration support query, since
  // the default fixture mock only accepts Main.
  ON_CALL(*video_device3_.Get(),
          CheckFeatureSupport(
              D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT, _, _))
      .WillByDefault([](D3D12_FEATURE_VIDEO, void* data, UINT size) {
        auto* codec_config = static_cast<
            D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT*>(
            data);
        EXPECT_EQ(*codec_config->Profile.pHEVCProfile,
                  D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN10);
        codec_config->IsSupported =
            codec_config->Codec == D3D12_VIDEO_ENCODER_CODEC_HEVC &&
            *codec_config->Profile.pHEVCProfile ==
                D3D12_VIDEO_ENCODER_PROFILE_HEVC_MAIN10;
        *codec_config->CodecSupportLimits.pHEVCSupport = {
            .SupportFlags =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_HEVC_FLAG_NONE,
            .MinLumaCodingUnitSize =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_8x8,
            .MaxLumaCodingUnitSize =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_64x64,
            .MinLumaTransformUnitSize =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_4x4,
            .MaxLumaTransformUnitSize =
                D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_32x32,
            .max_transform_hierarchy_depth_inter = 0,
            .max_transform_hierarchy_depth_intra = 0,
        };
        return S_OK;
      });

  // A BT.2020 PQ HDR frame delivered as an RGBA shared image should select
  // P010 as the encoder input format so that the D3D12 video processor
  // converts the RGB input to 10-bit before encoding.
  VideoEncodeAccelerator::Config config = GetDefaultH265Config();
  config.output_profile = HEVCPROFILE_MAIN10;
  config.input_format = PIXEL_FORMAT_ARGB;
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());
  EXPECT_EQ(encoder_delegate_->GetFormatForTesting(), DXGI_FORMAT_P010);

  auto input_frame =
      CreateResource(config.input_visible_size, config.input_format);
  constexpr size_t kBufferSize = 1024;
  constexpr size_t kStreamSize = 512;
  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
  BitstreamBuffer bitstream_buffer(0, shared_memory.Duplicate(), kBufferSize);

  // BT.2020 PQ HDR10 input carried in an RGB frame (matrix RGB).
  gfx::ColorSpace hdr_rgb_color_space(
      gfx::ColorSpace::PrimaryID::BT2020, gfx::ColorSpace::TransferID::PQ,
      gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL);
  gfx::ColorSpace expected_output_color_space =
      GetEncoderOutputColorSpaceFromInputColorSpace(hdr_rgb_color_space);

  // The video processor must be invoked to convert the RGB input to P010,
  // using the HDR input color space and the derived YUV output color space.
  EXPECT_CALL(*GetVideoProcessorWrapper(),
              ProcessFrames(_, _, hdr_rgb_color_space, _, _, _,
                            expected_output_color_space, _))
      .Times(1);
  EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata())
      .WillOnce(Return(GetEncoderOutputMetadataResourceMap(kStreamSize)));
  EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
      .WillOnce(Return(EncoderStatus::Codes::kOk));
  EXPECT_CALL(*GetVideoEncoderWrapper(), ReadbackBitstream)
      .WillOnce([&](base::span<uint8_t> bitstream_buffer) {
        constexpr base::span kStartCode = base::span_from_cstring("\0\0\1");
        EXPECT_GE(bitstream_buffer.size(), kStartCode.size());
        std::ranges::copy(kStartCode, bitstream_buffer.begin());
        return EncoderStatus::Codes::kOk;
      });

  gfx::HDRMetadata hdr_metadata;
  skhdr::MasteringDisplayColorVolume mdcv;
  mdcv.fDisplayPrimaries = {0.680f, 0.320f, 0.265f,  0.690f,
                            0.150f, 0.060f, 0.3127f, 0.3290f};
  mdcv.fMaximumDisplayMasteringLuminance = 1000.0f;
  mdcv.fMinimumDisplayMasteringLuminance = 0.05f;
  hdr_metadata.SetMDCV(mdcv);
  hdr_metadata.SetCLLI(
      skhdr::ContentLightLevelInformation::MakeUint16(/*maxCLL=*/1000,
                                                      /*maxFALL=*/400));

  auto result_or_error = encoder_delegate_->Encode(
      input_frame, hdr_rgb_color_space, bitstream_buffer,
      VideoEncoder::EncodeOptions(), hdr_metadata);
  ASSERT_TRUE(result_or_error.has_value());

  BitstreamBufferMetadata metadata =
      std::move(result_or_error).value().metadata;
  ASSERT_GT(metadata.payload_size_bytes, kStreamSize);
  ASSERT_LE(metadata.payload_size_bytes, kBufferSize);
  // The encoded stream should be tagged with the derived HDR output color
  // space.
  EXPECT_EQ(metadata.encoded_color_space, expected_output_color_space);

  H265Parser parser;
  base::WritableSharedMemoryMapping map = shared_memory.Map();
  parser.SetStream(map.GetMemoryAsSpan<uint8_t>());

  // VPS.
  H265NALU nalu;
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::VPS_NUT);
  int vps_id;
  ASSERT_EQ(parser.ParseVPS(&vps_id), H265Parser::Result::kOk);

  // SPS: verify Main10 bit depth and HDR VUI colour signalling.
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::SPS_NUT);
  int sps_id;
  ASSERT_EQ(parser.ParseSPS(&sps_id), H265Parser::Result::kOk);
  const H265SPS* sps = parser.GetSPS(sps_id);
  ASSERT_TRUE(sps);
  EXPECT_EQ(sps->bit_depth_luma_minus8, 2);
  EXPECT_EQ(sps->bit_depth_chroma_minus8, 2);
  EXPECT_TRUE(sps->vui_parameters_present_flag);
  EXPECT_TRUE(sps->vui_parameters.colour_description_present_flag);
  EXPECT_EQ(sps->vui_parameters.colour_primaries, 9);           // BT.2020
  EXPECT_EQ(sps->vui_parameters.transfer_characteristics, 16);  // PQ
  EXPECT_EQ(sps->vui_parameters.matrix_coeffs, 9);              // BT.2020-NCL

  // PPS.
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::PPS_NUT);
  int pps_id;
  ASSERT_EQ(parser.ParsePPS(nalu, &pps_id), H265Parser::Result::kOk);

  // Prefix SEI carrying the HDR static metadata.
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), H265Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H265NALU::PREFIX_SEI_NUT);
  H265SEI sei;
  ASSERT_EQ(parser.ParseSEI(&sei), H265Parser::Result::kOk);
  bool found_mdcv = false;
  bool found_clli = false;
  for (const auto& sei_msg : sei.msgs) {
    if (std::holds_alternative<H26xSEIMasteringDisplayInfo>(sei_msg)) {
      found_mdcv = true;
    } else if (std::holds_alternative<H26xSEIContentLightLevelInfo>(sei_msg)) {
      found_clli = true;
    }
  }
  EXPECT_TRUE(found_mdcv);
  EXPECT_TRUE(found_clli);
}

TEST_F(D3D12VideoEncodeH265DelegateTest, EncodeFramesAndVerifyKeyFrameFlag) {
  VideoEncodeAccelerator::Config config = GetDefaultH265Config();
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
              sizeof(D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_HEVC)) {
            return EncoderStatus::Codes::kSystemAPICallError;
          }
          EXPECT_EQ(input_arguments.PictureControlDesc.PictureControlCodecData
                            .pHEVCPicData->FrameType ==
                        D3D12_VIDEO_ENCODER_FRAME_TYPE_HEVC_IDR_FRAME,
                    should_be_key_frame);
          return EncoderStatus::Codes::kOk;
        });
    EXPECT_CALL(*GetVideoEncoderWrapper(), ReadbackBitstream)
        .WillOnce([&](base::span<uint8_t> bitstream_buffer) {
          constexpr base::span kStartCode = base::span_from_cstring("\0\0\1");
          EXPECT_GE(bitstream_buffer.size(), kStartCode.size());
          std::ranges::copy(kStartCode, bitstream_buffer.begin());
          return EncoderStatus::Codes::kOk;
        });
    auto result_or_error = encoder_delegate_->Encode(
        input_frame, gfx::ColorSpace::CreateSRGB(), bitstream_buffer,
        VideoEncoder::EncodeOptions());
    ASSERT_TRUE(result_or_error.has_value());
    Mock::VerifyAndClearExpectations(GetVideoEncoderWrapper());
  }
}

}  // namespace media
