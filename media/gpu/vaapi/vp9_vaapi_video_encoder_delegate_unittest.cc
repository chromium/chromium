// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/vp9_vaapi_video_encoder_delegate.h"

#include <va/va.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <optional>
#include <tuple>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vp9_svc_layers.h"
#include "media/parsers/vp9_parser.h"
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

constexpr int kSpatialLayersResolutionScaleDenom[][3] = {
    {1, 0, 0},  // For one spatial layer.
    {2, 1, 0},  // For two spatial layers.
    {4, 2, 1},  // For three spatial layers.
};
constexpr uint8_t kTemporalLayerPattern[][4] = {
    {0, 0, 0, 0},
    {0, 1, 0, 1},
    {0, 2, 1, 2},
};

VaapiVideoEncoderDelegate::Config kDefaultVaapiVideoEncoderDelegateConfig{
    .max_num_ref_frames = kDefaultMaxNumRefFrames};

VideoEncodeAccelerator::Config DefaultVideoEncodeAcceleratorConfig() {
  VideoEncodeAccelerator::Config vea_config(
      PIXEL_FORMAT_I420, gfx::Size(1280, 720), VP9PROFILE_PROFILE0,
      Bitrate::ConstantBitrate(14000000u),
      VideoEncodeAccelerator::kDefaultFramerate,
      VideoEncodeAccelerator::Config::StorageType::kShmem,
      VideoEncodeAccelerator::Config::ContentType::kCamera);
  return vea_config;
}

constexpr std::array<bool, kVp9NumRefsPerFrame> kRefFramesUsedForKeyFrame = {
    false, false, false};
constexpr std::array<bool, kVp9NumRefsPerFrame> kRefFramesUsedForInterFrame = {
    true, true, true};
constexpr std::array<bool, kVp9NumRefsPerFrame>
    kRefFramesUsedForInterFrameInTemporalLayer = {true, false, false};

void GetTemporalLayer(bool keyframe,
                      int frame_num,
                      size_t num_spatial_layers,
                      size_t num_temporal_layers,
                      std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used,
                      uint8_t* temporal_layer_id) {
  switch (num_temporal_layers) {
    case 1:
      *temporal_layer_id = 0;
      if (num_spatial_layers > 1) {
        // K-SVC stream.
        *ref_frames_used = keyframe
                               ? kRefFramesUsedForKeyFrame
                               : kRefFramesUsedForInterFrameInTemporalLayer;
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
        *temporal_layer_id = kTemporalLayerPattern[1][frame_num % 4];
        *ref_frames_used = kRefFramesUsedForInterFrameInTemporalLayer;
      }
      break;
    case 3:
      if (keyframe) {
        *temporal_layer_id = 0u;
        *ref_frames_used = kRefFramesUsedForKeyFrame;
        return;
      }

      {
        *temporal_layer_id = kTemporalLayerPattern[2][frame_num % 4];
        *ref_frames_used = kRefFramesUsedForInterFrameInTemporalLayer;
      }
      break;
  }
}

VideoBitrateAllocation CreateBitrateAllocationWithActiveLayers(
    const VideoBitrateAllocation& bitrate_allocation,
    const std::vector<size_t>& active_layers,
    size_t num_temporal_layers) {
  VideoBitrateAllocation new_bitrate_allocation;
  for (size_t si : active_layers) {
    for (size_t ti = 0; ti < num_temporal_layers; ++ti) {
      const uint32_t bps = bitrate_allocation.GetBitrateBps(si, ti);
      new_bitrate_allocation.SetBitrate(si, ti, bps);
    }
  }

  return new_bitrate_allocation;
}

VideoBitrateAllocation AdaptBitrateAllocation(
    const VideoBitrateAllocation& bitrate_allocation) {
  VideoBitrateAllocation new_bitrate_allocation;
  size_t new_si = 0;
  for (size_t si = 0; si < VideoBitrateAllocation::kMaxSpatialLayers; ++si) {
    int sum = 0;
    for (size_t ti = 0; ti < VideoBitrateAllocation::kMaxTemporalLayers; ++ti)
      sum += bitrate_allocation.GetBitrateBps(si, ti);
    if (sum == 0) {
      // The spatial layer is disabled.
      continue;
    }

    for (size_t ti = 0; ti < VideoBitrateAllocation::kMaxTemporalLayers; ++ti) {
      const uint32_t bps = bitrate_allocation.GetBitrateBps(si, ti);
      new_bitrate_allocation.SetBitrate(new_si, ti, bps);
    }

    new_si++;
  }

  return new_bitrate_allocation;
}

