// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_vaapi_video_encoder_delegate.h"

#include <memory>
#include <numeric>
#include <tuple>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vaapi/vp9_rate_control.h"
#include "media/gpu/vaapi/vp9_svc_layers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/libvpx/source/libvpx/vp9/common/vp9_blockd.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

namespace media {
namespace {

constexpr size_t kDefaultMaxNumRefFrames = kVp9NumRefsPerFrame;

constexpr double kSpatialLayersBitrateScaleFactors[][3] = {
    {1.00, 0.00, 0.00},  // For one spatial layer.
    {0.30, 0.70, 0.00},  // For two spatial layers.
    {0.07, 0.23, 0.70},  // For three spatial layers.
};
constexpr int kSpatialLayersResolutionScaleDenom[][3] = {
    {1, 0, 0},  // For one spatial layer.
    {2, 1, 0},  // For two spatial layers.
    {4, 2, 1},  // For three spatial layers.
};
constexpr double kTemporalLayersBitrateScaleFactors[][3] = {
    {1.00, 0.00, 0.00},  // For one temporal layer.
    {0.50, 0.50, 0.00},  // For two temporal layers.
    {0.25, 0.25, 0.50},  // For three temporal layers.
};

VaapiVideoEncoderDelegate::Config kDefaultVaapiVideoEncoderDelegateConfig{
    kDefaultMaxNumRefFrames,
    VaapiVideoEncoderDelegate::BitrateControl::kConstantQuantizationParameter};

VideoEncodeAccelerator::Config kDefaultVideoEncodeAcceleratorConfig(
    PIXEL_FORMAT_I420,
    gfx::Size(1280, 720),
    VP9PROFILE_PROFILE0,
    14000000 /* = maximum bitrate in bits per second for level 3.1 */,
    VideoEncodeAccelerator::kDefaultFramerate,
    absl::nullopt /* gop_length */,
    absl::nullopt /* h264 output level*/,
    false /* is_constrained_h264 */,
    VideoEncodeAccelerator::Config::StorageType::kShmem);

constexpr std::array<bool, kVp9NumRefsPerFrame> kRefFramesUsedForKeyFrame = {
    false, false, false};
constexpr std::array<bool, kVp9NumRefsPerFrame> kRefFramesUsedForInterFrame = {
    true, true, true};

void GetTemporalLayer(bool keyframe,
                      int frame_num,
                      size_t num_spatial_layers,
                      size_t num_temporal_layers,
                      std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used,
                      absl::optional<uint8_t>* temporal_layer_id) {
  switch (num_temporal_layers) {
    case 1:
      if (num_spatial_layers > 1) {
        // K-SVC stream.
        if (keyframe) {
          *ref_frames_used = keyframe ? kRefFramesUsedForKeyFrame
                                      : kRefFramesUsedForInterFrame;
          return;
        }

        *temporal_layer_id = 0;
        {
          constexpr std::tuple<uint8_t, std::array<bool, kVp9NumRefsPerFrame>>
              kOneTemporalLayersDescription[] = {{0, {true, false, false}}};
          const auto& layer_info = kOneTemporalLayersDescription
              [frame_num % base::size(kOneTemporalLayersDescription)];
          std::tie(*temporal_layer_id, *ref_frames_used) = layer_info;
        }
      } else {
        // Simple stream.
        *ref_frames_used =
            keyframe ? kRefFramesUsedForKeyFrame : kRefFramesUsedForInterFrame;
      }
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
            [frame_num % base::size(kTwoTemporalLayersDescription)];
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
            [frame_num % base::size(kThreeTemporalLayersDescription)];
        std::tie(*temporal_layer_id, *ref_frames_used) = layer_info;
      }
      break;
  }
}

VideoBitrateAllocation GetDefaultVideoBitrateAllocation(
    size_t num_spatial_layers,
    size_t num_temporal_layers,
    uint32_t bitrate) {
  VideoBitrateAllocation bitrate_allocation;
  DCHECK_LE(num_spatial_layers, 3u);
  DCHECK_LE(num_temporal_layers, 3u);
  if (num_spatial_layers == 1u && num_temporal_layers == 1u) {
    bitrate_allocation.SetBitrate(0, 0, bitrate);
    return bitrate_allocation;
  }

  for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
    const double bitrate_factor =
        kSpatialLayersBitrateScaleFactors[num_spatial_layers - 1][sid];
    uint32_t sl_bitrate = bitrate * bitrate_factor;

    for (size_t tl_idx = 0; tl_idx < num_temporal_layers; ++tl_idx) {
      const double tl_factor =
          kTemporalLayersBitrateScaleFactors[num_temporal_layers - 1][tl_idx];
      bitrate_allocation.SetBitrate(
          sid, tl_idx, base::checked_cast<int>(sl_bitrate * tl_factor));
    }
  }
  return bitrate_allocation;
}

