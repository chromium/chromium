// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/h264_vaapi_video_encoder_delegate.h"

#include <memory>

#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace media {
namespace {

constexpr gfx::Size kDefaultVisibleSize = gfx::Size(1280, 720);
constexpr VideoEncodeAccelerator::Config::ContentType kDefaultContentType =
    VideoEncodeAccelerator::Config::ContentType::kCamera;
// Limit max delay for intra frame with HRD buffer size (500ms-1s for camera
// video, 1s-10s for desktop sharing).
constexpr base::TimeDelta kHRDBufferDelayCamera = base::Milliseconds(1000);
constexpr base::TimeDelta kHRDBufferDelayDisplay = base::Milliseconds(3000);
constexpr uint8_t kMinQP = 1;
constexpr uint8_t kScreenMinQP = 10;
constexpr uint8_t kMaxQP = 42;
constexpr size_t kSupportedNumTemporalLayersByController = 1;

VaapiVideoEncoderDelegate::Config kDefaultVEADelegateConfig{
    .max_num_ref_frames = 4,
};

VideoEncodeAccelerator::Config DefaultVEAConfig() {
  VideoEncodeAccelerator::Config vea_config(
      PIXEL_FORMAT_I420, kDefaultVisibleSize, H264PROFILE_BASELINE,
      /* = maximum bitrate in bits per second for level 3.1 */
      Bitrate::ConstantBitrate(14000000u),
      VideoEncodeAccelerator::kDefaultFramerate,
      VideoEncodeAccelerator::Config::StorageType::kShmem, kDefaultContentType);

  return vea_config;
}

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

MATCHER_P5(MatchRtcConfigWithRates,
           bitrate_allocation,
           framerate,
           visible_size,
           num_temporal_layers,
           content_type,
           "") {
  uint32_t bitrate_sum = 0;
  for (size_t tid = 0; tid < num_temporal_layers; ++tid) {
    bitrate_sum += bitrate_allocation.GetBitrateBps(0u, tid);
    const auto layer_setting = arg.layer_settings[tid];
    if (layer_setting.avg_bitrate != bitrate_sum) {
      return false;
    }
    if (bitrate_allocation.GetMode() == Bitrate::Mode::kConstant) {
      if (layer_setting.peak_bitrate != bitrate_sum) {
        return false;
      }
    } else {
      if (layer_setting.peak_bitrate !=
          static_cast<uint32_t>(bitrate_sum * 3 / 2)) {
        return false;
      }
    }
    base::TimeDelta buffer_delay;
    if (content_type == VideoEncodeAccelerator::Config::ContentType::kDisplay) {
      buffer_delay = kHRDBufferDelayDisplay;
      if (layer_setting.min_qp != kScreenMinQP) {
        return false;
      }
    } else {
      buffer_delay = kHRDBufferDelayCamera;
      if (layer_setting.min_qp != kMinQP) {
        return false;
      }
    }
    if (layer_setting.max_qp != kMaxQP) {
      return false;
    }
    base::CheckedNumeric<size_t> buffer_size(layer_setting.avg_bitrate);
    buffer_size *= buffer_delay.InMilliseconds();
    buffer_size /= base::Seconds(8).InMilliseconds();

    if (layer_setting.hrd_buffer_size != buffer_size.ValueOrDie()) {
      return false;
    }
    auto layer_framerate =
        static_cast<float>(framerate / (1u << (num_temporal_layers - tid - 1)));
    if (layer_setting.frame_rate != layer_framerate) {
      return false;
    }
  }
  return arg.frame_size == visible_size && arg.frame_rate_max == framerate &&
         arg.num_temporal_layers == num_temporal_layers &&
         arg.content_type == content_type;
}

MATCHER_P3(MatchFrameParam, keyframe, temporal_layer_id, timestamp, "") {
  return arg.keyframe == keyframe &&
         arg.temporal_layer_id == static_cast<int>(temporal_layer_id) &&
         arg.timestamp == timestamp;
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
  MockVaapiWrapper()
      : VaapiWrapper(VADisplayStateHandle(), kEncodeConstantBitrate) {}

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

class MockH264RateControl : public H264RateControlWrapper {
 public:
  MockH264RateControl() = default;
  ~MockH264RateControl() override = default;

  MOCK_METHOD1(UpdateRateControl, void(const H264RateControlConfigRTC&));
  MOCK_METHOD1(ComputeQP,
               H264RateCtrlRTC::FrameDropDecision(const H264FrameParamsRTC&));
  MOCK_CONST_METHOD0(GetQP, int());
  MOCK_METHOD2(PostEncodeUpdate, void(uint64_t, const H264FrameParamsRTC&));
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
  void InitializeEncoderWithSWBitrateController();
  void EncodeFrame(bool force_keyframe,
                   base::TimeDelta timestamp,
                   uint8_t num_temporal_layers);
  void UpdateRatesAndEncode(bool force_keyframe,
                            uint32_t bitrate,
                            uint32_t framerate);

 protected:
  std::unique_ptr<H264VaapiVideoEncoderDelegate> encoder_;
  raw_ptr<MockH264RateControl> mock_rate_ctrl_ = nullptr;

 private:
  std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob> CreateEncodeJob(
      bool keyframe,
      base::TimeDelta timestamp);

  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  unsigned int next_surface_id_ = 0;
  size_t num_encode_frames_ = 0;
  int previous_frame_num_ = 0;
};

std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob>
H264VaapiVideoEncoderDelegateTest::CreateEncodeJob(bool keyframe,
                                                   base::TimeDelta timestamp) {
  scoped_refptr<H264Picture> picture(
      new VaapiH264Picture(std::make_unique<VASurfaceHandle>(
          next_surface_id_++, base::DoNothing())));

  constexpr VABufferID kDummyVABufferID = 12;
  auto scoped_va_buffer = ScopedVABuffer::CreateForTesting(
      kDummyVABufferID, VAEncCodedBufferType,
      DefaultVEAConfig().input_visible_size.GetArea());

  return std::make_unique<VaapiVideoEncoderDelegate::EncodeJob>(
      keyframe, timestamp, /*spatial_index=*/0u, /*end_of_picture=*/true,
      next_surface_id_++, picture, std::move(scoped_va_buffer));
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
  auto vea_config = DefaultVEAConfig();
  vea_config.spatial_layers.resize(1u);
  auto& sl = vea_config.spatial_layers[0];
  sl.width = vea_config.input_visible_size.width();
  sl.height = vea_config.input_visible_size.height();
  sl.bitrate_bps = vea_config.bitrate.target_bps();
  sl.framerate = vea_config.framerate;
  sl.max_qp = 30;
  sl.num_of_temporal_layers = num_temporal_layers;
  return encoder_->Initialize(vea_config, kDefaultVEADelegateConfig);
}

void H264VaapiVideoEncoderDelegateTest::
    InitializeEncoderWithSWBitrateController() {
  auto rate_ctrl = std::make_unique<MockH264RateControl>();
  mock_rate_ctrl_ = rate_ctrl.get();
  encoder_->set_rate_ctrl_for_testing(std::move(rate_ctrl));

  auto vea_config = DefaultVEAConfig();
  auto initial_bitrate_allocation =
      AllocateBitrateForDefaultEncoding(vea_config);

  EXPECT_CALL(
      *mock_rate_ctrl_,
      UpdateRateControl(MatchRtcConfigWithRates(
          initial_bitrate_allocation, vea_config.framerate, kDefaultVisibleSize,
          kSupportedNumTemporalLayersByController, kDefaultContentType)));
  EXPECT_TRUE(InitializeEncoder(kSupportedNumTemporalLayersByController));
}

void H264VaapiVideoEncoderDelegateTest::EncodeFrame(
    bool force_keyframe,
    base::TimeDelta timestamp,
    uint8_t num_temporal_layers) {
  auto encode_job = CreateEncodeJob(force_keyframe, timestamp);
  ::testing::InSequence seq;

  if (mock_rate_ctrl_) {
    EXPECT_CALL(*mock_rate_ctrl_,
                ComputeQP(MatchFrameParam(
                    force_keyframe, kSupportedNumTemporalLayersByController - 1,
                    encode_job->timestamp())))
        .WillOnce(Return(H264RateCtrlRTC::FrameDropDecision::kOk));
    constexpr int kDefaultQP = 34;
    EXPECT_CALL(*mock_rate_ctrl_, GetQP()).WillOnce(Return(kDefaultQP));
  }

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

  if (!mock_rate_ctrl_) {
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
  }
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

  EXPECT_EQ(encoder_->PrepareEncodeJob(*encode_job.get()),
            VaapiVideoEncoderDelegate::PrepareEncodeJobResult::kSuccess);

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
  EXPECT_EQ(metadata.timestamp, encode_job->timestamp());
  if (num_temporal_layers > 1u) {
    ASSERT_TRUE(metadata.h264.has_value());
    const uint8_t temporal_idx = metadata.h264->temporal_idx;
    ValidateTemporalLayerStructure(GetParam(), num_encode_frames_, frame_num,
                                   temporal_idx, pic.ref, previous_frame_num_);
  }
  num_encode_frames_++;

  if (mock_rate_ctrl_) {
    EXPECT_CALL(*mock_rate_ctrl_,
                PostEncodeUpdate(
                    kDummyPayloadSize,
                    MatchFrameParam(force_keyframe,
                                    kSupportedNumTemporalLayersByController - 1,
                                    encode_job->timestamp())))
        .WillOnce(Return());
    encoder_->BitrateControlUpdate(metadata);
  }
}

void H264VaapiVideoEncoderDelegateTest::UpdateRatesAndEncode(
    bool force_keyframe,
    uint32_t bitrate,
    uint32_t framerate) {
  auto vea_config = DefaultVEAConfig();
  vea_config.framerate = framerate;
  vea_config.bitrate = media::Bitrate::ConstantBitrate(bitrate);
  auto bitrate_allocation = AllocateBitrateForDefaultEncoding(vea_config);

  ASSERT_TRUE(encoder_->curr_params_.bitrate_allocation != bitrate_allocation ||
              encoder_->curr_params_.framerate != framerate);
  EXPECT_CALL(
      *mock_rate_ctrl_,
      UpdateRateControl(MatchRtcConfigWithRates(
          bitrate_allocation, framerate, kDefaultVisibleSize,
          kSupportedNumTemporalLayersByController, kDefaultContentType)));
  EXPECT_TRUE(encoder_->UpdateRates(bitrate_allocation, framerate));
  EXPECT_EQ(encoder_->curr_params_.bitrate_allocation, bitrate_allocation);
  EXPECT_EQ(encoder_->curr_params_.framerate, framerate);
  base::TimeDelta timestamp = base::Milliseconds(1);
  EncodeFrame(force_keyframe, timestamp,
              kSupportedNumTemporalLayersByController);
}

TEST_F(H264VaapiVideoEncoderDelegateTest, Initialize) {
  auto vea_config = DefaultVEAConfig();
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
  auto vea_config = DefaultVEAConfig();
  const auto vea_delegate_config = kDefaultVEADelegateConfig;
  EXPECT_TRUE(encoder_->Initialize(vea_config, vea_delegate_config));

  const uint32_t new_bitrate_bps = DefaultVEAConfig().bitrate.target_bps();
  VideoBitrateAllocation new_allocation =
      VideoBitrateAllocation(Bitrate::Mode::kVariable);
  new_allocation.SetBitrate(0, 0, new_bitrate_bps);
  EXPECT_TRUE(new_allocation.SetPeakBps(2u * new_bitrate_bps));

  ASSERT_FALSE(encoder_->UpdateRates(
      new_allocation, VideoEncodeAccelerator::kDefaultFramerate));
}

TEST_F(H264VaapiVideoEncoderDelegateTest, VariableBitrate_Initialize) {
  auto vea_config = DefaultVEAConfig();
  const uint32_t bitrate_bps = vea_config.bitrate.target_bps();
  vea_config.bitrate = Bitrate::VariableBitrate(bitrate_bps, 2u * bitrate_bps);
  const auto vea_delegate_config = kDefaultVEADelegateConfig;

  ASSERT_TRUE(encoder_->Initialize(vea_config, vea_delegate_config));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(H264VaapiVideoEncoderDelegateTest, InitializeWithSWBitrateController) {
  base::test::ScopedFeatureList scoped_feature_list(
      media::kVaapiH264SWBitrateController);
  InitializeEncoderWithSWBitrateController();
}

TEST_F(H264VaapiVideoEncoderDelegateTest, EncodeWithSWBitrateController) {
  base::test::ScopedFeatureList scoped_feature_list(
      media::kVaapiH264SWBitrateController);
  InitializeEncoderWithSWBitrateController();
  size_t kKeyFrameInterval = 10;
  for (size_t frame_num = 0; frame_num < 30; ++frame_num) {
    const bool force_keyframe = frame_num % kKeyFrameInterval == 0;
    base::TimeDelta timestamp = base::Milliseconds(frame_num);
    EncodeFrame(force_keyframe, timestamp,
                kSupportedNumTemporalLayersByController);
  }
}

TEST_F(H264VaapiVideoEncoderDelegateTest, UpdateRates) {
  base::test::ScopedFeatureList scoped_feature_list(
      media::kVaapiH264SWBitrateController);
  InitializeEncoderWithSWBitrateController();
  const uint32_t kBitrate = DefaultVEAConfig().bitrate.target_bps();
  const uint32_t kFramerate = DefaultVEAConfig().framerate;
  // Call UpdateRates before Encode.
  UpdateRatesAndEncode(true, kBitrate / 2, kFramerate);
  // Bitrate change only.
  UpdateRatesAndEncode(false, kBitrate, kFramerate);
  // Framerate change only.
  UpdateRatesAndEncode(false, kBitrate, kFramerate + 2);
  // Bitrate + Frame changes.
  UpdateRatesAndEncode(false, kBitrate * 3 / 4, kFramerate - 5);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_P(H264VaapiVideoEncoderDelegateTest, EncodeTemporalLayerRequest) {
  const uint8_t num_temporal_layers = GetParam();
  const bool initialize_success = num_temporal_layers <= 3;
  // Initialize.
  EXPECT_EQ(initialize_success, InitializeEncoder(num_temporal_layers));
  if (!initialize_success)
    return;

  EXPECT_EQ(encoder_->GetCodedSize(), DefaultVEAConfig().input_visible_size);
  EXPECT_EQ(encoder_->GetMaxNumOfRefFrames(),
            base::checked_cast<size_t>(num_temporal_layers - 1));
  EXPECT_EQ(encoder_->GetSVCLayerResolutions(),
            std::vector<gfx::Size>{DefaultVEAConfig().input_visible_size});

  size_t kKeyFrameInterval = 10;
  for (size_t frame_num = 0; frame_num < 30; ++frame_num) {
    const bool force_keyframe = frame_num % kKeyFrameInterval == 0;
    base::TimeDelta timestamp = base::Milliseconds(frame_num);
    EncodeFrame(force_keyframe, timestamp, num_temporal_layers);
  }
}

// We expect 4 to fail to initialize.
INSTANTIATE_TEST_SUITE_P(,
                         H264VaapiVideoEncoderDelegateTest,
                         ::testing::Values(2u, 3u, 4u));
}  // namespace media