std::vector<gfx::Size> GetDefaultSpatialLayerResolutions(
    size_t num_spatial_layers) {
  const gfx::Size kDefaultSize =
      DefaultVideoEncodeAcceleratorConfig().input_visible_size;
  std::vector<gfx::Size> spatial_layer_resolutions(num_spatial_layers);
  for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
    const int denom =
        kSpatialLayersResolutionScaleDenom[num_spatial_layers - 1][sid];
    spatial_layer_resolutions[sid] =
        gfx::Size(kDefaultSize.width() / denom, kDefaultSize.height() / denom);
  }

  return spatial_layer_resolutions;
}

MATCHER_P4(MatchRtcConfigWithRates,
           bitrate_allocation,
           framerate,
           num_temporal_layers,
           spatial_layer_resolutions,
           "") {
  if (arg.target_bandwidth < 0)
    return false;

  if (static_cast<uint32_t>(arg.target_bandwidth) !=
      bitrate_allocation.GetSumBps() / 1000) {
    return false;
  }

  if (arg.framerate != static_cast<double>(framerate))
    return false;

  const size_t num_spatial_layers = spatial_layer_resolutions.size();
  for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
    int bitrate_sum = 0;
    for (size_t tid = 0; tid < num_temporal_layers; ++tid) {
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

  const gfx::Size& size = spatial_layer_resolutions.back();
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
         arg.temporal_layer_id == temporal_layer_id &&
         arg.spatial_layer_id == spatial_layer_id;
}

MATCHER_P2(MatchVABufferDescriptor, va_buffer_type, va_buffer_size, "") {
  return arg.type == va_buffer_type && arg.size == va_buffer_size &&
         arg.data != nullptr;
}

class MockVaapiWrapper : public VaapiWrapper {
 public:
  MockVaapiWrapper()
      : VaapiWrapper(VADisplayStateHandle(),
                     kEncodeConstantQuantizationParameter) {}
  MOCK_METHOD1(SubmitBuffer_Locked, bool(const VABufferDescriptor&));

 protected:
  ~MockVaapiWrapper() override = default;
};

class MockVP9RateControl : public VP9RateControlWrapper {
 public:
  MockVP9RateControl() = default;
  ~MockVP9RateControl() override = default;

  MOCK_METHOD1(UpdateRateControl, void(const libvpx::VP9RateControlRtcConfig&));
  MOCK_CONST_METHOD0(GetLoopfilterLevel, int());
  MOCK_METHOD1(ComputeQP,
               libvpx::FrameDropDecision(const libvpx::VP9FrameParamsQpRTC&));
  MOCK_CONST_METHOD0(GetQP, int());
  MOCK_METHOD2(PostEncodeUpdate,
               void(uint64_t, const libvpx::VP9FrameParamsQpRTC&));
};
}  // namespace

struct VP9VaapiVideoEncoderDelegateTestParam;

class VP9VaapiVideoEncoderDelegateTest
    : public ::testing::TestWithParam<VP9VaapiVideoEncoderDelegateTestParam> {
 public:
  VP9VaapiVideoEncoderDelegateTest() = default;
  ~VP9VaapiVideoEncoderDelegateTest() override = default;

  void SetUp() override;

  MOCK_METHOD0(OnError, void());

 protected:
  void ResetEncoder();

  void InitializeVP9VaapiVideoEncoderDelegate(
      SVCInterLayerPredMode inter_layer_pred,
      size_t num_spatial_layers,
      size_t num_temporal_layers,
      bool enable_frame_drop);
  void EncodeConstantQuantizationParameterSequence(
      bool force_key,
      bool end_of_picture,
      base::TimeDelta timestamp,
      const gfx::Size& layer_size,
      std::optional<std::array<bool, kVp9NumRefsPerFrame>>
          expected_ref_frames_used,
      uint8_t expected_temporal_layer_id,
      uint8_t expected_spatial_layer_id,
      bool s_mode_keyframe,
      bool drop_frame);
  void UpdateRatesTest(SVCInterLayerPredMode inter_layer_pred,
                       size_t num_spatial_layers,
                       size_t num_temporal_layers);
  void UpdateRatesAndEncode(
      SVCInterLayerPredMode inter_layer_pred,
      const VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate,
      bool valid_rates_request,
      bool is_key_pic,
      const std::vector<gfx::Size>& expected_spatial_layer_resolutions,
      size_t expected_temporal_layers,
      size_t expected_temporal_layer_id);

 private:
  std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob> CreateEncodeJob(
      bool keyframe,
      uint8_t spatial_index,
      bool end_of_picture,
      base::TimeDelta timestamp,
      const VASurfaceID va_surface_id,
      const scoped_refptr<VP9Picture>& picture);

  std::unique_ptr<VP9VaapiVideoEncoderDelegate> encoder_;
  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  raw_ptr<MockVP9RateControl, DanglingUntriaged> mock_rate_ctrl_ = nullptr;
};

