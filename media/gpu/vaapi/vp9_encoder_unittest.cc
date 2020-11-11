// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_encoder.h"

#include <memory>
#include <numeric>
#include <tuple>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/vaapi/vp9_rate_control.h"
#include "media/gpu/vaapi/vp9_temporal_layers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libvpx/source/libvpx/vp9/common/vp9_blockd.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

namespace media {
namespace {

constexpr size_t kDefaultMaxNumRefFrames = kVp9NumRefsPerFrame;

AcceleratedVideoEncoder::Config kDefaultAcceleratedVideoEncoderConfig{
    kDefaultMaxNumRefFrames,
    AcceleratedVideoEncoder::BitrateControl::kConstantBitrate};

VideoEncodeAccelerator::Config kDefaultVideoEncodeAcceleratorConfig(
    PIXEL_FORMAT_I420,
    gfx::Size(1280, 720),
    VP9PROFILE_PROFILE0,
    14000000 /* = maximum bitrate in bits per second for level 3.1 */,
    VideoEncodeAccelerator::kDefaultFramerate,
    base::nullopt /* gop_length */,
    base::nullopt /* h264 output level*/,
    false /* is_constrained_h264 */,
    VideoEncodeAccelerator::Config::StorageType::kShmem);

constexpr std::array<bool, kVp9NumRefsPerFrame> kRefFramesUsedForKeyFrame = {
    false, false, false};
constexpr std::array<bool, kVp9NumRefsPerFrame> kRefFramesUsedForInterFrame = {
    true, true, true};

void GetTemporalLayer(bool keyframe,
                      int index,
                      size_t num_temporal_layers,
                      std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used,
                      base::Optional<uint8_t>* temporal_layer_id) {
  switch (num_temporal_layers) {
    case 1:
      *ref_frames_used =
          keyframe ? kRefFramesUsedForKeyFrame : kRefFramesUsedForInterFrame;
      break;
    case 2:
      if (keyframe) {
        *temporal_layer_id = 0;
        *ref_frames_used = kRefFramesUsedForKeyFrame;
        return;
      }

      {
        // 2 temporal layers structure. See https://imgur.com/vBvHtdp.
        constexpr std::tuple<uint8_t, std::array<bool, kVp9NumRefsPerFrame>>
            kTwoTemporalLayersDescription[] = {
                {0, {true, false, false}}, {1, {true, false, false}},
                {0, {true, false, false}}, {1, {true, true, false}},
                {0, {true, false, false}}, {1, {true, true, false}},
                {0, {true, false, false}}, {1, {true, true, false}},
            };
        const auto& layer_info = kTwoTemporalLayersDescription
            [index % base::size(kTwoTemporalLayersDescription)];
        std::tie(*temporal_layer_id, *ref_frames_used) = layer_info;
      }
      break;
    case 3:
      if (keyframe) {
        *temporal_layer_id = 0u;
        *ref_frames_used = kRefFramesUsedForKeyFrame;
        return;
      }

      {
        // 3 temporal layers structure. See https://imgur.com/pURAGvp.
        constexpr std::tuple<uint8_t, std::array<bool, kVp9NumRefsPerFrame>>
            kThreeTemporalLayersDescription[] = {
                {0, {true, false, false}}, {2, {true, false, false}},
                {1, {true, false, false}}, {2, {true, true, false}},
                {0, {true, false, false}}, {2, {true, true, false}},
                {1, {true, true, false}},  {2, {true, true, false}},
            };
        const auto& layer_info = kThreeTemporalLayersDescription
            [index % base::size(kThreeTemporalLayersDescription)];
        std::tie(*temporal_layer_id, *ref_frames_used) = layer_info;
      }
      break;
  }
}

VideoBitrateAllocation GetDefaultVideoBitrateAllocation(
    size_t num_temporal_layers,
    uint32_t bitrate) {
  VideoBitrateAllocation bitrate_allocation;
  if (num_temporal_layers == 1u) {
    bitrate_allocation.SetBitrate(0, 0, bitrate);
    return bitrate_allocation;
  }

  LOG_ASSERT(num_temporal_layers <=
             VP9TemporalLayers::kMaxSupportedTemporalLayers);
  constexpr double kTemporalLayersBitrateScaleFactors
      [][VP9TemporalLayers::kMaxSupportedTemporalLayers] = {
          {0.50, 0.50, 0.00},  // For two temporal layers.
          {0.25, 0.25, 0.50},  // For three temporal layers.
      };

  for (size_t i = 0; i < num_temporal_layers; i++) {
    const double factor =
        kTemporalLayersBitrateScaleFactors[num_temporal_layers - 2][i];
    bitrate_allocation.SetBitrate(0 /* spatial_index */, i,
                                  base::checked_cast<int>(bitrate * factor));
  }
  return bitrate_allocation;
}

MATCHER_P4(MatchRtcConfigWithRates,
           size,
           bitrate_allocation,
           framerate,
           num_temporal_layers,
           "") {
  if (arg.target_bandwidth != bitrate_allocation.GetSumBps() / 1000)
    return false;

  if (arg.framerate != static_cast<double>(framerate))
    return false;

  int bitrate_sum = 0;
  for (size_t i = 0; i < num_temporal_layers; i++) {
    bitrate_sum += bitrate_allocation.GetBitrateBps(0, i);
    if (arg.layer_target_bitrate[i] != bitrate_sum / 1000)
      return false;
    if (arg.ts_rate_decimator[i] != (1 << (num_temporal_layers - i - 1)))
      return false;
  }

  return arg.width == size.width() && arg.height == size.height() &&
         base::checked_cast<size_t>(arg.ts_number_layers) ==
             num_temporal_layers &&
         arg.ss_number_layers == 1 && arg.scaling_factor_num[0] == 1 &&
         arg.scaling_factor_den[0] == 1;
}

MATCHER_P2(MatchFrameParam, frame_type, temporal_layer_id, "") {
  return arg.frame_type == frame_type &&
         (!temporal_layer_id || arg.temporal_layer_id == *temporal_layer_id);
}

class MockVP9Accelerator : public VP9Encoder::Accelerator {
 public:
  MockVP9Accelerator() = default;
  ~MockVP9Accelerator() override = default;
  MOCK_METHOD1(GetPicture,
               scoped_refptr<VP9Picture>(AcceleratedVideoEncoder::EncodeJob*));