MATCHER_P5(MatchRtcConfigWithRates,
           size,
           bitrate_allocation,
           framerate,
           num_spatial_layers,
           num_temporal_layers,
           "") {
  if (arg.target_bandwidth != bitrate_allocation.GetSumBps() / 1000)
    return false;

  if (arg.framerate != static_cast<double>(framerate))
    return false;

  for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
    int bitrate_sum = 0;
    for (size_t tid = 0; tid < num_temporal_layers; tid++) {
      size_t idx = sid * num_temporal_layers + tid;
      bitrate_sum += bitrate_allocation.GetBitrateBps(sid, tid);
      if (arg.layer_target_bitrate[idx] != bitrate_sum / 1000)
        return false;
      if (arg.ts_rate_decimator[tid] != (1 << (num_temporal_layers - tid - 1)))
        return false;
    }

    if (arg.scaling_factor_num[sid] != 1 ||
        arg.scaling_factor_den[sid] !=
            kSpatialLayersResolutionScaleDenom[num_spatial_layers - 1][sid]) {
      return false;
    }
  }

  return arg.width == size.width() && arg.height == size.height() &&
         base::checked_cast<size_t>(arg.ss_number_layers) ==
             num_spatial_layers &&
         base::checked_cast<size_t>(arg.ts_number_layers) ==
             num_temporal_layers;
}

MATCHER_P3(MatchFrameParam,
           frame_type,
           temporal_layer_id,
           spatial_layer_id,
           "") {
  return arg.frame_type == frame_type &&
         (!temporal_layer_id || arg.temporal_layer_id == *temporal_layer_id) &&
         (!spatial_layer_id || arg.spatial_layer_id == *spatial_layer_id);
}

class MockVaapiWrapper : public VaapiWrapper {
 public:
  MockVaapiWrapper() : VaapiWrapper(kEncodeConstantQuantizationParameter) {}