void VP9VaapiVideoEncoderDelegateTest::ResetEncoder() {
  encoder_ = std::make_unique<VP9VaapiVideoEncoderDelegate>(
      mock_vaapi_wrapper_,
      base::BindRepeating(&VP9VaapiVideoEncoderDelegateTest::OnError,
                          base::Unretained(this)));
}

void VP9VaapiVideoEncoderDelegateTest::SetUp() {
  mock_vaapi_wrapper_ = base::MakeRefCounted<MockVaapiWrapper>();
  ASSERT_TRUE(mock_vaapi_wrapper_);

  ResetEncoder();
  EXPECT_CALL(*this, OnError()).Times(0);
}

std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob>
VP9VaapiVideoEncoderDelegateTest::CreateEncodeJob(
    bool keyframe,
    uint8_t spatial_index,
    bool end_of_picture,
    base::TimeDelta timestamp,
    const VASurfaceID va_surface_id,
    const scoped_refptr<VP9Picture>& picture) {
  constexpr VABufferID kDummyVABufferID = 12;
  auto scoped_va_buffer = ScopedVABuffer::CreateForTesting(
      kDummyVABufferID, VAEncCodedBufferType,
      DefaultVideoEncodeAcceleratorConfig().input_visible_size.GetArea());

  return std::make_unique<VaapiVideoEncoderDelegate::EncodeJob>(
      keyframe, timestamp, spatial_index, end_of_picture, va_surface_id,
      picture, std::move(scoped_va_buffer));
}

void VP9VaapiVideoEncoderDelegateTest::InitializeVP9VaapiVideoEncoderDelegate(
    SVCInterLayerPredMode inter_layer_pred,
    size_t num_spatial_layers,
    size_t num_temporal_layers,
    bool enable_drop_frame) {
  auto config = DefaultVideoEncodeAcceleratorConfig();
  config.inter_layer_pred = inter_layer_pred;
  config.drop_frame_thresh_percentage = enable_drop_frame ? 30 : 0;
  auto ave_config = kDefaultVaapiVideoEncoderDelegateConfig;

  auto rate_ctrl = std::make_unique<MockVP9RateControl>();
  mock_rate_ctrl_ = rate_ctrl.get();
  encoder_->set_rate_ctrl_for_testing(std::move(rate_ctrl));

  auto initial_bitrate_allocation = AllocateDefaultBitrateForTesting(
      num_spatial_layers, num_temporal_layers,
      DefaultVideoEncodeAcceleratorConfig().bitrate);
  std::vector<gfx::Size> svc_layer_size =
      GetDefaultSpatialLayerResolutions(num_spatial_layers);
  if (num_spatial_layers > 1u || num_temporal_layers > 1u) {
    DCHECK_GT(num_spatial_layers, 0u);
    for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
      uint32_t sl_bitrate = 0;
      for (size_t tid = 0; tid < num_temporal_layers; ++tid)
        sl_bitrate += initial_bitrate_allocation.GetBitrateBps(sid, tid);

      VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
      spatial_layer.width = svc_layer_size[sid].width();
      spatial_layer.height = svc_layer_size[sid].height();
      spatial_layer.bitrate_bps = sl_bitrate;
      spatial_layer.framerate = config.framerate;
      spatial_layer.num_of_temporal_layers = num_temporal_layers;
      spatial_layer.max_qp = 30u;
      config.spatial_layers.push_back(spatial_layer);
    }
  }

  EXPECT_CALL(*mock_rate_ctrl_,
              UpdateRateControl(MatchRtcConfigWithRates(
                  AllocateDefaultBitrateForTesting(
                      num_spatial_layers, num_temporal_layers, config.bitrate),
                  VideoEncodeAccelerator::kDefaultFramerate,
                  num_temporal_layers, svc_layer_size)))
      .Times(1)
      .WillOnce(Return());

  EXPECT_TRUE(encoder_->Initialize(config, ave_config));
  EXPECT_EQ(num_temporal_layers > 1u || num_spatial_layers > 1u,
            !!encoder_->svc_layers_);
  EXPECT_EQ(encoder_->GetSVCLayerResolutions(), svc_layer_size);
}

