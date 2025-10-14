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
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

namespace media {

constexpr uint32_t kInputFrameWidth = 1280;
constexpr uint32_t kInputFrameHeight = 720;
constexpr VideoCodecProfile kAV1Profile = AV1PROFILE_PROFILE_MAIN;

uint8_t AV1QPtoQindex(uint8_t avenc_qp) {
  uint8_t q_index = avenc_qp * 4;
  if (q_index == 248) {
    q_index = 249;
  } else if (q_index == 252) {
    q_index = 255;
  }
  return q_index;
}

class MockD3D12VideoEncodeAV1Delegate : public D3D12VideoEncodeAV1Delegate {
 public:
  explicit MockD3D12VideoEncodeAV1Delegate(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device)
      : D3D12VideoEncodeAV1Delegate(std::move(video_device)) {}

  MOCK_METHOD(EncoderStatus::Or<size_t>,
              GetEncodedBitstreamWrittenBytesCount,
              (const ScopedD3D12ResourceMap& metadata),
              (override));
};

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
        .WillByDefault([](D3D12_FEATURE_VIDEO feature,
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
            av1_support->SupportedInterpolationFilters =
                D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_FLAG_EIGHTTAP;
            av1_support->SupportedFeatureFlags =
                D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_CDEF_FILTERING |
                D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_ORDER_HINT_TOOLS |
                D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_LOOP_RESTORATION_FILTER |
                D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_REDUCED_TX_SET;
            av1_support->RequiredFeatureFlags =
                D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_LOOP_RESTORATION_FILTER;
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
        });

    encoder_delegate_ =
        std::make_unique<MockD3D12VideoEncodeAV1Delegate>(video_device3_);
    encoder_delegate_->SetFactoriesForTesting(
        base::BindRepeating(&CreateVideoEncoderWrapper),
        base::BindRepeating(&CreateVideoProcessorWrapper));
  }

  MockD3D12VideoEncodeAV1Delegate* GetMockDelegate() {
    return static_cast<MockD3D12VideoEncodeAV1Delegate*>(
        encoder_delegate_.get());
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

  void UpdatePostEncodeValues(
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES& post_encode_values,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAGS& post_encode_flags) {
    GetMockDelegate()->UpdateFrameHeaderPostEncode(
        post_encode_flags, post_encode_values, frame_header_);
  }

  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
  Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> video_device3_;
  AV1BitstreamBuilder::FrameHeader frame_header_{};
};

TEST_F(D3D12VideoEncodeAV1DelegateTest, GetSupportedProfiles) {
  std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
      expected_profiles = {
          {AV1PROFILE_PROFILE_MAIN,
           {PIXEL_FORMAT_NV12, PIXEL_FORMAT_P010LE, PIXEL_FORMAT_ABGR}}};
  EXPECT_CALL(*video_device3_.Get(), CheckFeatureSupport).Times(7);
  auto profiles =
      D3D12VideoEncodeAV1Delegate::GetSupportedProfiles(video_device3_.Get());
  EXPECT_EQ(profiles, expected_profiles);
}