 protected:
  ~MockVaapiWrapper() override = default;
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

struct VP9VaapiVideoEncoderDelegateTestParam;

class VP9VaapiVideoEncoderDelegateTest
    : public ::testing::TestWithParam<VP9VaapiVideoEncoderDelegateTestParam> {
 public:
  using BitrateControl = VaapiVideoEncoderDelegate::BitrateControl;

  VP9VaapiVideoEncoderDelegateTest() = default;
  ~VP9VaapiVideoEncoderDelegateTest() override = default;

  void SetUp() override;

  MOCK_METHOD0(OnError, void());

 protected:
  void InitializeVP9VaapiVideoEncoderDelegate(BitrateControl bitrate_control,
                                              size_t num_spatial_layers,
                                              size_t num_temporal_layers);
  void EncodeConstantQuantizationParameterSequence(
      bool is_keyframe,
      size_t num_spatial_layers,
      absl::optional<std::array<bool, kVp9NumRefsPerFrame>>
          expected_ref_frames_used,
      absl::optional<uint8_t> expected_temporal_layer_id = absl::nullopt,
      absl::optional<uint8_t> expected_spatial_layer_id = absl::nullopt);
  void UpdateRatesTest(BitrateControl bitrate_control,
                       size_t num_spatial_layers,
                       size_t num_temporal_layers);

 private:
  std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob> CreateEncodeJob(
      bool keyframe,
      const scoped_refptr<VASurface>& va_surface,
      const scoped_refptr<VP9Picture>& picture);
  void UpdateRatesSequence(const VideoBitrateAllocation& bitrate_allocation,
                           uint32_t framerate,
                           BitrateControl bitrate_control,
                           size_t num_spatial_layers,
                           size_t num_temporal_layers);

  std::unique_ptr<VP9VaapiVideoEncoderDelegate> encoder_;
  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  MockVP9RateControl* mock_rate_ctrl_ = nullptr;
};

void VP9VaapiVideoEncoderDelegateTest::SetUp() {
  mock_vaapi_wrapper_ = base::MakeRefCounted<MockVaapiWrapper>();
  ASSERT_TRUE(mock_vaapi_wrapper_);

  encoder_ = std::make_unique<VP9VaapiVideoEncoderDelegate>(
      mock_vaapi_wrapper_,
      base::BindRepeating(&VP9VaapiVideoEncoderDelegateTest::OnError,
                          base::Unretained(this)));
  EXPECT_CALL(*this, OnError()).Times(0);
}

std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob>
VP9VaapiVideoEncoderDelegateTest::CreateEncodeJob(
    bool keyframe,
    const scoped_refptr<VASurface>& va_surface,
    const scoped_refptr<VP9Picture>& picture) {
  auto input_frame = VideoFrame::CreateFrame(
      kDefaultVideoEncodeAcceleratorConfig.input_format,
      kDefaultVideoEncodeAcceleratorConfig.input_visible_size,
      gfx::Rect(kDefaultVideoEncodeAcceleratorConfig.input_visible_size),
      kDefaultVideoEncodeAcceleratorConfig.input_visible_size,
      base::TimeDelta());
  LOG_ASSERT(input_frame) << " Failed to create VideoFrame";

  constexpr VABufferID kDummyVABufferID = 12;
  auto scoped_va_buffer = ScopedVABuffer::CreateForTesting(
      kDummyVABufferID, VAEncCodedBufferType,
      kDefaultVideoEncodeAcceleratorConfig.input_visible_size.GetArea());

  return std::make_unique<VaapiVideoEncoderDelegate::EncodeJob>(
      input_frame, keyframe, base::DoNothing(), va_surface, picture,
      std::move(scoped_va_buffer));
}

void VP9VaapiVideoEncoderDelegateTest::InitializeVP9VaapiVideoEncoderDelegate(
    BitrateControl bitrate_control,
    size_t num_spatial_layers,
    size_t num_temporal_layers) {
  auto config = kDefaultVideoEncodeAcceleratorConfig;
  auto ave_config = kDefaultVaapiVideoEncoderDelegateConfig;
  ave_config.bitrate_control = bitrate_control;
  ASSERT_EQ(bitrate_control, BitrateControl::kConstantQuantizationParameter);

  auto rate_ctrl = std::make_unique<MockVP9RateControl>();
  mock_rate_ctrl_ = rate_ctrl.get();
  encoder_->set_rate_ctrl_for_testing(std::move(rate_ctrl));

  VideoBitrateAllocation initial_bitrate_allocation;
  initial_bitrate_allocation.SetBitrate(
      0, 0, kDefaultVideoEncodeAcceleratorConfig.initial_bitrate);
  if (num_spatial_layers > 1u || num_temporal_layers > 1u) {
    DCHECK_GT(num_spatial_layers, 0u);
    for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
      const double bitrate_factor =
          kSpatialLayersBitrateScaleFactors[num_spatial_layers - 1][sid];
      const double resolution_denom =
          kSpatialLayersResolutionScaleDenom[num_spatial_layers - 1][sid];
      VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
      spatial_layer.width =
          config.input_visible_size.width() / resolution_denom;
      spatial_layer.height =
          config.input_visible_size.height() / resolution_denom;
      spatial_layer.bitrate_bps = config.initial_bitrate * bitrate_factor;
      spatial_layer.framerate = *config.initial_framerate;
      spatial_layer.num_of_temporal_layers = num_temporal_layers;
      spatial_layer.max_qp = 30u;
      config.spatial_layers.push_back(spatial_layer);
    }
  }