void VP9VaapiVideoEncoderDelegateTest::
    EncodeConstantQuantizationParameterSequence(
        bool force_key,
        bool end_of_picture,
        base::TimeDelta timestamp,
        const gfx::Size& layer_size,
        std::optional<std::array<bool, kVp9NumRefsPerFrame>>
            expected_ref_frames_used,
        uint8_t expected_temporal_layer_id,
        uint8_t expected_spatial_layer_id,
        bool s_mode_keyframe,
        bool drop_frame) {
  InSequence seq;

  constexpr VASurfaceID kDummyVASurfaceID = 123;
  scoped_refptr<VP9Picture> picture(new VaapiVP9Picture(
      std::make_unique<VASurfaceHandle>(kDummyVASurfaceID, base::DoNothing())));

  auto encode_job =
      CreateEncodeJob(force_key, expected_spatial_layer_id, end_of_picture,
                      timestamp, kDummyVASurfaceID, picture);

  // The first frame will be set to KeyFrame under the S-mode.
  libvpx::RcFrameType libvpx_frame_type =
      force_key || s_mode_keyframe ? libvpx::RcFrameType::kKeyFrame
                                   : libvpx::RcFrameType::kInterFrame;

  constexpr int kDefaultQP = 34;
  if (drop_frame) {
    if (expected_spatial_layer_id == 0) {
      EXPECT_CALL(*mock_rate_ctrl_,
                  ComputeQP(MatchFrameParam(libvpx_frame_type,
                                            expected_temporal_layer_id,
                                            expected_spatial_layer_id)))
          .WillOnce(Return(libvpx::FrameDropDecision::kDrop));
    }

    EXPECT_EQ(encoder_->PrepareEncodeJob(*encode_job.get()),
              VaapiVideoEncoderDelegate::PrepareEncodeJobResult::kDrop);
    testing::Mock::VerifyAndClearExpectations(mock_rate_ctrl_);
    return;
  }

  EXPECT_CALL(
      *mock_rate_ctrl_,
      ComputeQP(MatchFrameParam(libvpx_frame_type, expected_temporal_layer_id,
                                expected_spatial_layer_id)))
      .WillOnce(Return(libvpx::FrameDropDecision::kOk));

  EXPECT_CALL(*mock_rate_ctrl_, GetQP()).WillOnce(Return(kDefaultQP));
  constexpr int kDefaultLoopFilterLevel = 8;
  EXPECT_CALL(*mock_rate_ctrl_, GetLoopfilterLevel())
      .WillOnce(Return(kDefaultLoopFilterLevel));

  EXPECT_CALL(*mock_vaapi_wrapper_,
              SubmitBuffer_Locked(MatchVABufferDescriptor(
                  VAEncSequenceParameterBufferType,
                  sizeof(VAEncSequenceParameterBufferVP9))))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_vaapi_wrapper_,
              SubmitBuffer_Locked(MatchVABufferDescriptor(
                  VAEncPictureParameterBufferType,
                  sizeof(VAEncPictureParameterBufferVP9))))
      .WillOnce(Return(true));

  EXPECT_EQ(encoder_->PrepareEncodeJob(*encode_job.get()),
            VaapiVideoEncoderDelegate::PrepareEncodeJobResult::kSuccess);

  const std::bitset<kDefaultMaxNumRefFrames> refresh_frame_flags(
      picture->frame_hdr->refresh_frame_flags);
  for (size_t i = 0; i < kDefaultMaxNumRefFrames; ++i) {
    if (refresh_frame_flags[i]) {
      EXPECT_EQ(encoder_->reference_frames_.GetFrame(i), picture);
    }
  }

  constexpr size_t kDefaultEncodedFrameSize = 123456;
  // For BitrateControlUpdate sequence.
  EXPECT_CALL(*mock_rate_ctrl_,
              PostEncodeUpdate(
                  kDefaultEncodedFrameSize,
                  MatchFrameParam(libvpx_frame_type, expected_temporal_layer_id,
                                  expected_spatial_layer_id)))
      .WillOnce(Return());
  BitstreamBufferMetadata metadata;
  metadata.payload_size_bytes = kDefaultEncodedFrameSize;
  metadata.key_frame = force_key || s_mode_keyframe;
  metadata.qp = kDefaultQP;
  if (encoder_->svc_layers_) {
    metadata.vp9.emplace();
    metadata.vp9->spatial_idx = expected_spatial_layer_id;
    metadata.vp9->temporal_idx = expected_temporal_layer_id;
  }
  encoder_->BitrateControlUpdate(metadata);
}