TEST_F(D3D12VideoEncodeAV1DelegateTest, GetSupportedProfiles_HighProfile) {
  // Simulate only AV1 high profile is supported and only PIXEL_FORMAT_ABGR is
  // supported for it. Patch the mock to only support high profile and ABGR
  // format.
  ON_CALL(*video_device3_.Get(), CheckFeatureSupport(_, _, _))
      .WillByDefault([](D3D12_FEATURE_VIDEO feature, void* pFeatureSupportData,
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
          feature_data->IsSupported = (*feature_data->Profile.pAV1Profile ==
                                       D3D12_VIDEO_ENCODER_AV1_PROFILE_HIGH);
        } else if (feature == D3D12_FEATURE_VIDEO_ENCODER_INPUT_FORMAT) {
          auto* feature_data =
              static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT*>(
                  pFeatureSupportData);
          CHECK_EQ(feature_data->Codec, D3D12_VIDEO_ENCODER_CODEC_AV1);
          CHECK_EQ(*feature_data->Profile.pAV1Profile,
                   D3D12_VIDEO_ENCODER_AV1_PROFILE_HIGH);
          feature_data->IsSupported = feature_data->Format == DXGI_FORMAT_AYUV;
        } else if (feature ==
                   D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT) {
          auto* feature_data = static_cast<
              D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT*>(
              pFeatureSupportData);
          CHECK_EQ(feature_data->Codec, D3D12_VIDEO_ENCODER_CODEC_AV1);
          CHECK_LE(*feature_data->Profile.pAV1Profile,
                   D3D12_VIDEO_ENCODER_AV1_PROFILE_HIGH);
          auto* av1_support = feature_data->CodecSupportLimits.pAV1Support;
          av1_support->SupportedInterpolationFilters =
              D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS_FLAG_EIGHTTAP;
          av1_support->SupportedFeatureFlags =
              D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_CDEF_FILTERING |
              D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_ORDER_HINT_TOOLS |
              D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_LOOP_RESTORATION_FILTER |
              D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_REDUCED_TX_SET;
          av1_support->RequiredFeatureFlags =
              D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAG_LOOP_RESTORATION_FILTER;
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
      });
  std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
      expected_profiles = {{AV1PROFILE_PROFILE_HIGH, {PIXEL_FORMAT_ABGR}}};
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
    // AV1 output metadata includes post encode syntax values, so we need
    // larger buffer.
    constexpr size_t kBufferSize = 4096;
    constexpr size_t kStreamSize = 3072;
    auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
    BitstreamBuffer bitstream_buffer(base::RandInt(0, 7 /*MaxDPBSize - 1*/),
                                     shared_memory.Duplicate(), kBufferSize);
    EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
        .WillOnce(Return(EncoderStatus::Codes::kOk));
    EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata)
        .WillRepeatedly(
            [&] { return GetEncoderOutputMetadataResourceMap(kStreamSize); });
    EXPECT_CALL(*GetMockDelegate(), GetEncodedBitstreamWrittenBytesCount(_))
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

TEST_F(D3D12VideoEncodeAV1DelegateTest, ExternalRateControl) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  config.bitrate = Bitrate::ExternalRateControl();
  EXPECT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  std::array<uint8_t, 3> quantizers = {56, 26, 10};
  for (size_t i = 0; i < quantizers.size(); i++) {
    auto input_frame = MakeComPtr<NiceMock<D3D12ResourceMock>>();
    EXPECT_CALL(*input_frame.Get(), GetDesc())
        .WillOnce(Return(D3D12_RESOURCE_DESC{
            .Width = static_cast<UINT64>(config.input_visible_size.width()),
            .Height = static_cast<UINT>(config.input_visible_size.height()),
            .Format = VideoPixelFormatToDxgiFormat(config.input_format),
        }));
    constexpr size_t kBufferSize = 4096;
    constexpr size_t kStreamSize = 3072;
    auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
    BitstreamBuffer bitstream_buffer(base::RandInt(0, 7 /*MaxDPBSize - 1*/),
                                     shared_memory.Duplicate(), kBufferSize);
    EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
        .WillOnce(Return(EncoderStatus::Codes::kOk));
    EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata)
        .WillRepeatedly(
            [&] { return GetEncoderOutputMetadataResourceMap(kStreamSize); });
    EXPECT_CALL(*GetMockDelegate(), GetEncodedBitstreamWrittenBytesCount(_))
        .WillRepeatedly(Return(kStreamSize));

    VideoEncoder::EncodeOptions options;
    options.quantizer = quantizers[i];
    auto result = encoder_delegate_->Encode(
        input_frame.Get(), 0 /*input_frame_subresource*/,
        gfx::ColorSpace::CreateSRGB(), bitstream_buffer, options);
    EXPECT_EQ(result.has_value(), true);
    auto [bitstream_buffer_id, metadata] = std::move(result).value();
    EXPECT_EQ(metadata.qp, AV1QPtoQindex(quantizers[i]));
  }
}