  EXPECT_CALL(
      *mock_rate_ctrl_,
      UpdateRateControl(MatchRtcConfigWithRates(
          kDefaultVideoEncodeAcceleratorConfig.input_visible_size,
          GetDefaultVideoBitrateAllocation(
              num_spatial_layers, num_temporal_layers, config.initial_bitrate),
          VideoEncodeAccelerator::kDefaultFramerate, num_spatial_layers,
          num_temporal_layers)))
      .Times(1)
      .WillOnce(Return());

  EXPECT_TRUE(encoder_->Initialize(config, ave_config));
  EXPECT_EQ(num_temporal_layers > 1u || num_spatial_layers > 1u,
            !!encoder_->svc_layers_);
}

void VP9VaapiVideoEncoderDelegateTest::
    EncodeConstantQuantizationParameterSequence(
        bool is_keyframe,
        size_t num_spatial_layers,
        absl::optional<std::array<bool, kVp9NumRefsPerFrame>>
            expected_ref_frames_used,
        absl::optional<uint8_t> expected_temporal_layer_id,
        absl::optional<uint8_t> expected_spatial_layer_id) {
  InSequence seq;

  constexpr VASurfaceID kDummyVASurfaceID = 123;
  const double resolution_denom =
      kSpatialLayersResolutionScaleDenom[num_spatial_layers - 1]
                                        [expected_spatial_layer_id.value_or(0)];
  gfx::Size layer_size = gfx::Size(
      kDefaultVideoEncodeAcceleratorConfig.input_visible_size.width() /
          resolution_denom,
      kDefaultVideoEncodeAcceleratorConfig.input_visible_size.height() /
          resolution_denom);
  auto va_surface = base::MakeRefCounted<VASurface>(
      kDummyVASurfaceID, layer_size, VA_RT_FORMAT_YUV420, base::DoNothing());
  scoped_refptr<VP9Picture> picture = new VaapiVP9Picture(va_surface);

  auto encode_job = CreateEncodeJob(is_keyframe, va_surface, picture);

  FRAME_TYPE libvpx_frame_type =
      is_keyframe ? FRAME_TYPE::KEY_FRAME : FRAME_TYPE::INTER_FRAME;
  EXPECT_CALL(
      *mock_rate_ctrl_,
      ComputeQP(MatchFrameParam(libvpx_frame_type, expected_temporal_layer_id,
                                expected_spatial_layer_id)))
      .WillOnce(Return());
  constexpr int kDefaultQP = 34;
  constexpr int kDefaultLoopFilterLevel = 8;
  EXPECT_CALL(*mock_rate_ctrl_, GetQP()).WillOnce(Return(kDefaultQP));
  EXPECT_CALL(*mock_rate_ctrl_, GetLoopfilterLevel())
      .WillOnce(Return(kDefaultLoopFilterLevel));

  // TODO(mcasas): Consider setting expectations on MockVaapiWrapper calls.

  EXPECT_TRUE(encoder_->PrepareEncodeJob(encode_job.get()));

  // TODO(hiroh): Test for encoder_->reference_frames_.

  constexpr size_t kDefaultEncodedFrameSize = 123456;
  // For BitrateControlUpdate sequence.
  EXPECT_CALL(*mock_rate_ctrl_, PostEncodeUpdate(kDefaultEncodedFrameSize))
      .WillOnce(Return());
  encoder_->BitrateControlUpdate(kDefaultEncodedFrameSize);
}

void VP9VaapiVideoEncoderDelegateTest::UpdateRatesSequence(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    BitrateControl bitrate_control,
    size_t num_spatial_layers,
    size_t num_temporal_layers) {
  ASSERT_TRUE(encoder_->current_params_.bitrate_allocation !=
                  bitrate_allocation ||
              encoder_->current_params_.framerate != framerate);

  ASSERT_EQ(bitrate_control, BitrateControl::kConstantQuantizationParameter);
  EXPECT_CALL(*mock_rate_ctrl_,
              UpdateRateControl(MatchRtcConfigWithRates(
                  encoder_->visible_size_, bitrate_allocation, framerate,
                  num_spatial_layers, num_temporal_layers)))
      .Times(1)
      .WillOnce(Return());

  EXPECT_TRUE(encoder_->UpdateRates(bitrate_allocation, framerate));
}

