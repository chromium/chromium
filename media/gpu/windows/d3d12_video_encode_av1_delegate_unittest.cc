// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"

#include <array>
#include <memory>
#include <new>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/video_encoder.h"
#include "media/base/win/d3d12_mocks.h"
#include "media/base/win/d3d12_video_mocks.h"
#include "media/gpu/windows/d3d12_video_encode_delegate_unittest.h"
#include "media/gpu/windows/format_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "ui/gfx/geometry/rect.h"

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

namespace media {

constexpr uint32_t kInputFrameWidth = 1280;
constexpr uint32_t kInputFrameHeight = 720;
constexpr VideoCodecProfile kAV1Profile = AV1PROFILE_PROFILE_MAIN;

// A minimal tile group OBU payload, so that the temporal unit the delegate
// packs around it is parseable by libgav1.
constexpr std::array<uint8_t, 2> kDummyTileGroupObu = {0x00, 0x80};

class MockD3D12VideoEncodeAV1Delegate : public D3D12VideoEncodeAV1Delegate {
 public:
  explicit MockD3D12VideoEncodeAV1Delegate(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds)
      : D3D12VideoEncodeAV1Delegate(std::move(video_device), gpu_workarounds) {}

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
            // SuggestedLevel is an output parameter the delegate copies into
            // the sequence header's seq_level_idx. Report level 4.0 (index 8):
            // high enough that its MaxPicSize covers the 1280x720 test
            // resolution so the emitted stream stays libgav1-conformant, and
            // >7 so the seq_tier bit is exercised.
            if (feature_data->SuggestedLevel.pAV1LevelSetting) {
              *feature_data->SuggestedLevel.pAV1LevelSetting = {
                  .Level = D3D12_VIDEO_ENCODER_AV1_LEVELS_4_0,
                  .Tier = D3D12_VIDEO_ENCODER_AV1_TIER_MAIN};
            }
          }
          return S_OK;
        });

    const gpu::GpuDriverBugWorkarounds gpu_workarounds{};
    encoder_delegate_ = std::make_unique<MockD3D12VideoEncodeAV1Delegate>(
        video_device3_, gpu_workarounds);
    encoder_delegate_->SetFactoriesForTesting(
        base::BindRepeating(&CreateVideoEncoderWrapper),
        base::BindRepeating(&CreateVideoProcessorWrapper));

    buffer_pool_ = std::make_unique<libgav1::BufferPool>(
        /*on_frame_buffer_size_changed=*/nullptr,
        /*get_frame_buffer=*/nullptr,
        /*release_frame_buffer=*/nullptr,
        /*callback_private_data=*/nullptr);
    av1_decoder_state_ = std::make_unique<libgav1::DecoderState>();
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

  // `D3D12VideoEncodeAV1DelegateTest` is a friend of the delegate, but the
  // TEST_F bodies are subclasses of it and are not, so private state has to be
  // reached through the fixture.
  const AV1BitstreamBuilder::SequenceHeader& GetSequenceHeader() {
    return GetMockDelegate()->sequence_header_;
  }

  void UpdatePostEncodeValues(
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES& post_encode_values,
      D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAGS& post_encode_flags) {
    GetMockDelegate()->UpdateFrameHeaderPostEncode(
        post_encode_flags, post_encode_values, frame_header_);
  }

  // Encodes a single keyframe at `qindex` with external rate control, so that
  // the delegate takes the CQP path that estimates the loop filter level
  // itself, and returns the level it picked.
  int EncodeKeyFrameAndGetLoopFilterLevel(
      const VideoEncodeAccelerator::Config& config,
      const gfx::ColorSpace& color_space,
      int qindex) {
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
    BitstreamBuffer bitstream_buffer(0, shared_memory.Duplicate(), kBufferSize);
    EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
        .WillOnce(Return(EncoderStatus::Codes::kOk));
    EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata)
        .WillRepeatedly(
            [&] { return GetEncoderOutputMetadataResourceMap(kStreamSize); });
    EXPECT_CALL(*GetMockDelegate(), GetEncodedBitstreamWrittenBytesCount(_))
        .WillRepeatedly(Return(kStreamSize));

    VideoEncoder::EncodeOptions options;
    options.quantizer = qindex;
    auto result = encoder_delegate_->Encode(
        {input_frame.Get()}, gfx::Rect(config.input_visible_size), color_space,
        bitstream_buffer, options);
    if (!result.has_value()) {
      ADD_FAILURE() << "Encode() failed: "
                    << std::move(result).error().message();
      return -1;
    }
    EXPECT_TRUE(std::move(result).value().metadata.key_frame);
    return base::span(
        GetMockDelegate()->picture_params_.LoopFilter.LoopFilterLevel)[0];
  }

  // Encodes a single key frame and returns the produced bitstream, which is a
  // complete temporal unit: the header OBUs the delegate packs, followed by
  // `kDummyTileGroupObu` as the encoder output.
  std::vector<uint8_t> EncodeKeyFrame(
      const VideoEncodeAccelerator::Config& config,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      const VideoEncoder::EncodeOptions& options =
          VideoEncoder::EncodeOptions()) {
    auto input_frame =
        CreateResource(config.input_visible_size, config.input_format);
    // The metadata resource has to be large enough for the AV1 post encode
    // values, independently of how large the encoded bitstream is.
    constexpr size_t kMetadataSize = 4096;
    constexpr size_t kBufferSize = 1024;
    auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
    BitstreamBuffer bitstream_buffer(0, shared_memory.Duplicate(), kBufferSize);
    EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
        .WillOnce(Return(EncoderStatus::Codes::kOk));
    EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata)
        .WillRepeatedly(
            [&] { return GetEncoderOutputMetadataResourceMap(kMetadataSize); });
    EXPECT_CALL(*GetMockDelegate(), GetEncodedBitstreamWrittenBytesCount(_))
        .WillRepeatedly(Return(kDummyTileGroupObu.size()));
    EXPECT_CALL(*GetVideoEncoderWrapper(), ReadbackBitstream)
        .WillOnce([](base::span<uint8_t> buffer) {
          EXPECT_EQ(buffer.size(), kDummyTileGroupObu.size());
          buffer.copy_from(base::span(kDummyTileGroupObu));
          return EncoderStatus::Codes::kOk;
        });

    auto result = encoder_delegate_->Encode(
        {input_frame.Get()}, gfx::Rect(config.input_visible_size), color_space,
        bitstream_buffer, options, hdr_metadata);
    if (!result.has_value()) {
      ADD_FAILURE() << "Encode() failed: "
                    << std::move(result).error().message();
      return {};
    }
    BitstreamBufferMetadata metadata = std::move(result).value().metadata;
    EXPECT_TRUE(metadata.key_frame);
    base::WritableSharedMemoryMapping map = shared_memory.Map();
    base::span bitstream =
        map.GetMemoryAsSpan<uint8_t>().first(metadata.payload_size_bytes);
    return std::vector<uint8_t>(bitstream.begin(), bitstream.end());
  }

  // Parses `bitstream` as a single AV1 temporal unit. The parsed sequence
  // header is available afterwards through `parser_`.
  libgav1::StatusCode ParseTemporalUnit(base::span<const uint8_t> bitstream,
                                        libgav1::RefCountedBufferPtr* frame) {
    parser_ = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
        bitstream.data(), bitstream.size(), 0, buffer_pool_.get(),
        av1_decoder_state_.get()));
    return parser_->ParseOneFrame(frame);
  }

  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
  Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> video_device3_;
  AV1BitstreamBuilder::FrameHeader frame_header_{};
  std::unique_ptr<libgav1::BufferPool> buffer_pool_;
  std::unique_ptr<libgav1::DecoderState> av1_decoder_state_;
  std::unique_ptr<libgav1::ObuParser> parser_;
};

