// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/h264_vaapi_video_encoder_delegate.h"

#include <memory>

#include "base/logging.h"
#include "build/build_config.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace media {
namespace {

VaapiVideoEncoderDelegate::Config kDefaultVEADelegateConfig{
    .max_num_ref_frames = 4,
};

VideoEncodeAccelerator::Config kDefaultVEAConfig(
    PIXEL_FORMAT_I420,
    gfx::Size(1280, 720),
    H264PROFILE_BASELINE,
    Bitrate::ConstantBitrate(14000000u)
    /* = maximum bitrate in bits per second for level 3.1 */,
    VideoEncodeAccelerator::kDefaultFramerate,
    absl::nullopt /* gop_length */,
    absl::nullopt /* h264 output level*/,
    false /* is_constrained_h264 */,
    VideoEncodeAccelerator::Config::StorageType::kShmem,
    VideoEncodeAccelerator::Config::ContentType::kCamera);

MATCHER_P2(MatchVABufferDescriptor, va_buffer_type, va_buffer_size, "") {
  return arg.type == va_buffer_type && arg.size == va_buffer_size &&
         arg.data != nullptr;
}

MATCHER_P2(MatchVABufferDescriptorForMiscParam,
           va_misc_param_type,
           va_misc_param_size,
           "") {
  if (arg.type != VAEncMiscParameterBufferType ||
      arg.size != va_misc_param_size + sizeof(VAEncMiscParameterBuffer)) {
    return false;
  }
  const auto* va_buffer =
      static_cast<const VAEncMiscParameterBuffer*>(arg.data);

  return va_buffer != nullptr && va_buffer->type == va_misc_param_type;
}

MATCHER_P(MatchVABufferDescriptorForPackedHeader, va_packed_header_type, "") {
  if (arg.type != VAEncPackedHeaderParameterBufferType ||
      arg.size != sizeof(VAEncPackedHeaderParameterBuffer)) {
    return false;
  }

  const auto* va_buffer =
      static_cast<const VAEncPackedHeaderParameterBuffer*>(arg.data);
  return va_buffer != nullptr && va_buffer->type == va_packed_header_type;
}

MATCHER(MatchVABufferDescriptorForPackedHeaderData, "") {
  return arg.type == VAEncPackedHeaderDataBufferType && arg.data != nullptr;
}

void ValidateTemporalLayerStructure(uint8_t num_temporal_layers,
                                    size_t num_frames,
                                    int frame_num,
                                    uint8_t temporal_idx,
                                    bool ref,
                                    int& previous_frame_num) {
  constexpr size_t kTemporalLayerCycle = 4;
  constexpr uint8_t kExpectedTemporalIdx[][kTemporalLayerCycle] = {
      {0, 1, 0, 1},  // For two temporal layers.
      {0, 2, 1, 2}   // For three temporal layers.
  };

  const uint8_t expected_temporal_idx =
      kExpectedTemporalIdx[num_temporal_layers - 2]
                          [num_frames % kTemporalLayerCycle];
  EXPECT_EQ(temporal_idx, expected_temporal_idx)
      << "Unexpected temporal index: temporal_idx"
      << base::strict_cast<int>(temporal_idx)
      << ", expected=" << base::strict_cast<int>(expected_temporal_idx)
      << ", num_frames=" << num_frames;

  const bool expected_ref = temporal_idx != num_temporal_layers - 1;
  EXPECT_EQ(ref, expected_ref)
      << "Unexpected reference: reference=" << ref
      << ", expected=" << expected_ref
      << ", temporal_idx=" << base::strict_cast<int>(temporal_idx)
      << ", num_frames=" << num_frames;

  if (num_frames == 0) {
    // IDR frame.
    EXPECT_EQ(frame_num, 0);
    previous_frame_num = 0;
    return;
  }

  EXPECT_EQ(frame_num, previous_frame_num + ref);
  previous_frame_num = frame_num;
}

class MockVaapiWrapper : public VaapiWrapper {
 public:
  MockVaapiWrapper() : VaapiWrapper(kEncodeConstantBitrate) {}

  MOCK_METHOD1(SubmitBuffer_Locked, bool(const VABufferDescriptor&));

  bool GetSupportedPackedHeaders(VideoCodecProfile profile,
                                 bool& packed_sps,
                                 bool& packed_pps,
                                 bool& packed_slice) override {
    packed_sps = true;
    packed_pps = true;
    packed_slice = true;
    return true;
  }

 protected:
  ~MockVaapiWrapper() override = default;
};

}  // namespace

class H264VaapiVideoEncoderDelegateTest
    : public ::testing::TestWithParam<uint8_t> {
 public:
  H264VaapiVideoEncoderDelegateTest() = default;
  void SetUp() override;

  void ExpectLevel(uint8_t level) { EXPECT_EQ(encoder_->level_, level); }

  MOCK_METHOD0(OnError, void());

  bool InitializeEncoder(uint8_t num_temporal_layers);
  void EncodeFrame(bool force_keyframe);

 protected:
  std::unique_ptr<H264VaapiVideoEncoderDelegate> encoder_;

 private:
  std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob> CreateEncodeJob(
      bool keyframe);

  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  unsigned int next_surface_id_ = 0;
  size_t num_encode_frames_ = 0;
  int previous_frame_num_ = 0;
};