void VP9VaapiVideoEncoderDelegateTest::UpdateRatesAndEncode(
    SVCInterLayerPredMode inter_layer_pred,
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    bool valid_rates_request,
    bool is_key_pic,
    const std::vector<gfx::Size>& expected_spatial_layer_resolutions,
    size_t expected_temporal_layers,
    size_t expected_temporal_layer_id) {
  ASSERT_TRUE(encoder_->current_params_.bitrate_allocation !=
                  bitrate_allocation ||
              encoder_->current_params_.framerate != framerate);
  // Since the request is pended, this is always successful and no call happens
  // to VP9SVCLayersStateful and RateControl.
  EXPECT_TRUE(encoder_->UpdateRates(bitrate_allocation, framerate));
  EXPECT_TRUE(encoder_->pending_update_rates_.has_value());

  // The pending update rates request is applied in GetSVCLayerResolutions().
  if (!valid_rates_request) {
    EXPECT_TRUE(encoder_->GetSVCLayerResolutions().empty());
    return;
  }

  // VideoBitrateAllocation is adapted if some spatial layers are deactivated.
  const VideoBitrateAllocation adapted_bitrate_allocation =
      AdaptBitrateAllocation(bitrate_allocation);

  EXPECT_CALL(*mock_rate_ctrl_, UpdateRateControl(MatchRtcConfigWithRates(
                                    adapted_bitrate_allocation, framerate,
                                    expected_temporal_layers,
                                    expected_spatial_layer_resolutions)))
      .Times(1)
      .WillOnce(Return());

  EXPECT_EQ(encoder_->GetSVCLayerResolutions(),
            expected_spatial_layer_resolutions);

  EXPECT_FALSE(encoder_->pending_update_rates_.has_value());
  EXPECT_EQ(encoder_->current_params_.bitrate_allocation,
            adapted_bitrate_allocation);
  EXPECT_EQ(encoder_->current_params_.framerate, framerate);

  base::TimeDelta timestamp = base::Milliseconds(1);
  const size_t num_spatial_layers = expected_spatial_layer_resolutions.size();
  for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
    const gfx::Size& layer_size = expected_spatial_layer_resolutions[sid];
    const bool is_keyframe = is_key_pic && sid == 0;
    const bool end_of_picture = sid == num_spatial_layers - 1;
    const bool s_mode_keyframe =
        (inter_layer_pred == SVCInterLayerPredMode::kOff) && is_key_pic;
    EncodeConstantQuantizationParameterSequence(
        is_keyframe, end_of_picture, timestamp, layer_size,
        /*expected_ref_frames_used=*/{}, expected_temporal_layer_id, sid,
        s_mode_keyframe,
        /*drop_frame=*/false);
  }
}

void VP9VaapiVideoEncoderDelegateTest::UpdateRatesTest(
    SVCInterLayerPredMode inter_layer_pred,
    size_t num_spatial_layers,
    size_t num_temporal_layers) {
  ASSERT_LE(num_temporal_layers, VP9SVCLayers::kMaxTemporalLayers);
  ASSERT_LE(num_spatial_layers, VP9SVCLayers::kMaxSpatialLayers);
  const auto spatial_layer_resolutions =
      GetDefaultSpatialLayerResolutions(num_spatial_layers);
  auto update_rates_and_encode = [this, inter_layer_pred, num_spatial_layers,
                                  num_temporal_layers,
                                  &spatial_layer_resolutions](
                                     bool is_key_pic,
                                     uint8_t expected_temporal_layer_id,
                                     uint32_t bitrate, uint32_t framerate) {
    auto bitrate_allocation = AllocateDefaultBitrateForTesting(
        num_spatial_layers, num_temporal_layers,
        media::Bitrate::ConstantBitrate(bitrate));
    UpdateRatesAndEncode(inter_layer_pred, bitrate_allocation, framerate,
                         /*valid_rates_request=*/true, is_key_pic,
                         spatial_layer_resolutions, num_temporal_layers,
                         expected_temporal_layer_id);
  };

  const uint32_t kBitrate =
      DefaultVideoEncodeAcceleratorConfig().bitrate.target_bps();
  const uint32_t kFramerate = DefaultVideoEncodeAcceleratorConfig().framerate;
  const uint8_t* expected_temporal_ids =
      kTemporalLayerPattern[num_temporal_layers - 1];
  // Call UpdateRates before Encode.
  update_rates_and_encode(true, expected_temporal_ids[0], kBitrate / 2,
                          kFramerate);
  // Bitrate change only.
  update_rates_and_encode(false, expected_temporal_ids[1], kBitrate,
                          kFramerate);
  // Framerate change only.
  update_rates_and_encode(false, expected_temporal_ids[2], kBitrate,
                          kFramerate + 2);
  // Bitrate + Frame changes.
  update_rates_and_encode(false, expected_temporal_ids[3], kBitrate * 3 / 4,
                          kFramerate - 5);
}