TEST_F(D3D12VideoEncodeAV1DelegateTest, GetSupportedProfiles) {
  std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
      expected_profiles = {
          {AV1PROFILE_PROFILE_MAIN,
           {PIXEL_FORMAT_NV12, PIXEL_FORMAT_P010LE, PIXEL_FORMAT_ABGR}}};
  EXPECT_CALL(*video_device3_.Get(), CheckFeatureSupport).Times(7);
  auto profiles = D3D12VideoEncodeAV1Delegate::GetSupportedProfiles(
      video_device3_.Get(), gpu::GpuDriverBugWorkarounds());
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
  auto profiles = D3D12VideoEncodeAV1Delegate::GetSupportedProfiles(
      video_device3_.Get(), gpu::GpuDriverBugWorkarounds());
  EXPECT_EQ(profiles, expected_profiles);
}

TEST_F(D3D12VideoEncodeAV1DelegateTest,
       GetSupportedProfiles_WorkaroundLimitsToMain) {
  // Simulate a driver that supports all three AV1 profiles (Main, High, Pro).
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
          // All profiles are supported by the driver.
          feature_data->IsSupported = true;
        } else if (feature == D3D12_FEATURE_VIDEO_ENCODER_INPUT_FORMAT) {
          auto* feature_data =
              static_cast<D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT*>(
                  pFeatureSupportData);
          CHECK_EQ(feature_data->Codec, D3D12_VIDEO_ENCODER_CODEC_AV1);
          feature_data->IsSupported = true;
        }
        return S_OK;
      });

  // With the workaround enabled, only Main profile should be returned even
  // though the driver claims to support all profiles.
  gpu::GpuDriverBugWorkarounds workarounds;
  workarounds.limit_d3d12_av1_profile_to_main = true;
  auto profiles = D3D12VideoEncodeAV1Delegate::GetSupportedProfiles(
      video_device3_.Get(), workarounds);
  ASSERT_EQ(profiles.size(), 1u);
  EXPECT_EQ(profiles[0].first, AV1PROFILE_PROFILE_MAIN);
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
    BitstreamBuffer bitstream_buffer(
        base::RandIntInclusive(0, 7 /*MaxDPBSize - 1*/),
        shared_memory.Duplicate(), kBufferSize);
    EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
        .WillOnce(Return(EncoderStatus::Codes::kOk));
    EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata)
        .WillRepeatedly(
            [&] { return GetEncoderOutputMetadataResourceMap(kStreamSize); });
    EXPECT_CALL(*GetMockDelegate(), GetEncodedBitstreamWrittenBytesCount(_))
        .WillRepeatedly(Return(kStreamSize));

    auto result = encoder_delegate_->Encode(
        {input_frame.Get()}, gfx::Rect(config.input_visible_size),
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

// AV1 main profile covers both 8 and 10 bit, so a P010 input must be encoded at
// 10 bit rather than being converted down to NV12.
TEST_F(D3D12VideoEncodeAV1DelegateTest, EncodeFrameWith10BitInput) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  config.input_format = PIXEL_FORMAT_P010LE;
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());
  EXPECT_EQ(encoder_delegate_->GetFormatForTesting(), DXGI_FORMAT_P010);
  EXPECT_EQ(GetSequenceHeader().bit_depth, 10);

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
  BitstreamBuffer bitstream_buffer(0, shared_memory.Duplicate(), kBufferSize);
  EXPECT_CALL(*GetVideoEncoderWrapper(), Encode)
      .WillOnce(Return(EncoderStatus::Codes::kOk));
  EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncoderOutputMetadata)
      .WillRepeatedly(
          [&] { return GetEncoderOutputMetadataResourceMap(kStreamSize); });
  EXPECT_CALL(*GetMockDelegate(), GetEncodedBitstreamWrittenBytesCount(_))
      .WillRepeatedly(Return(kStreamSize));

  auto result = encoder_delegate_->Encode(
      {input_frame.Get()}, gfx::Rect(config.input_visible_size),
      gfx::ColorSpace::CreateHDR10(), bitstream_buffer,
      VideoEncoder::EncodeOptions());
  ASSERT_TRUE(result.has_value());
  auto [bitstream_buffer_id, metadata] = std::move(result).value();
  EXPECT_EQ(bitstream_buffer_id, bitstream_buffer.id());
  EXPECT_TRUE(metadata.key_frame);
  EXPECT_GT(metadata.payload_size_bytes, kStreamSize);
  EXPECT_LE(metadata.payload_size_bytes, kBufferSize);
}