void VP9VaapiVideoEncoderDelegateTest::UpdateRatesTest(
    BitrateControl bitrate_control,
    size_t num_spatial_layers,
    size_t num_temporal_layers) {
  ASSERT_TRUE(num_temporal_layers <= VP9SVCLayers::kMaxSupportedTemporalLayers);
  const auto update_rates_and_encode =
      [this, bitrate_control, num_spatial_layers, num_temporal_layers](
          bool is_key_pic, const VideoBitrateAllocation& bitrate_allocation,
          uint32_t framerate) {
        UpdateRatesSequence(bitrate_allocation, framerate, bitrate_control,
                            num_spatial_layers, num_temporal_layers);
        ASSERT_EQ(bitrate_control,
                  BitrateControl::kConstantQuantizationParameter);

        for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
          const bool is_keyframe = is_key_pic && sid == 0;
          EncodeConstantQuantizationParameterSequence(
              is_keyframe, num_spatial_layers, {}, absl::nullopt,
              absl::nullopt);
          // Check if a rate change request is applied because the request is
          // applied during PrepareEncodeJob().
          EXPECT_EQ(encoder_->current_params_.bitrate_allocation,
                    bitrate_allocation);
          EXPECT_EQ(encoder_->current_params_.framerate, framerate);
        }
      };

  const uint32_t kBitrate =
      kDefaultVideoEncodeAcceleratorConfig.initial_bitrate;
  const uint32_t kFramerate =
      *kDefaultVideoEncodeAcceleratorConfig.initial_framerate;
  // Call UpdateRates before Encode.
  update_rates_and_encode(
      true,
      GetDefaultVideoBitrateAllocation(num_spatial_layers, num_temporal_layers,
                                       kBitrate / 2),
      kFramerate);
  // Bitrate change only.
  update_rates_and_encode(
      false,
      GetDefaultVideoBitrateAllocation(num_spatial_layers, num_temporal_layers,
                                       kBitrate),
      kFramerate);
  // Framerate change only.
  update_rates_and_encode(
      false,
      GetDefaultVideoBitrateAllocation(num_spatial_layers, num_temporal_layers,
                                       kBitrate),
      kFramerate + 2);
  // Bitrate + Frame changes.
  update_rates_and_encode(
      false,
      GetDefaultVideoBitrateAllocation(num_spatial_layers, num_temporal_layers,
                                       kBitrate * 3 / 4),
      kFramerate - 5);
}

struct VP9VaapiVideoEncoderDelegateTestParam {
  VP9VaapiVideoEncoderDelegateTest::BitrateControl bitrate_control;
  size_t num_spatial_layers;
  size_t num_temporal_layers;
} kTestCasesForVP9VaapiVideoEncoderDelegateTest[] = {
    // {VP9VaapiVideoEncoderDelegateTest::BitrateControl, num_spatial_layers,
    // num_temporal_layers}
    {VP9VaapiVideoEncoderDelegateTest::BitrateControl::
         kConstantQuantizationParameter,
     1u, 1u},
    {VP9VaapiVideoEncoderDelegateTest::BitrateControl::
         kConstantQuantizationParameter,
     1u, 2u},
    {VP9VaapiVideoEncoderDelegateTest::BitrateControl::
         kConstantQuantizationParameter,
     1u, 3u},
    {VP9VaapiVideoEncoderDelegateTest::BitrateControl::
         kConstantQuantizationParameter,
     2u, 1u},
    {VP9VaapiVideoEncoderDelegateTest::BitrateControl::
         kConstantQuantizationParameter,
     2u, 2u},
    {VP9VaapiVideoEncoderDelegateTest::BitrateControl::
         kConstantQuantizationParameter,
     2u, 3u},
    {VP9VaapiVideoEncoderDelegateTest::BitrateControl::
         kConstantQuantizationParameter,
     3u, 1u},
    {VP9VaapiVideoEncoderDelegateTest::BitrateControl::
         kConstantQuantizationParameter,
     3u, 2u},
    {VP9VaapiVideoEncoderDelegateTest::BitrateControl::
         kConstantQuantizationParameter,
     3u, 3u},
};