std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob>
H264VaapiVideoEncoderDelegateTest::CreateEncodeJob(bool keyframe) {
  auto va_surface = base::MakeRefCounted<VASurface>(
      next_surface_id_++, kDefaultVEAConfig.input_visible_size,
      VA_RT_FORMAT_YUV420, base::DoNothing());
  scoped_refptr<H264Picture> picture(new VaapiH264Picture(va_surface));

  constexpr VABufferID kDummyVABufferID = 12;
  auto scoped_va_buffer = ScopedVABuffer::CreateForTesting(
      kDummyVABufferID, VAEncCodedBufferType,
      kDefaultVEAConfig.input_visible_size.GetArea());

  // TODO(b/229358029): Set a valid timestamp and check the timestamp in
  // metadata.
  constexpr base::TimeDelta timestamp;
  return std::make_unique<VaapiVideoEncoderDelegate::EncodeJob>(
      keyframe, timestamp, next_surface_id_++, picture,
      std::move(scoped_va_buffer));
}

void H264VaapiVideoEncoderDelegateTest::SetUp() {
  mock_vaapi_wrapper_ = base::MakeRefCounted<MockVaapiWrapper>();
  ASSERT_TRUE(mock_vaapi_wrapper_);

  encoder_ = std::make_unique<H264VaapiVideoEncoderDelegate>(
      mock_vaapi_wrapper_,
      base::BindRepeating(&H264VaapiVideoEncoderDelegateTest::OnError,
                          base::Unretained(this)));
  EXPECT_CALL(*this, OnError()).Times(0);
}

bool H264VaapiVideoEncoderDelegateTest::InitializeEncoder(
    uint8_t num_temporal_layers) {
  auto vea_config = kDefaultVEAConfig;
  vea_config.spatial_layers.resize(1u);
  auto& sl = vea_config.spatial_layers[0];
  sl.width = vea_config.input_visible_size.width();
  sl.height = vea_config.input_visible_size.height();
  sl.bitrate_bps = vea_config.bitrate.target_bps();
  sl.framerate = vea_config.initial_framerate.value_or(30);
  sl.max_qp = 30;
  sl.num_of_temporal_layers = num_temporal_layers;
  return encoder_->Initialize(vea_config, kDefaultVEADelegateConfig);
}

void H264VaapiVideoEncoderDelegateTest::EncodeFrame(bool force_keyframe) {
  auto encode_job = CreateEncodeJob(force_keyframe);
  ::testing::InSequence seq;

  EXPECT_CALL(*mock_vaapi_wrapper_,
              SubmitBuffer_Locked(MatchVABufferDescriptor(
                  VAEncSequenceParameterBufferType,
                  sizeof(VAEncSequenceParameterBufferH264))))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_vaapi_wrapper_,
              SubmitBuffer_Locked(MatchVABufferDescriptor(
                  VAEncPictureParameterBufferType,
                  sizeof(VAEncPictureParameterBufferH264))))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_vaapi_wrapper_, SubmitBuffer_Locked(MatchVABufferDescriptor(
                                        VAEncSliceParameterBufferType,
                                        sizeof(VAEncSliceParameterBufferH264))))
      .WillOnce(Return(true));

  // Misc Parameters.
  EXPECT_CALL(*mock_vaapi_wrapper_,
              SubmitBuffer_Locked(MatchVABufferDescriptorForMiscParam(
                  VAEncMiscParameterTypeRateControl,
                  sizeof(VAEncMiscParameterRateControl))))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_vaapi_wrapper_,
              SubmitBuffer_Locked(MatchVABufferDescriptorForMiscParam(
                  VAEncMiscParameterTypeFrameRate,
                  sizeof(VAEncMiscParameterFrameRate))))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_vaapi_wrapper_,
              SubmitBuffer_Locked(MatchVABufferDescriptorForMiscParam(
                  VAEncMiscParameterTypeHRD, sizeof(VAEncMiscParameterHRD))))
      .WillOnce(Return(true));
  // Packed slice header.
  EXPECT_CALL(*mock_vaapi_wrapper_,
              SubmitBuffer_Locked(MatchVABufferDescriptorForPackedHeader(
                  VAEncPackedHeaderSlice)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_vaapi_wrapper_,
              SubmitBuffer_Locked(MatchVABufferDescriptorForPackedHeaderData()))
      .WillOnce(Return(true));

  // Assume IDR frame is produced if and only if |force_keyframe| because IDR
  // frame period is long enough.
  if (force_keyframe) {
    // Packed SPS header.
    EXPECT_CALL(*mock_vaapi_wrapper_,
                SubmitBuffer_Locked(MatchVABufferDescriptorForPackedHeader(
                    VAEncPackedHeaderSequence)))
        .WillOnce(Return(true));
    EXPECT_CALL(
        *mock_vaapi_wrapper_,
        SubmitBuffer_Locked(MatchVABufferDescriptorForPackedHeaderData()))
        .WillOnce(Return(true));

    // Packed PPS header.
    EXPECT_CALL(*mock_vaapi_wrapper_,
                SubmitBuffer_Locked(MatchVABufferDescriptorForPackedHeader(
                    VAEncPackedHeaderPicture)))
        .WillOnce(Return(true));
    EXPECT_CALL(
        *mock_vaapi_wrapper_,
        SubmitBuffer_Locked(MatchVABufferDescriptorForPackedHeaderData()))
        .WillOnce(Return(true));
  }

  EXPECT_TRUE(encoder_->PrepareEncodeJob(*encode_job.get()));

  const H264Picture& pic =
      *reinterpret_cast<H264Picture*>(encode_job->picture().get());
  EXPECT_EQ(pic.type == H264SliceHeader::kISlice, pic.idr);
  EXPECT_EQ(pic.idr, force_keyframe);
  if (force_keyframe)
    num_encode_frames_ = 0;

  const int frame_num = pic.frame_num;
  constexpr size_t kDummyPayloadSize = 12345;
  const BitstreamBufferMetadata metadata =
      encoder_->GetMetadata(*encode_job.get(), kDummyPayloadSize);
  ASSERT_TRUE(metadata.h264.has_value());

  const uint8_t temporal_idx = metadata.h264->temporal_idx;
  ValidateTemporalLayerStructure(GetParam(), num_encode_frames_, frame_num,
                                 temporal_idx, pic.ref, previous_frame_num_);

  num_encode_frames_++;
}