TEST_F(D3D12VideoEncodeAV1DelegateTest, EncodeHDR10Frame) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  config.input_format = PIXEL_FORMAT_P010LE;
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  gfx::HDRMetadata hdr_metadata;
  skhdr::MasteringDisplayColorVolume mdcv;
  // BT.2020 primaries in R, G, B order, followed by the D65 white point.
  mdcv.fDisplayPrimaries = {0.708f, 0.292f, 0.170f,  0.797f,
                            0.131f, 0.046f, 0.3127f, 0.3290f};
  mdcv.fMaximumDisplayMasteringLuminance = 1000.0f;
  mdcv.fMinimumDisplayMasteringLuminance = 0.05f;
  hdr_metadata.SetMDCV(mdcv);
  hdr_metadata.SetCLLI(
      skhdr::ContentLightLevelInformation::MakeUint16(/*maxCLL=*/1000,
                                                      /*maxFALL=*/400));

  std::vector<uint8_t> bitstream =
      EncodeKeyFrame(config, gfx::ColorSpace::CreateHDR10(), hdr_metadata);
  ASSERT_FALSE(bitstream.empty());

  libgav1::RefCountedBufferPtr frame;
  ASSERT_EQ(ParseTemporalUnit(bitstream, &frame), libgav1::kStatusOk);
  ASSERT_NE(frame, nullptr);

  // The sequence header must signal 10 bit BT.2020 PQ.
  const libgav1::ColorConfig& color_config =
      parser_->sequence_header().color_config;
  EXPECT_EQ(color_config.bitdepth, 10);
  EXPECT_EQ(color_config.color_primary, kLibgav1ColorPrimaryBt2020);
  EXPECT_EQ(color_config.transfer_characteristics,
            kLibgav1TransferCharacteristicsSmpte2084);
  EXPECT_EQ(color_config.matrix_coefficients,
            kLibgav1MatrixCoefficientsBt2020Ncl);

  // The content light level metadata OBU.
  ASSERT_TRUE(frame->hdr_cll_set());
  EXPECT_EQ(frame->hdr_cll().max_cll, 1000u);
  EXPECT_EQ(frame->hdr_cll().max_fall, 400u);

  // The mastering display colour volume metadata OBU. Chromaticities are 0.16
  // fixed point, the maximum luminance 24.8 and the minimum luminance 18.14.
  // Scaling by a power of two is exact in floating point, so the rounded
  // results are deterministic.
  ASSERT_TRUE(frame->hdr_mdcv_set());
  const libgav1::ObuMetadataHdrMdcv parsed_mdcv = frame->hdr_mdcv();
  EXPECT_THAT(parsed_mdcv.primary_chromaticity_x,
              ElementsAreArray({46399u, 11141u, 8585u}));  // R, G, B x
  EXPECT_THAT(parsed_mdcv.primary_chromaticity_y,
              ElementsAreArray({19137u, 52232u, 3015u}));  // R, G, B y
  EXPECT_EQ(parsed_mdcv.white_point_chromaticity_x, 20493u);
  EXPECT_EQ(parsed_mdcv.white_point_chromaticity_y, 21561u);
  EXPECT_EQ(parsed_mdcv.luminance_max, 1000u * 256u);
  EXPECT_EQ(parsed_mdcv.luminance_min, 819u);  // 0.05 * 16384 = 819.2
}