  MOCK_METHOD5(SubmitFrameParameters,
               bool(AcceleratedVideoEncoder::EncodeJob*,
                    const VP9Encoder::EncodeParams&,
                    scoped_refptr<VP9Picture>,
                    const Vp9ReferenceFrameVector&,
                    const std::array<bool, kVp9NumRefsPerFrame>&));
};

class MockVP9RateControl : public VP9RateControl {
 public:
  MockVP9RateControl() = default;
  ~MockVP9RateControl() override = default;

  MOCK_METHOD1(UpdateRateControl, void(const libvpx::VP9RateControlRtcConfig&));
  MOCK_CONST_METHOD0(GetQP, int());
  MOCK_CONST_METHOD0(GetLoopfilterLevel, int());
  MOCK_METHOD1(ComputeQP, void(const libvpx::VP9FrameParamsQpRTC&));
  MOCK_METHOD1(PostEncodeUpdate, void(uint64_t));
};
}  // namespace

struct VP9EncoderTestParam;

class VP9EncoderTest : public ::testing::TestWithParam<VP9EncoderTestParam> {
 public:
  using BitrateControl = AcceleratedVideoEncoder::BitrateControl;

  VP9EncoderTest() = default;
  ~VP9EncoderTest() override = default;

  void SetUp() override;

 protected:
  void InitializeVP9Encoder(BitrateControl bitrate_control,
                            size_t num_temporal_layers);
  void EncodeSequence(bool is_keyframe);
  void EncodeConstantQuantizationParameterSequence(
      bool is_keyframe,
      base::Optional<std::array<bool, kVp9NumRefsPerFrame>>
          expected_ref_frames_used,
      base::Optional<uint8_t> expected_temporal_layer_id = base::nullopt);
  void UpdateRatesTest(BitrateControl bitrate_control,
                       size_t num_temporal_layers);

 private:
  std::unique_ptr<AcceleratedVideoEncoder::EncodeJob> CreateEncodeJob(
      bool keyframe);
  void UpdateRatesSequence(const VideoBitrateAllocation& bitrate_allocation,
                           uint32_t framerate,
                           BitrateControl bitrate_control,
                           size_t num_temporal_layers);