struct VP9VaapiVideoEncoderDelegateTestParam {
  SVCInterLayerPredMode inter_layer_pred = SVCInterLayerPredMode::kOnKeyPic;
  size_t num_spatial_layers;
  size_t num_temporal_layers;
} kTestCasesForVP9VaapiVideoEncoderDelegateTest[] = {
    // {inter_layer_pred, num_of_spatial_layers, num_of_temporal_layers}
    {SVCInterLayerPredMode::kOn, 1u, 1u},
    {SVCInterLayerPredMode::kOn, 1u, 2u},
    {SVCInterLayerPredMode::kOn, 1u, 3u},
    {SVCInterLayerPredMode::kOnKeyPic, 1u, 1u},
    {SVCInterLayerPredMode::kOnKeyPic, 1u, 2u},
    {SVCInterLayerPredMode::kOnKeyPic, 1u, 3u},
    {SVCInterLayerPredMode::kOnKeyPic, 2u, 1u},
    {SVCInterLayerPredMode::kOnKeyPic, 2u, 2u},
    {SVCInterLayerPredMode::kOnKeyPic, 2u, 3u},
    {SVCInterLayerPredMode::kOnKeyPic, 3u, 1u},
    {SVCInterLayerPredMode::kOnKeyPic, 3u, 2u},
    {SVCInterLayerPredMode::kOnKeyPic, 3u, 3u},
    {SVCInterLayerPredMode::kOff, 1u, 1u},
    {SVCInterLayerPredMode::kOff, 1u, 2u},
    {SVCInterLayerPredMode::kOff, 1u, 3u},
    {SVCInterLayerPredMode::kOff, 2u, 1u},
    {SVCInterLayerPredMode::kOff, 2u, 2u},
    {SVCInterLayerPredMode::kOff, 2u, 3u},
    {SVCInterLayerPredMode::kOff, 3u, 1u},
    {SVCInterLayerPredMode::kOff, 3u, 2u},
    {SVCInterLayerPredMode::kOff, 3u, 3u},

};

TEST_P(VP9VaapiVideoEncoderDelegateTest, Initialize) {
  InitializeVP9VaapiVideoEncoderDelegate(GetParam().inter_layer_pred,
                                         GetParam().num_spatial_layers,
                                         GetParam().num_temporal_layers,
                                         /*enable_frame_drop=*/false);
}

TEST_P(VP9VaapiVideoEncoderDelegateTest, EncodeWithSoftwareBitrateControl) {
  const SVCInterLayerPredMode inter_layer_pred = GetParam().inter_layer_pred;
  const size_t num_spatial_layers = GetParam().num_spatial_layers;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9VaapiVideoEncoderDelegate(inter_layer_pred, num_spatial_layers,
                                         num_temporal_layers,
                                         /*enable_frame_drop=*/false);

  const std::vector<gfx::Size> layer_sizes =
      GetDefaultSpatialLayerResolutions(num_spatial_layers);
  constexpr size_t kEncodeFrames = 20;
  for (size_t frame_num = 0; frame_num < kEncodeFrames; ++frame_num) {
    base::TimeDelta timestamp = base::Milliseconds(frame_num);
    for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
      const bool is_keyframe = (frame_num == 0 && sid == 0);
      const bool end_of_picture = sid == num_spatial_layers - 1;
      const bool s_mode_keyframe =
          (inter_layer_pred == SVCInterLayerPredMode::kOff) && (frame_num == 0);
      std::array<bool, kVp9NumRefsPerFrame> ref_frames_used;
      uint8_t temporal_layer_id;
      GetTemporalLayer(is_keyframe, frame_num, num_spatial_layers,
                       num_temporal_layers, &ref_frames_used,
                       &temporal_layer_id);
      EncodeConstantQuantizationParameterSequence(
          is_keyframe, end_of_picture, timestamp, layer_sizes[sid],
          ref_frames_used, temporal_layer_id, sid, s_mode_keyframe,
          /*drop_frame=*/false);
    }
  }
}

TEST_P(VP9VaapiVideoEncoderDelegateTest,
       ForceKeyFrameWithSoftwareBitrateControl) {
  const SVCInterLayerPredMode inter_layer_pred = GetParam().inter_layer_pred;
  const size_t num_spatial_layers = GetParam().num_spatial_layers;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9VaapiVideoEncoderDelegate(inter_layer_pred, num_spatial_layers,
                                         num_temporal_layers,
                                         /*enable_frame_drop=*/false);
  constexpr size_t kNumKeyFrames = 3;
  constexpr size_t kKeyFrameInterval = 20;
  const std::vector<gfx::Size> layer_sizes =
      GetDefaultSpatialLayerResolutions(num_spatial_layers);
  for (size_t j = 0; j < kNumKeyFrames; ++j) {
    for (size_t i = 0; i < kKeyFrameInterval; ++i) {
      base::TimeDelta timestamp = base::Milliseconds(j * kKeyFrameInterval + i);
      for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
        const bool keyframe = (i == 0 && sid == 0);
        const bool end_of_picture = sid == num_spatial_layers - 1;
        const bool s_mode_keyframe =
            (inter_layer_pred == SVCInterLayerPredMode::kOff) && (i == 0);
        std::array<bool, kVp9NumRefsPerFrame> ref_frames_used;
        uint8_t temporal_layer_id;
        GetTemporalLayer(keyframe, i, num_spatial_layers, num_temporal_layers,
                         &ref_frames_used, &temporal_layer_id);
        EncodeConstantQuantizationParameterSequence(
            keyframe, end_of_picture, timestamp, layer_sizes[sid],
            ref_frames_used, temporal_layer_id, sid, s_mode_keyframe,
            /*drop_frame=*/false);
      }
    }
  }
}