// HLG needs no metadata OBU: the sequence header carries all the signalling.
TEST_F(D3D12VideoEncodeAV1DelegateTest, EncodeHLGFrame) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  config.input_format = PIXEL_FORMAT_P010LE;
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  gfx::ColorSpace hlg_color_space(
      gfx::ColorSpace::PrimaryID::BT2020, gfx::ColorSpace::TransferID::HLG,
      gfx::ColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
  std::vector<uint8_t> bitstream =
      EncodeKeyFrame(config, hlg_color_space, gfx::HDRMetadata());
  ASSERT_FALSE(bitstream.empty());

  libgav1::RefCountedBufferPtr frame;
  ASSERT_EQ(ParseTemporalUnit(bitstream, &frame), libgav1::kStatusOk);
  ASSERT_NE(frame, nullptr);

  const libgav1::ColorConfig& color_config =
      parser_->sequence_header().color_config;
  EXPECT_EQ(color_config.bitdepth, 10);
  EXPECT_EQ(color_config.color_primary, kLibgav1ColorPrimaryBt2020);
  EXPECT_EQ(color_config.transfer_characteristics,
            kLibgav1TransferCharacteristicsHlg);
  EXPECT_EQ(color_config.matrix_coefficients,
            kLibgav1MatrixCoefficientsBt2020Ncl);
  EXPECT_FALSE(frame->hdr_cll_set());
  EXPECT_FALSE(frame->hdr_mdcv_set());
}