TEST_P(VP9VaapiVideoEncoderDelegateTest, Initialize) {
  InitializeVP9VaapiVideoEncoderDelegate(GetParam().bitrate_control,
                                         GetParam().num_spatial_layers,
                                         GetParam().num_temporal_layers);
}

TEST_P(VP9VaapiVideoEncoderDelegateTest, EncodeWithSoftwareBitrateControl) {
  const auto& bitrate_control = GetParam().bitrate_control;
  if (bitrate_control != BitrateControl::kConstantQuantizationParameter)
    GTEST_SKIP() << "Test only for with software bitrate control";

  const size_t num_spatial_layers = GetParam().num_spatial_layers;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9VaapiVideoEncoderDelegate(bitrate_control, num_spatial_layers,
                                         num_temporal_layers);

  constexpr size_t kEncodeFrames = 20;
  for (size_t frame_num = 0; frame_num < kEncodeFrames; ++frame_num) {
    for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
      const bool is_keyframe = (frame_num == 0 && sid == 0);
      std::array<bool, kVp9NumRefsPerFrame> ref_frames_used;
      absl::optional<uint8_t> temporal_layer_id;
      GetTemporalLayer(is_keyframe, frame_num, num_spatial_layers,
                       num_temporal_layers, &ref_frames_used,
                       &temporal_layer_id);
      EncodeConstantQuantizationParameterSequence(
          is_keyframe, num_spatial_layers, ref_frames_used, temporal_layer_id,
          sid);
    }
  }
}

TEST_P(VP9VaapiVideoEncoderDelegateTest,
       ForceKeyFrameWithSoftwareBitrateControl) {
  const auto& bitrate_control = GetParam().bitrate_control;
  if (bitrate_control != BitrateControl::kConstantQuantizationParameter)
    GTEST_SKIP() << "Test only for with software bitrate control";

  const size_t num_spatial_layers = GetParam().num_spatial_layers;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9VaapiVideoEncoderDelegate(bitrate_control, num_spatial_layers,
                                         num_temporal_layers);
  constexpr size_t kNumKeyFrames = 3;
  constexpr size_t kKeyFrameInterval = 20;
  for (size_t j = 0; j < kNumKeyFrames; j++) {
    for (size_t i = 0; i < kKeyFrameInterval; i++) {
      for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
        const bool keyframe = (i == 0 && sid == 0);
        std::array<bool, kVp9NumRefsPerFrame> ref_frames_used;
        absl::optional<uint8_t> temporal_layer_id;
        GetTemporalLayer(keyframe, i, num_spatial_layers, num_temporal_layers,
                         &ref_frames_used, &temporal_layer_id);
        EncodeConstantQuantizationParameterSequence(
            keyframe, num_spatial_layers, ref_frames_used, temporal_layer_id,
            sid);
      }
    }
  }
}

TEST_P(VP9VaapiVideoEncoderDelegateTest, UpdateRates) {
  const auto& bitrate_control = GetParam().bitrate_control;
  const size_t num_spatial_layers = GetParam().num_spatial_layers;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9VaapiVideoEncoderDelegate(bitrate_control, num_spatial_layers,
                                         num_temporal_layers);
  UpdateRatesTest(bitrate_control, num_spatial_layers, num_temporal_layers);
}

// TODO(crbug.com/1186051): Add the test case to activate and deactivate spatial
// layers.

INSTANTIATE_TEST_SUITE_P(
    ,
    VP9VaapiVideoEncoderDelegateTest,
    ::testing::ValuesIn(kTestCasesForVP9VaapiVideoEncoderDelegateTest));
}  // namespace media