TEST_F(H264VaapiVideoEncoderDelegateTest, Initialize) {
  auto vea_config = kDefaultVEAConfig;
  const auto vea_delegate_config = kDefaultVEADelegateConfig;
  EXPECT_TRUE(encoder_->Initialize(vea_config, vea_delegate_config));
  // Profile is unspecified, H264VaapiVideoEncoderDelegate will select the
  // default level, 4.0. 4.0 will be proper with |vea_config|'s values.
  ExpectLevel(H264SPS::kLevelIDC4p0);

  // Initialize with 4k size. The level will be adjusted to 5.1 by
  // H264VaapiVideoEncoderDelegate.
  vea_config.input_visible_size.SetSize(3840, 2160);
  EXPECT_TRUE(encoder_->Initialize(vea_config, vea_delegate_config));
  ExpectLevel(H264SPS::kLevelIDC5p1);
}

TEST_F(H264VaapiVideoEncoderDelegateTest, ChangeBitrateModeFails) {
  auto vea_config = kDefaultVEAConfig;
  const auto vea_delegate_config = kDefaultVEADelegateConfig;
  EXPECT_TRUE(encoder_->Initialize(vea_config, vea_delegate_config));

  const uint32_t new_bitrate_bps = kDefaultVEAConfig.bitrate.target_bps();
  VideoBitrateAllocation new_allocation =
      VideoBitrateAllocation(Bitrate::Mode::kVariable);
  new_allocation.SetBitrate(0, 0, new_bitrate_bps);
  EXPECT_TRUE(new_allocation.SetPeakBps(2u * new_bitrate_bps));

  ASSERT_FALSE(encoder_->UpdateRates(
      new_allocation, VideoEncodeAccelerator::kDefaultFramerate));
}

TEST_F(H264VaapiVideoEncoderDelegateTest, VariableBitrate_Initialize) {
  auto vea_config = kDefaultVEAConfig;
  const uint32_t bitrate_bps = vea_config.bitrate.target_bps();
  vea_config.bitrate = Bitrate::VariableBitrate(bitrate_bps, 2u * bitrate_bps);
  const auto vea_delegate_config = kDefaultVEADelegateConfig;

  ASSERT_TRUE(encoder_->Initialize(vea_config, vea_delegate_config));
}

TEST_P(H264VaapiVideoEncoderDelegateTest, EncodeTemporalLayerRequest) {
  const uint8_t num_temporal_layers = GetParam();
  const bool initialize_success = num_temporal_layers <= 3;
  // Initialize.
  EXPECT_EQ(initialize_success, InitializeEncoder(num_temporal_layers));
  if (!initialize_success)
    return;

  EXPECT_EQ(encoder_->GetCodedSize(), kDefaultVEAConfig.input_visible_size);
  EXPECT_EQ(encoder_->GetMaxNumOfRefFrames(),
            base::checked_cast<size_t>(num_temporal_layers - 1));
  EXPECT_EQ(encoder_->GetSVCLayerResolutions(),
            std::vector<gfx::Size>{kDefaultVEAConfig.input_visible_size});

  size_t kKeyFrameInterval = 10;
  for (size_t frame_num = 0; frame_num < 30; ++frame_num) {
    const bool force_keyframe = frame_num % kKeyFrameInterval == 0;
    EncodeFrame(force_keyframe);
  }
}

// We expect 4 to fail to initialize.
INSTANTIATE_TEST_SUITE_P(,
                         H264VaapiVideoEncoderDelegateTest,
                         ::testing::Values(2u, 3u, 4u));
}  // namespace media