// An 8 bit stream still gets the colour description, but no HDR metadata, since
// HDR10 static metadata only makes sense for a 10 bit stream.
TEST_F(D3D12VideoEncodeAV1DelegateTest, EncodeHDRFrameWith8BitInput) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  gfx::HDRMetadata hdr_metadata;
  hdr_metadata.SetCLLI(
      skhdr::ContentLightLevelInformation::MakeUint16(/*maxCLL=*/1000,
                                                      /*maxFALL=*/400));
  std::vector<uint8_t> bitstream =
      EncodeKeyFrame(config, gfx::ColorSpace::CreateHDR10(), hdr_metadata);
  ASSERT_FALSE(bitstream.empty());

  libgav1::RefCountedBufferPtr frame;
  ASSERT_EQ(ParseTemporalUnit(bitstream, &frame), libgav1::kStatusOk);
  ASSERT_NE(frame, nullptr);

  const libgav1::ColorConfig& color_config =
      parser_->sequence_header().color_config;
  EXPECT_EQ(color_config.bitdepth, 8);
  EXPECT_EQ(color_config.transfer_characteristics,
            kLibgav1TransferCharacteristicsSmpte2084);
  EXPECT_FALSE(frame->hdr_cll_set());
  EXPECT_FALSE(frame->hdr_mdcv_set());
}

TEST_F(D3D12VideoEncodeAV1DelegateTest,
       EncodeFrameDoesNotSignalIdentityMatrix) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  gfx::ColorSpace identity_color_space(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::SRGB,
      gfx::ColorSpace::MatrixID::GBR, gfx::ColorSpace::RangeID::FULL);
  std::vector<uint8_t> bitstream =
      EncodeKeyFrame(config, identity_color_space, gfx::HDRMetadata());
  ASSERT_FALSE(bitstream.empty());

  libgav1::RefCountedBufferPtr frame;
  ASSERT_EQ(ParseTemporalUnit(bitstream, &frame), libgav1::kStatusOk);
  ASSERT_NE(frame, nullptr);

  const libgav1::ColorConfig& color_config =
      parser_->sequence_header().color_config;
  EXPECT_EQ(color_config.color_primary, kLibgav1ColorPrimaryBt709);
  EXPECT_EQ(color_config.transfer_characteristics,
            kLibgav1TransferCharacteristicsSrgb);
  EXPECT_EQ(color_config.matrix_coefficients,
            kLibgav1MatrixCoefficientsUnspecified);
  EXPECT_EQ(color_config.color_range, kLibgav1ColorRangeFull);
}