TEST_P(VP9VaapiVideoEncoderDelegateTest,
       EncodeWithSoftwareBitrateControlEnablingFrameDrop) {
  const SVCInterLayerPredMode inter_layer_pred = GetParam().inter_layer_pred;
  const size_t num_spatial_layers = GetParam().num_spatial_layers;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9VaapiVideoEncoderDelegate(inter_layer_pred, num_spatial_layers,
                                         num_temporal_layers,
                                         /*enable_frame_drop=*/true);

  const std::vector<gfx::Size> layer_sizes =
      GetDefaultSpatialLayerResolutions(num_spatial_layers);
  constexpr size_t kEncodeFrames = 20;
  constexpr size_t kDropFrameIndices[] = {7, 12, 18};
  size_t frame_num = 0;
  for (size_t i = 0; i < kEncodeFrames; ++i) {
    base::TimeDelta timestamp = base::Milliseconds(i);
    const bool drop_frame = base::Contains(kDropFrameIndices, i);
    for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
      const bool is_keyframe = (frame_num == 0 && sid == 0);
      const bool end_of_picture = sid == num_spatial_layers - 1;
      const bool s_mode_keyframe =
          (inter_layer_pred == SVCInterLayerPredMode::kOff) && (frame_num == 0);

      std::array<bool, kVp9NumRefsPerFrame> ref_frames_used;
      uint8_t temporal_layer_id;
      GetTemporalLayer(is_keyframe, frame_num, num_spatial_layers,
                       num_temporal_layers, &ref_frames_used,
                       &temporal_layer_id);
      EncodeConstantQuantizationParameterSequence(
          is_keyframe, end_of_picture, timestamp, layer_sizes[sid],
          ref_frames_used, temporal_layer_id, sid, s_mode_keyframe, drop_frame);
    }
    frame_num += !drop_frame;
  }
}

TEST_P(VP9VaapiVideoEncoderDelegateTest, UpdateRates) {
  const SVCInterLayerPredMode inter_layer_pred = GetParam().inter_layer_pred;
  const size_t num_spatial_layers = GetParam().num_spatial_layers;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  InitializeVP9VaapiVideoEncoderDelegate(inter_layer_pred, num_spatial_layers,
                                         num_temporal_layers,
                                         /*enable_frame_drop=*/false);
  UpdateRatesTest(inter_layer_pred, num_spatial_layers, num_temporal_layers);
}

TEST_P(VP9VaapiVideoEncoderDelegateTest, DeactivateActivateSpatialLayers) {
  const SVCInterLayerPredMode inter_layer_pred = GetParam().inter_layer_pred;
  const size_t num_spatial_layers = GetParam().num_spatial_layers;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  if (num_spatial_layers == 1)
    GTEST_SKIP() << "Skip a single spatial layer";

  InitializeVP9VaapiVideoEncoderDelegate(inter_layer_pred, num_spatial_layers,
                                         num_temporal_layers,
                                         /*enable_frame_drop=*/false);

  struct ActivationQuery {
    size_t num_temporal_layers;
    std::vector<size_t> active_layers;
  };
  std::vector<ActivationQuery> kQueries[2] = {
      {
          // Two spatial layers.
          {num_temporal_layers, {0}},     // Deactivate the top layer.
          {num_temporal_layers, {0, 1}},  // Activate the top layer.
          {num_temporal_layers, {1}},     // Deactivate the bottom layer.
          {num_temporal_layers, {0, 1}},  // Activate the bottom layer.
          {1, {0}},                       // L1T1
          {3, {0, 1}},                    // L2T3
          {1, {1}},                       // L1T1
          {3, {0}},                       // L1T3
          {2, {0, 1}},                    // L2T2
      },
      {
          // Three spatial layers.
          {num_temporal_layers, {0, 1}},     // Deactivate the top layer.
          {num_temporal_layers, {1}},        // Deactivate the bottom layer.
          {num_temporal_layers, {0}},        // Activate the bottom layer and
                                             // deactivate the top two layers.
          {num_temporal_layers, {1, 2}},     // Activate the top two layers and
                                             // deactivate the bottom layer.
          {num_temporal_layers, {0, 1, 2}},  // Activate the bottom layer.
          {num_temporal_layers, {2}},  // Deactivate the bottom two layers.
          {num_temporal_layers, {0, 1, 2}},  // Activate the bottom two layers.
          {3, {1}},                          // L1T3
          {3, {0, 1, 2}},                    // L3T3
          {2, {0, 1}},                       // L2T2
          {1, {0, 1}},                       // L2T1
          {1, {0}},                          // L1T1
          {1, {0, 1, 2}},                    // L3T1
      },
  };

  // Allocate a default bitrate allocation with the maximum temporal layers so
  // that it has non-zero bitrate up to the maximum supported temporal layers.
  const VideoBitrateAllocation kDefaultBitrateAllocation =
      AllocateDefaultBitrateForTesting(
          num_spatial_layers, VP9SVCLayers::kMaxTemporalLayers,
          DefaultVideoEncodeAcceleratorConfig().bitrate);
  const std::vector<gfx::Size> kDefaultSpatialLayers =
      GetDefaultSpatialLayerResolutions(num_spatial_layers);
  const uint32_t kFramerate = DefaultVideoEncodeAcceleratorConfig().framerate;

  for (const auto& query : kQueries[num_spatial_layers - 2]) {
    const auto& active_layers = query.active_layers;
    const VideoBitrateAllocation bitrate_allocation =
        CreateBitrateAllocationWithActiveLayers(kDefaultBitrateAllocation,
                                                query.active_layers,
                                                query.num_temporal_layers);
    std::vector<gfx::Size> spatial_layer_resolutions;
    for (size_t active_sid : active_layers)
      spatial_layer_resolutions.emplace_back(kDefaultSpatialLayers[active_sid]);

    // Always is_key_pic=true and temporal_layer_id=0 because the active spatial
    // layers are changed.
    UpdateRatesAndEncode(inter_layer_pred, bitrate_allocation, kFramerate,
                         /*valid_rates_request=*/true,
                         /*is_key_pic=*/true, spatial_layer_resolutions,
                         query.num_temporal_layers,
                         /*expected_temporal_layer_id=*/0u);
  }
}