  std::unique_ptr<VP9Encoder> encoder_;
  MockVP9Accelerator* mock_accelerator_ = nullptr;
  MockVP9RateControl* mock_rate_ctrl_ = nullptr;
};

void VP9EncoderTest::SetUp() {
  auto mock_accelerator = std::make_unique<MockVP9Accelerator>();
  mock_accelerator_ = mock_accelerator.get();
  encoder_ = std::make_unique<VP9Encoder>(std::move(mock_accelerator));
}

std::unique_ptr<AcceleratedVideoEncoder::EncodeJob>
VP9EncoderTest::CreateEncodeJob(bool keyframe) {
  auto input_frame = VideoFrame::CreateFrame(
      kDefaultVideoEncodeAcceleratorConfig.input_format,
      kDefaultVideoEncodeAcceleratorConfig.input_visible_size,
      gfx::Rect(kDefaultVideoEncodeAcceleratorConfig.input_visible_size),
      kDefaultVideoEncodeAcceleratorConfig.input_visible_size,
      base::TimeDelta());
  LOG_ASSERT(input_frame) << " Failed to create VideoFrame";
  return std::make_unique<AcceleratedVideoEncoder::EncodeJob>(
      input_frame, keyframe, base::DoNothing());
}

void VP9EncoderTest::InitializeVP9Encoder(BitrateControl bitrate_control,
                                          size_t num_temporal_layers) {
  auto config = kDefaultVideoEncodeAcceleratorConfig;
  auto ave_config = kDefaultAcceleratedVideoEncoderConfig;
  ave_config.bitrate_control = bitrate_control;
  if (bitrate_control == BitrateControl::kConstantQuantizationParameter) {
    auto rate_ctrl = std::make_unique<MockVP9RateControl>();
    mock_rate_ctrl_ = rate_ctrl.get();
    encoder_->set_rate_ctrl_for_testing(std::move(rate_ctrl));

    VideoBitrateAllocation initial_bitrate_allocation;
    initial_bitrate_allocation.SetBitrate(
        0, 0, kDefaultVideoEncodeAcceleratorConfig.initial_bitrate);
    if (num_temporal_layers > 1u) {
      VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
      spatial_layer.width = config.input_visible_size.width();
      spatial_layer.height = config.input_visible_size.height();
      spatial_layer.bitrate_bps = config.initial_bitrate;
      spatial_layer.framerate = *config.initial_framerate;
      spatial_layer.max_qp = 30;
      spatial_layer.num_of_temporal_layers = num_temporal_layers;
      config.spatial_layers.push_back(spatial_layer);
    }

    EXPECT_CALL(
        *mock_rate_ctrl_,
        UpdateRateControl(MatchRtcConfigWithRates(
            kDefaultVideoEncodeAcceleratorConfig.input_visible_size,
            GetDefaultVideoBitrateAllocation(num_temporal_layers,
                                             config.initial_bitrate),
            VideoEncodeAccelerator::kDefaultFramerate, num_temporal_layers)))
        .Times(1)
        .WillOnce(Return());
  } else {
    // VP9Encoder doesn't support temporal layer encoding in
    // BitrateControl::kConstantQuantizationParameter.
    ASSERT_EQ(num_temporal_layers, 1u);
  }

  EXPECT_TRUE(encoder_->Initialize(config, ave_config));
  EXPECT_EQ(num_temporal_layers > 1u, !!encoder_->temporal_layers_);
}

void VP9EncoderTest::EncodeSequence(bool is_keyframe) {
  InSequence seq;
  auto encode_job = CreateEncodeJob(is_keyframe);
  scoped_refptr<VP9Picture> picture(new VP9Picture);
  EXPECT_CALL(*mock_accelerator_, GetPicture(encode_job.get()))
      .WillOnce(Invoke(
          [picture](AcceleratedVideoEncoder::EncodeJob*) { return picture; }));
  const auto& expected_ref_frames_used =
      is_keyframe ? kRefFramesUsedForKeyFrame : kRefFramesUsedForInterFrame;
  EXPECT_CALL(*mock_accelerator_,
              SubmitFrameParameters(
                  encode_job.get(), _, _, _,
                  ::testing::ElementsAreArray(expected_ref_frames_used)))
      .WillOnce(Return(true));
  EXPECT_TRUE(encoder_->PrepareEncodeJob(encode_job.get()));
  // TODO(hiroh): Test for encoder_->reference_frames_.
}

void VP9EncoderTest::EncodeConstantQuantizationParameterSequence(
    bool is_keyframe,
    base::Optional<std::array<bool, kVp9NumRefsPerFrame>>
        expected_ref_frames_used,
    base::Optional<uint8_t> expected_temporal_layer_id) {
  InSequence seq;
  auto encode_job = CreateEncodeJob(is_keyframe);
  scoped_refptr<VP9Picture> picture(new VP9Picture);
  EXPECT_CALL(*mock_accelerator_, GetPicture(encode_job.get()))
      .WillOnce(Invoke(
          [picture](AcceleratedVideoEncoder::EncodeJob*) { return picture; }));

  FRAME_TYPE libvpx_frame_type =
      is_keyframe ? FRAME_TYPE::KEY_FRAME : FRAME_TYPE::INTER_FRAME;
  EXPECT_CALL(
      *mock_rate_ctrl_,
      ComputeQP(MatchFrameParam(libvpx_frame_type, expected_temporal_layer_id)))
      .WillOnce(Return());
  constexpr int kDefaultQP = 34;
  constexpr int kDefaultLoopFilterLevel = 8;
  EXPECT_CALL(*mock_rate_ctrl_, GetQP()).WillOnce(Return(kDefaultQP));
  EXPECT_CALL(*mock_rate_ctrl_, GetLoopfilterLevel())
      .WillOnce(Return(kDefaultLoopFilterLevel));
  if (expected_ref_frames_used) {
    EXPECT_CALL(*mock_accelerator_,
                SubmitFrameParameters(
                    encode_job.get(), _, _, _,
                    ::testing::ElementsAreArray(*expected_ref_frames_used)))
        .WillOnce(Return(true));
  } else {
    EXPECT_CALL(*mock_accelerator_,
                SubmitFrameParameters(encode_job.get(), _, _, _, _))
        .WillOnce(Return(true));
  }
  EXPECT_TRUE(encoder_->PrepareEncodeJob(encode_job.get()));

  // TODO(hiroh): Test for encoder_->reference_frames_.

  constexpr size_t kDefaultEncodedFrameSize = 123456;
  // For BitrateControlUpdate sequence.
  EXPECT_CALL(*mock_rate_ctrl_, PostEncodeUpdate(kDefaultEncodedFrameSize))
      .WillOnce(Return());
  encoder_->BitrateControlUpdate(kDefaultEncodedFrameSize);
}

void VP9EncoderTest::UpdateRatesSequence(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    BitrateControl bitrate_control,
    size_t num_temporal_layers) {
  ASSERT_TRUE(encoder_->current_params_.bitrate_allocation !=
                  bitrate_allocation ||
              encoder_->current_params_.framerate != framerate);

  if (bitrate_control == BitrateControl::kConstantQuantizationParameter) {
    EXPECT_CALL(*mock_rate_ctrl_,
                UpdateRateControl(MatchRtcConfigWithRates(
                    encoder_->visible_size_, bitrate_allocation, framerate,
                    num_temporal_layers)))
        .Times(1)
        .WillOnce(Return());
  }

  EXPECT_TRUE(encoder_->UpdateRates(bitrate_allocation, framerate));
  EXPECT_EQ(encoder_->current_params_.bitrate_allocation, bitrate_allocation);
  EXPECT_EQ(encoder_->current_params_.framerate, framerate);
}

void VP9EncoderTest::UpdateRatesTest(BitrateControl bitrate_control,
                                     size_t num_temporal_layers) {
  ASSERT_TRUE(num_temporal_layers <=
              VP9TemporalLayers::kMaxSupportedTemporalLayers);
  const auto update_rates_and_encode =
      [this, bitrate_control, num_temporal_layers](
          bool is_keyframe, const VideoBitrateAllocation& bitrate_allocation,
          uint32_t framerate) {
        UpdateRatesSequence(bitrate_allocation, framerate, bitrate_control,
                            num_temporal_layers);
        if (bitrate_control == BitrateControl::kConstantQuantizationParameter) {
          EncodeConstantQuantizationParameterSequence(is_keyframe, {},
                                                      base::nullopt);
        } else {
          EncodeSequence(is_keyframe);
        }
      };

  const uint32_t kBitrate =
      kDefaultVideoEncodeAcceleratorConfig.initial_bitrate;
  const uint32_t kFramerate =
      *kDefaultVideoEncodeAcceleratorConfig.initial_framerate;
  // Call UpdateRates before Encode.
  update_rates_and_encode(
      true, GetDefaultVideoBitrateAllocation(num_temporal_layers, kBitrate / 2),
      kFramerate);
  // Bitrate change only.
  update_rates_and_encode(
      false, GetDefaultVideoBitrateAllocation(num_temporal_layers, kBitrate),
      kFramerate);
  // Framerate change only.
  update_rates_and_encode(
      false, GetDefaultVideoBitrateAllocation(num_temporal_layers, kBitrate),
      kFramerate + 2);
  // Bitrate + Frame changes.
  update_rates_and_encode(
      false,
      GetDefaultVideoBitrateAllocation(num_temporal_layers, kBitrate * 3 / 4),
      kFramerate - 5);
}

struct VP9EncoderTestParam {
  VP9EncoderTest::BitrateControl bitrate_control;
  size_t num_temporal_layers;
} kTestCasesForVP9EncoderTest[] = {
    {VP9EncoderTest::BitrateControl::kConstantBitrate, 1u},
    {VP9EncoderTest::BitrateControl::kConstantQuantizationParameter, 1u},
    {VP9EncoderTest::BitrateControl::kConstantQuantizationParameter,
     VP9TemporalLayers::kMinSupportedTemporalLayers},
    {VP9EncoderTest::BitrateControl::kConstantQuantizationParameter,
     VP9TemporalLayers::kMaxSupportedTemporalLayers},
};

TEST_P(VP9EncoderTest, Initialize) {
  InitializeVP9Encoder(GetParam().bitrate_control,
                       GetParam().num_temporal_layers);
}

TEST_P(VP9EncoderTest, EncodeWithoutSoftwareBitrateControl) {
  const auto& bitrate_control = GetParam().bitrate_control;
  if (bitrate_control != BitrateControl::kConstantBitrate)
    GTEST_SKIP() << "Test only for without software bitrate control";

  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9Encoder(bitrate_control, num_temporal_layers);

  EncodeSequence(true);
  EncodeSequence(false);
}

TEST_P(VP9EncoderTest, EncodeWithSoftwareBitrateControl) {
  const auto& bitrate_control = GetParam().bitrate_control;
  if (bitrate_control != BitrateControl::kConstantQuantizationParameter)
    GTEST_SKIP() << "Test only for with software bitrate control";

  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9Encoder(bitrate_control, num_temporal_layers);

  constexpr size_t kEncodeFrames = 20;
  for (size_t i = 0; i < kEncodeFrames; i++) {
    const bool is_keyframe = i == 0;
    std::array<bool, kVp9NumRefsPerFrame> ref_frames_used;
    base::Optional<uint8_t> temporal_layer_id;
    GetTemporalLayer(is_keyframe, i, num_temporal_layers, &ref_frames_used,
                     &temporal_layer_id);
    EncodeConstantQuantizationParameterSequence(is_keyframe, ref_frames_used,
                                                temporal_layer_id);
  }
}

TEST_P(VP9EncoderTest, ForceKeyFrameWithoutSoftwareBitrateControl) {
  const auto& bitrate_control = GetParam().bitrate_control;
  if (bitrate_control != BitrateControl::kConstantBitrate)
    GTEST_SKIP() << "Test only for with software bitrate control";

  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9Encoder(bitrate_control, num_temporal_layers);

  EncodeSequence(true /* is_keyframe */);
  EncodeSequence(false /* is_keyframe */);
  EncodeSequence(true /* is_keyframe */);
  EncodeSequence(false /* is_keyframe */);
}

TEST_P(VP9EncoderTest, ForceKeyFrameWithSoftwareBitrateControl) {
  const auto& bitrate_control = GetParam().bitrate_control;
  if (bitrate_control != BitrateControl::kConstantQuantizationParameter)
    GTEST_SKIP() << "Test only for with software bitrate control";

  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9Encoder(bitrate_control, num_temporal_layers);
  constexpr size_t kNumKeyFrames = 3;
  constexpr size_t kKeyFrameInterval = 20;
  for (size_t j = 0; j < kNumKeyFrames; j++) {
    for (size_t i = 0; i < kKeyFrameInterval; i++) {
      const bool is_keyframe = i == 0;
      std::array<bool, kVp9NumRefsPerFrame> ref_frames_used;
      base::Optional<uint8_t> temporal_layer_id;
      GetTemporalLayer(is_keyframe, i, num_temporal_layers, &ref_frames_used,
                       &temporal_layer_id);
      EncodeConstantQuantizationParameterSequence(is_keyframe, ref_frames_used,
                                                  temporal_layer_id);
    }
  }
}

TEST_P(VP9EncoderTest, UpdateRates) {
  const auto& bitrate_control = GetParam().bitrate_control;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9Encoder(bitrate_control, num_temporal_layers);
  UpdateRatesTest(bitrate_control, num_temporal_layers);
}

INSTANTIATE_TEST_SUITE_P(,
                         VP9EncoderTest,
                         ::testing::ValuesIn(kTestCasesForVP9EncoderTest));
}  // namespace media