// The colour description and the HDR metadata are refreshed on every key frame,
// so a stream that stops carrying them must stop signalling them too.
TEST_F(D3D12VideoEncodeAV1DelegateTest, ColorDescriptionClearedOnNextKeyFrame) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  config.input_format = PIXEL_FORMAT_P010LE;
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  gfx::HDRMetadata hdr_metadata;
  hdr_metadata.SetCLLI(
      skhdr::ContentLightLevelInformation::MakeUint16(/*maxCLL=*/1000,
                                                      /*maxFALL=*/400));
  ASSERT_FALSE(
      EncodeKeyFrame(config, gfx::ColorSpace::CreateHDR10(), hdr_metadata)
          .empty());
  ASSERT_TRUE(GetSequenceHeader().color_description_present_flag);

  // A colour space that carries nothing to signal, on a forced key frame.
  std::vector<uint8_t> bitstream =
      EncodeKeyFrame(config, gfx::ColorSpace(), gfx::HDRMetadata(),
                     VideoEncoder::EncodeOptions(/*key_frame=*/true));
  ASSERT_FALSE(bitstream.empty());
  EXPECT_FALSE(GetSequenceHeader().color_description_present_flag);

  libgav1::RefCountedBufferPtr frame;
  ASSERT_EQ(ParseTemporalUnit(bitstream, &frame), libgav1::kStatusOk);
  ASSERT_NE(frame, nullptr);
  const libgav1::ColorConfig& color_config =
      parser_->sequence_header().color_config;
  EXPECT_EQ(color_config.color_primary, kLibgav1ColorPrimaryUnspecified);
  EXPECT_EQ(color_config.transfer_characteristics,
            kLibgav1TransferCharacteristicsUnspecified);
  EXPECT_EQ(color_config.matrix_coefficients,
            kLibgav1MatrixCoefficientsUnspecified);
  EXPECT_FALSE(frame->hdr_cll_set());
  EXPECT_FALSE(frame->hdr_mdcv_set());
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
    BitstreamBuffer bitstream_buffer(
        base::RandIntInclusive(0, 7 /*MaxDPBSize - 1*/),
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
        {input_frame.Get()}, gfx::Rect(config.input_visible_size),
        gfx::ColorSpace::CreateSRGB(), bitstream_buffer, options);
    EXPECT_EQ(result.has_value(), true);
    auto [bitstream_buffer_id, metadata] = std::move(result).value();
    EXPECT_EQ(metadata.qp, quantizers[i]);
  }
}

// qindex 128 maps to q = 176 in the 8 bit lookup, and the keyframe fit gives
// (176 * 17563 - 421574 + (1 << 17)) >> 18 = 10.
TEST_F(D3D12VideoEncodeAV1DelegateTest, LoopFilterLevelForCqp8Bit) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  config.bitrate = Bitrate::ExternalRateControl();
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());
  ASSERT_EQ(GetSequenceHeader().bit_depth, 8);

  EXPECT_EQ(EncodeKeyFrameAndGetLoopFilterLevel(
                config, gfx::ColorSpace::CreateSRGB(), /*qindex=*/128),
            10);
}

// qindex 128 maps to q = 700 in the 10 bit lookup, and the 10 bit fit gives
// (700 * 20723 + 4060632 + (1 << 19)) >> 20 = 18.
TEST_F(D3D12VideoEncodeAV1DelegateTest, LoopFilterLevelForCqp10Bit) {
  VideoEncodeAccelerator::Config config = GetDefaultConfig();
  config.input_format = PIXEL_FORMAT_P010LE;
  config.bitrate = Bitrate::ExternalRateControl();
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());
  ASSERT_EQ(GetSequenceHeader().bit_depth, 10);

  EXPECT_EQ(EncodeKeyFrameAndGetLoopFilterLevel(
                config, gfx::ColorSpace::CreateHDR10(), /*qindex=*/128),
            18);
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