TEST_P(VP9VaapiVideoEncoderDelegateTest, FailsWithInvalidSpatialLayers) {
  const SVCInterLayerPredMode inter_layer_pred = GetParam().inter_layer_pred;
  const size_t num_spatial_layers = GetParam().num_spatial_layers;
  const size_t num_temporal_layers = GetParam().num_temporal_layers;
  const VideoBitrateAllocation kDefaultBitrateAllocation =
      AllocateDefaultBitrateForTesting(
          num_spatial_layers, num_temporal_layers,
          DefaultVideoEncodeAcceleratorConfig().bitrate);
  std::vector<VideoBitrateAllocation> invalid_bitrate_allocations;
  constexpr uint32_t kBitrate = 1234u;
  auto bitrate_allocation = kDefaultBitrateAllocation;
  // Activate one more top spatial layer.
  ASSERT_LE(num_spatial_layers + 1, VideoBitrateAllocation::kMaxSpatialLayers);
  bitrate_allocation.SetBitrate(num_spatial_layers, /*temporal_index=*/0,
                                kBitrate);
  invalid_bitrate_allocations.push_back(bitrate_allocation);

  // Deactivate a middle spatial layer.
  if (num_spatial_layers == 3) {
    bitrate_allocation = kDefaultBitrateAllocation;
    for (size_t ti = 0; ti < VideoBitrateAllocation::kMaxTemporalLayers; ++ti)
      bitrate_allocation.SetBitrate(1, ti, 0u);
    invalid_bitrate_allocations.push_back(bitrate_allocation);
  }

  // Set 0 in the bottom temporal layer.
  if (num_temporal_layers > 1) {
    bitrate_allocation = kDefaultBitrateAllocation;
    bitrate_allocation.SetBitrate(/*spatial_index=*/0, /*temporal_index=*/0,
                                  0u);
    invalid_bitrate_allocations.push_back(bitrate_allocation);
  }

  // Set 0 in the middle temporal layer
  if (num_temporal_layers == 3) {
    bitrate_allocation = kDefaultBitrateAllocation;
    bitrate_allocation.SetBitrate(/*spatial_index=*/0, /*temporal_index=*/1,
                                  0u);
    invalid_bitrate_allocations.push_back(bitrate_allocation);
  }

  const uint32_t kFramerate = DefaultVideoEncodeAcceleratorConfig().framerate;
  for (const auto& invalid_allocation : invalid_bitrate_allocations) {
    InitializeVP9VaapiVideoEncoderDelegate(inter_layer_pred, num_spatial_layers,
                                           num_temporal_layers,
                                           /*enable_frame_drop=*/false);

    // The values of expected_spatial_layer_resolutions, is_key_pic,
    // expected_temporal_layers and expected_temporal_layer_id are meaningless
    // because UpdateRatesAndEncode will returns before checking them due to the
    // invalid VideoBitrateAllocation request.
    UpdateRatesAndEncode(inter_layer_pred, invalid_allocation, kFramerate,
                         /*valid_rates_request=*/false,
                         /*is_key_pic=*/true,
                         /*expected_spatial_layer_resolutions=*/{},
                         /*expected_temporal_layers=*/0u,
                         /*expected_temporal_layer_id=*/0u);

    ResetEncoder();
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VP9VaapiVideoEncoderDelegateTest,
    ::testing::ValuesIn(kTestCasesForVP9VaapiVideoEncoderDelegateTest));
}  // namespace media