// Test post encode update of frame header through
// UpdateFrameHeaderPostEncode() with every flag that is possible.
TEST_F(D3D12VideoEncodeAV1DelegateTest, UpdateFrameHeaderPostEncode) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  EXPECT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES post_encode_values{};
  D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAGS post_encode_flags =
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_NONE;

  // CDEF
  constexpr std::array<uint8_t, 8> kCdefPriStrength = {9, 12, 0, 6, 2, 4, 1, 2};
  constexpr std::array<uint8_t, 8> kCdefSecStrength = {0, 2, 0, 0, 0, 1, 0, 1};
  post_encode_flags = D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_CDEF_DATA;
  post_encode_values.CDEF.CdefBits = 3;
  post_encode_values.CDEF.CdefDampingMinus3 = 2;
  for (uint32_t i = 0; i < (1 << post_encode_values.CDEF.CdefBits); i++) {
    base::span(post_encode_values.CDEF.CdefYPriStrength)[i] =
        kCdefPriStrength[i];
    base::span(post_encode_values.CDEF.CdefUVPriStrength)[i] =
        kCdefPriStrength[i];
    base::span(post_encode_values.CDEF.CdefYSecStrength)[i] =
        kCdefSecStrength[i];
    base::span(post_encode_values.CDEF.CdefUVSecStrength)[i] =
        kCdefSecStrength[i];
  }
  UpdatePostEncodeValues(post_encode_values, post_encode_flags);
  EXPECT_EQ(frame_header_.cdef_damping_minus_3, 2);
  EXPECT_EQ(frame_header_.cdef_bits, 3);
  EXPECT_THAT(frame_header_.cdef_y_pri_strength,
              ::testing::ElementsAreArray(kCdefPriStrength));
  EXPECT_THAT(frame_header_.cdef_y_sec_strength,
              ::testing::ElementsAreArray(kCdefSecStrength));
  EXPECT_THAT(frame_header_.cdef_uv_pri_strength,
              ::testing::ElementsAreArray(kCdefPriStrength));
  EXPECT_THAT(frame_header_.cdef_uv_sec_strength,
              ::testing::ElementsAreArray(kCdefSecStrength));

  // Loop filter
  post_encode_flags =
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_LOOP_FILTER;
  post_encode_values.LoopFilter.LoopFilterLevel[0] = 5;
  post_encode_values.LoopFilter.LoopFilterLevel[1] = 5;
  post_encode_values.LoopFilter.LoopFilterLevelU = 5;
  post_encode_values.LoopFilter.LoopFilterLevelV = 5;
  post_encode_values.LoopFilter.LoopFilterSharpnessLevel = 0;
  post_encode_values.LoopFilter.LoopFilterDeltaEnabled = true;
  post_encode_values.LoopFilter.UpdateRefDelta = true;
  constexpr std::array<int8_t, 8> kRefDeltas = {1, -1, 0, 0, 0, 0, 0, 0};
  for (size_t i = 0; i < kRefDeltas.size(); ++i) {
    base::span(post_encode_values.LoopFilter.RefDeltas)[i] = kRefDeltas[i];
  }
  post_encode_values.LoopFilter.UpdateModeDelta = true;
  constexpr std::array<int8_t, 2> kModeDeltas = {1, -1};
  for (size_t i = 0; i < kModeDeltas.size(); ++i) {
    base::span(post_encode_values.LoopFilter.ModeDeltas)[i] = kModeDeltas[i];
  }
  UpdatePostEncodeValues(post_encode_values, post_encode_flags);
  EXPECT_TRUE(frame_header_.loop_filter_delta_enabled);
  EXPECT_TRUE(frame_header_.loop_filter_delta_update);
  EXPECT_EQ(frame_header_.filter_level[0], 5u);
  EXPECT_EQ(frame_header_.filter_level[1], 5u);
  EXPECT_EQ(frame_header_.filter_level_u, 5u);
  EXPECT_EQ(frame_header_.filter_level_v, 5u);
  EXPECT_EQ(frame_header_.sharpness_level, 0u);
  EXPECT_THAT(frame_header_.loop_filter_ref_deltas,
              ::testing::ElementsAreArray(kRefDeltas));
  EXPECT_THAT(frame_header_.loop_filter_mode_deltas,
              ::testing::ElementsAreArray(kModeDeltas));

  // Loop filter delta
  post_encode_flags =
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_LOOP_FILTER_DELTA;
  post_encode_values.LoopFilterDelta.DeltaLFPresent = true;
  post_encode_values.LoopFilterDelta.DeltaLFMulti = true;
  post_encode_values.LoopFilterDelta.DeltaLFRes = 1;
  UpdatePostEncodeValues(post_encode_values, post_encode_flags);
  EXPECT_TRUE(frame_header_.delta_lf_present);
  EXPECT_TRUE(frame_header_.delta_lf_multi);
  EXPECT_EQ(frame_header_.delta_lf_res, 1u);

  // Quantization
  post_encode_flags =
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_QUANTIZATION;
  post_encode_values.Quantization.BaseQIndex = 100;
  post_encode_values.Quantization.YDCDeltaQ = 1;
  post_encode_values.Quantization.UDCDeltaQ = 2;
  post_encode_values.Quantization.UACDeltaQ = 3;
  post_encode_values.Quantization.VDCDeltaQ = 4;
  post_encode_values.Quantization.VACDeltaQ = 5;
  post_encode_values.Quantization.UsingQMatrix = true;
  post_encode_values.Quantization.QMY = 1;
  post_encode_values.Quantization.QMU = 2;
  post_encode_values.Quantization.QMV = 3;
  UpdatePostEncodeValues(post_encode_values, post_encode_flags);
  EXPECT_EQ(frame_header_.base_qindex,
            post_encode_values.Quantization.BaseQIndex);
  EXPECT_TRUE(frame_header_.separate_uv_delta_q);
  EXPECT_EQ(frame_header_.delta_q_y_dc,
            post_encode_values.Quantization.YDCDeltaQ);
  EXPECT_EQ(frame_header_.delta_q_u_dc,
            post_encode_values.Quantization.UDCDeltaQ);
  EXPECT_EQ(frame_header_.delta_q_u_ac,
            post_encode_values.Quantization.UACDeltaQ);
  EXPECT_EQ(frame_header_.delta_q_v_dc,
            post_encode_values.Quantization.VDCDeltaQ);
  EXPECT_EQ(frame_header_.delta_q_v_ac,
            post_encode_values.Quantization.VACDeltaQ);
  EXPECT_TRUE(frame_header_.using_qmatrix);
  EXPECT_EQ(frame_header_.qm_y, post_encode_values.Quantization.QMY);
  EXPECT_EQ(frame_header_.qm_u, post_encode_values.Quantization.QMU);
  EXPECT_EQ(frame_header_.qm_v, post_encode_values.Quantization.QMV);

  // Quantization delta
  post_encode_flags =
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_QUANTIZATION_DELTA;
  post_encode_values.QuantizationDelta.DeltaQPresent = true;
  post_encode_values.QuantizationDelta.DeltaQRes = 1;
  UpdatePostEncodeValues(post_encode_values, post_encode_flags);
  EXPECT_TRUE(frame_header_.delta_q_present);
  EXPECT_EQ(frame_header_.delta_q_res,
            post_encode_values.QuantizationDelta.DeltaQRes);

  // Primary reference frame
  post_encode_flags =
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_PRIMARY_REF_FRAME;
  post_encode_values.PrimaryRefFrame = 2;
  UpdatePostEncodeValues(post_encode_values, post_encode_flags);
  EXPECT_EQ(frame_header_.primary_ref_frame,
            post_encode_values.PrimaryRefFrame);

  // Reference indices
  constexpr std::array<uint8_t, 7> kReferenceIndices = {0, 1, 1, 2, 2, 3, 3};
  post_encode_flags =
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_REFERENCE_INDICES;
  for (uint32_t i = 0; i < kReferenceIndices.size(); ++i) {
    base::span(post_encode_values.ReferenceIndices)[i] = kReferenceIndices[i];
  }
  UpdatePostEncodeValues(post_encode_values, post_encode_flags);
  EXPECT_THAT(frame_header_.ref_frame_idx,
              ::testing::ElementsAreArray(kReferenceIndices));

  // Compound prediction type
  post_encode_flags =
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAG_COMPOUND_PREDICTION_MODE;
  post_encode_values.CompoundPredictionType = 1;
  UpdatePostEncodeValues(post_encode_values, post_encode_flags);
  EXPECT_EQ(frame_header_.reference_select,
            post_encode_values.CompoundPredictionType);
}

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_AV1_DELEGATE_UNITTEST_H_
