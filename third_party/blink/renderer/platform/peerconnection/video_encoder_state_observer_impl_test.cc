// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/peerconnection/video_encoder_state_observer_impl.h"

#include <queue>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/video_codecs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/video_encoder_state_observer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/video/encoded_image.h"
#include "third_party/webrtc/api/video/video_content_type.h"
#include "third_party/webrtc/api/video/video_frame_type.h"
#include "third_party/webrtc/modules/video_coding/svc/scalability_mode_util.h"

namespace blink {

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 720;

webrtc::VideoCodec BasicVideoCodec(webrtc::VideoCodecType codec_type,
                                   int width,
                                   int height) {
  webrtc::VideoCodec video_codec;
  video_codec.codecType = codec_type;
  video_codec.width = width;
  video_codec.height = height;
  video_codec.startBitrate = 300;
  video_codec.minBitrate = 30;
  video_codec.maxBitrate = 300;
  video_codec.maxFramerate = 30;
  video_codec.qpMax = 0;
  video_codec.active = true;
  video_codec.mode = webrtc::VideoCodecMode::kRealtimeVideo;
  return video_codec;
}

void FillSimulcastStreams(webrtc::VideoCodec& video_codec,
                          unsigned int num_simulcast_streams,
                          unsigned int num_temporal_layers) {
  CHECK_LE(num_simulcast_streams, std::size(video_codec.simulcastStream));
  video_codec.numberOfSimulcastStreams = num_simulcast_streams;
  for (unsigned int i = 0; i < num_simulcast_streams; i++) {
    webrtc::SimulcastStream& ss = video_codec.simulcastStream[i];
    const int log_scale = num_simulcast_streams - i - 1;
    ss.width = video_codec.width >> log_scale;
    ss.height = video_codec.height >> log_scale;
    ss.maxFramerate = video_codec.maxFramerate;
    ss.numberOfTemporalLayers = num_temporal_layers;
    ss.targetBitrate = video_codec.maxBitrate >> log_scale;
    ss.maxBitrate = ss.targetBitrate;
    ss.minBitrate = ss.targetBitrate;
    ss.qpMax = 0;
    ss.active = true;
  };
}

webrtc::VideoCodec VP8VideoCodec(unsigned int num_simulcast_streams,
                                 unsigned int num_temporal_layers,
                                 int top_layer_width = kWidth,
                                 int top_layer_height = kHeight) {
  webrtc::VideoCodec video_codec = BasicVideoCodec(
      webrtc::kVideoCodecVP8, top_layer_width, top_layer_height);
  FillSimulcastStreams(video_codec, num_simulcast_streams, num_temporal_layers);
  video_codec.VP8()->numberOfTemporalLayers = num_temporal_layers;
  video_codec.SetScalabilityMode(*webrtc::MakeScalabilityMode(
      /*num_spatial_layers=*/1, num_temporal_layers,
      webrtc::InterLayerPredMode::kOff,
      /*ratio=*/std::nullopt,
      /*shift=*/false));
  return video_codec;
}

// This is based on SimulcastEncoderAdapter::MakeStreamCodec().
webrtc::VideoCodec CreateStreamCodec(const webrtc::VideoCodec& codec,
                                     int stream_idx,
                                     bool is_highest_quality_stream) {
  webrtc::VideoCodec codec_params = codec;
  const webrtc::SimulcastStream& stream_params =
      codec.simulcastStream[stream_idx];

  codec_params.numberOfSimulcastStreams = 0;
  codec_params.width = stream_params.width;
  codec_params.height = stream_params.height;
  codec_params.maxBitrate = stream_params.maxBitrate;
  codec_params.minBitrate = stream_params.minBitrate;
  codec_params.maxFramerate = stream_params.maxFramerate;
  codec_params.qpMax = stream_params.qpMax;
  codec_params.active = stream_params.active;
  std::optional<webrtc::ScalabilityMode> scalability_mode =
      stream_params.GetScalabilityMode();
  if (codec.GetScalabilityMode().has_value()) {
    bool only_active_stream = true;
    for (int i = 0; i < codec.numberOfSimulcastStreams; ++i) {
      if (i != stream_idx && codec.simulcastStream[i].active) {
        only_active_stream = false;
        break;
      }
    }
    if (only_active_stream) {
      scalability_mode = codec.GetScalabilityMode();
    }
  }
  if (scalability_mode.has_value()) {
    codec_params.SetScalabilityMode(*scalability_mode);
  }
  if (codec.codecType == webrtc::kVideoCodecVP8) {
    codec_params.VP8()->numberOfTemporalLayers =
        stream_params.numberOfTemporalLayers;
    if (!is_highest_quality_stream) {
      // For resolutions below CIF, set the codec `complexity` parameter to
      // kComplexityHigher, which maps to cpu_used = -4.
      int pixels_per_frame = codec_params.width * codec_params.height;
      if (pixels_per_frame < 352 * 288) {
        codec_params.SetVideoEncoderComplexity(
            webrtc::VideoCodecComplexity::kComplexityHigher);
      }
      // Turn off denoising for all streams but the highest resolution.
      codec_params.VP8()->denoisingOn = false;
    }
  } else if (codec.codecType == webrtc::kVideoCodecH264) {
    codec_params.H264()->numberOfTemporalLayers =
        stream_params.numberOfTemporalLayers;
  }

  return codec_params;
}

void FillSpatialLayers(webrtc::VideoCodec& video_codec,
                       unsigned int num_spatial_layers,
                       unsigned int num_temporal_layers) {
  CHECK_LE(num_spatial_layers, std::size(video_codec.simulcastStream));
  for (unsigned int i = 0; i < num_spatial_layers; i++) {
    webrtc::SpatialLayer& sl = video_codec.spatialLayers[i];
    const int log_scale = num_spatial_layers - i - 1;
    sl.width = video_codec.width >> log_scale;
    sl.height = video_codec.height >> log_scale;
    sl.maxFramerate = video_codec.maxFramerate;
    sl.numberOfTemporalLayers = num_temporal_layers;
    sl.targetBitrate = video_codec.maxBitrate >> log_scale;
    sl.maxBitrate = sl.targetBitrate;
    sl.minBitrate = sl.targetBitrate;
    sl.qpMax = 0;
    sl.active = true;
  };
}

webrtc::VideoCodec VP9kSVCVideoCodec(unsigned int num_spatial_layers,
                                     unsigned int num_temporal_layers,
                                     int top_layer_width = kWidth,
                                     int top_layer_height = kHeight) {
  webrtc::VideoCodec video_codec = BasicVideoCodec(
      webrtc::kVideoCodecVP9, top_layer_width, top_layer_height);
  FillSpatialLayers(video_codec, num_spatial_layers, num_temporal_layers);
  webrtc::VideoCodecVP9& vp9 = *video_codec.VP9();
  vp9.numberOfTemporalLayers = num_temporal_layers;
  vp9.numberOfSpatialLayers = num_spatial_layers;
  vp9.interLayerPred = webrtc::InterLayerPredMode::kOff;

  video_codec.SetScalabilityMode(*webrtc::MakeScalabilityMode(
      num_spatial_layers, num_temporal_layers,
      webrtc::InterLayerPredMode::kOnKeyPic,
      /*ratio=*/webrtc::ScalabilityModeResolutionRatio::kTwoToOne,
      /*shift=*/false));
  return video_codec;
}

template <typename T>  // webrtc::SimulcastStream, webrtc::VideoCodec.
int PixelRate(const T& config) {
  base::CheckedNumeric<int> pixel_rate = config.width;
  pixel_rate *= config.height;
  pixel_rate *= config.maxFramerate;
  return pixel_rate.ValueOrDie();
}

std::tuple<size_t, size_t, size_t> GetActiveIndexInfo(
    const Vector<bool>& active_layers) {
  size_t num_active_layers = 0;
  int bottom_sid = -1;
  int top_sid = -1;
  for (size_t i = 0; i < active_layers.size(); i++) {
    if (active_layers[i]) {
      num_active_layers++;
      top_sid = i;
      if (bottom_sid == -1) {
        bottom_sid = i;
      }
    }
  }
  if (num_active_layers == 0) {
    return {0, 0, 0};
  }

  return {num_active_layers, bottom_sid, top_sid};
}
}  // namespace

class VideoEncoderStateObserverImplTest : public ::testing::Test {
 public:
  VideoEncoderStateObserverImplTest() = default;
  ~VideoEncoderStateObserverImplTest() override = default;

  void TearDown() override { observer_.reset(); }

 protected:
  using TopLayerInfo = VideoEncoderStateObserverImpl::TopLayerInfo;
  using EncodeResult = VideoEncoderStateObserver::EncodeResult;
  using StatsKey = StatsCollector::StatsKey;
  using VideoStats = StatsCollector::VideoStats;

  void CreateObserver(media::VideoCodecProfile profile) {
    observer_ = std::make_unique<VideoEncoderStateObserverImpl>(
        profile, base::BindRepeating(
                     &VideoEncoderStateObserverImplTest::StoreProcessingStats,
                     base::Unretained(this)));
    ASSERT_TRUE(observer_);
  }

  void ExpectTopLayerForSimulcast(
      int stream_idx,
      int encoder_id_offset,
      base::span<const webrtc::VideoCodec> codec_params) {
    ExpectTopLayer(encoder_id_offset + stream_idx, 0,
                   PixelRate(codec_params[stream_idx]));
  }

  void ExpectTopLayerForSVC(int spatial_id,
                            int encoder_id,
                            base::span<const int> pixel_rates) {
    ExpectTopLayer(encoder_id, spatial_id, pixel_rates[spatial_id]);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<VideoEncoderStateObserverImpl> observer_;
  std::queue<std::pair<StatsKey, VideoStats>> processing_stats_;

 private:
  void ExpectTopLayer(int encoder_id, int spatial_id, int pixel_rate) {
    CHECK(observer_);
    const std::optional<VideoEncoderStateObserverImpl::TopLayerInfo> top_layer =
        observer_->FindHighestActiveEncoding();
    ASSERT_TRUE(top_layer.has_value());
    EXPECT_EQ(top_layer->encoder_id, encoder_id);
    EXPECT_EQ(top_layer->spatial_id, spatial_id);
    EXPECT_EQ(top_layer->pixel_rate, pixel_rate);
  }

  void StoreProcessingStats(const StatsKey& stats_key,
                            const VideoStats& video_stats) {
    processing_stats_.emplace(stats_key, video_stats);
  }
};

TEST_F(VideoEncoderStateObserverImplTest,
       FindHighestActiveEncoding_CreateAndDestroy_VP8Vanilla_SingleEncoder) {
  constexpr int kEncoderId = 2;
  constexpr int kSimulcasts = 1;
  constexpr int kTemporalLayers = 3;
  const auto vp8 = VP8VideoCodec(kSimulcasts, kTemporalLayers);

  CreateObserver(media::VP8PROFILE_ANY);
  observer_->OnEncoderCreated(kEncoderId, vp8);

  std::optional<VideoEncoderStateObserverImpl::TopLayerInfo> top_layer =
      observer_->FindHighestActiveEncoding();
  ASSERT_TRUE(top_layer.has_value());
  EXPECT_EQ(top_layer->encoder_id, kEncoderId);
  EXPECT_EQ(top_layer->spatial_id, kSimulcasts - 1);
  EXPECT_EQ(top_layer->pixel_rate, PixelRate(vp8));

  observer_->OnEncoderDestroyed(kEncoderId);

  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());
}

TEST_F(VideoEncoderStateObserverImplTest,
       FindHighestActiveEncoding_CreateAndDestroy_VP8Simulcast_SingleEncoder) {
  constexpr int kEncoderId = 8;
  constexpr int kSimulcasts = 3;
  constexpr int kTemporalLayers = 3;
  const auto vp8 = VP8VideoCodec(kSimulcasts, kTemporalLayers);

  CreateObserver(media::VP8PROFILE_ANY);
  observer_->OnEncoderCreated(kEncoderId, vp8);

  std::optional<VideoEncoderStateObserverImpl::TopLayerInfo> top_layer =
      observer_->FindHighestActiveEncoding();
  ASSERT_TRUE(top_layer.has_value());
  EXPECT_EQ(top_layer->encoder_id, kEncoderId);
  EXPECT_EQ(top_layer->spatial_id, kSimulcasts - 1);
  EXPECT_EQ(top_layer->pixel_rate, PixelRate(vp8));

  observer_->OnEncoderDestroyed(kEncoderId);

  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());
}

TEST_F(
    VideoEncoderStateObserverImplTest,
    FindHighestActiveEncoding_CreateAndDestroy_VP8Simulcast_MultipleEncoders) {
  constexpr int kBaseEncoderId = 8;
  constexpr int kSimulcasts = 3;
  constexpr int kTemporalLayers = 3;

  CreateObserver(media::VP8PROFILE_ANY);
  const auto codec = VP8VideoCodec(kSimulcasts, kTemporalLayers);
  webrtc::VideoCodec codec_params[kSimulcasts];
  for (size_t stream_idx = 0; stream_idx < kSimulcasts; stream_idx++) {
    codec_params[stream_idx] =
        CreateStreamCodec(codec, stream_idx, stream_idx == kSimulcasts - 1);
    observer_->OnEncoderCreated(kBaseEncoderId + stream_idx,
                                codec_params[stream_idx]);
  }

  ExpectTopLayerForSimulcast(2, kBaseEncoderId, codec_params);

  // Destroy the top encoder.
  observer_->OnEncoderDestroyed(kBaseEncoderId + 2);
  ExpectTopLayerForSimulcast(1, kBaseEncoderId, codec_params);

  // Destroy the bottom encoder id.
  observer_->OnEncoderDestroyed(kBaseEncoderId);
  // The top encoder is still the middle one.
  ExpectTopLayerForSimulcast(1, kBaseEncoderId, codec_params);

  observer_->OnEncoderDestroyed(kBaseEncoderId + 1);
  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());
}

TEST_F(VideoEncoderStateObserverImplTest,
       FindHighestActiveEncoding_CreateAndDestroy_VP9kSVC_SingleEncoder) {
  constexpr int kEncoderId = 8;
  constexpr int kSpatialLayers = 3;
  constexpr int kTemporalLayers = 1;
  const auto vp9 = VP9kSVCVideoCodec(kSpatialLayers, kTemporalLayers);

  CreateObserver(media::VP9PROFILE_PROFILE0);
  observer_->OnEncoderCreated(kEncoderId, vp9);

  std::optional<VideoEncoderStateObserverImpl::TopLayerInfo> top_layer =
      observer_->FindHighestActiveEncoding();
  ASSERT_TRUE(top_layer.has_value());
  EXPECT_EQ(top_layer->encoder_id, kEncoderId);
  EXPECT_EQ(top_layer->spatial_id, kSpatialLayers - 1);
  EXPECT_EQ(top_layer->pixel_rate, PixelRate(vp9));

  observer_->OnEncoderDestroyed(kEncoderId);

  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());
}

TEST_F(VideoEncoderStateObserverImplTest,
       FindHighestActiveEncoding_ActivateLayers_VP9kSVC_SingleEncoder) {
  constexpr int kEncoderId = 8;
  constexpr int kSpatialLayers = 3;
  constexpr int kTemporalLayers = 3;
  const auto vp9 = VP9kSVCVideoCodec(kSpatialLayers, kTemporalLayers);
  const int kPixelRates[] = {
      PixelRate(vp9.spatialLayers[0]),
      PixelRate(vp9.spatialLayers[1]),
      PixelRate(vp9.spatialLayers[2]),
  };

  CreateObserver(media::VP9PROFILE_PROFILE0);

  observer_->OnEncoderCreated(kEncoderId, vp9);

  // Unchanged with all active layers.
  observer_->OnRatesUpdated(kEncoderId, {true, true, true});
  ExpectTopLayerForSVC(2, kEncoderId, kPixelRates);

  // Deactivate the top layer.
  observer_->OnRatesUpdated(kEncoderId, {true, true});
  ExpectTopLayerForSVC(1, kEncoderId, kPixelRates);

  // Deactivate the middle layer.
  observer_->OnRatesUpdated(kEncoderId, {true});
  ExpectTopLayerForSVC(0, kEncoderId, kPixelRates);

  // Activate the middle and top layer and deactivate the bottom layer.
  observer_->OnRatesUpdated(kEncoderId, {false, true, true});
  ExpectTopLayerForSVC(2, kEncoderId, kPixelRates);

  // Deactivate all the layers.
  observer_->OnRatesUpdated(kEncoderId, {});
  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());

  // Activate all the layers.
  observer_->OnRatesUpdated(kEncoderId, {true, true, true});
  ExpectTopLayerForSVC(2, kEncoderId, kPixelRates);

  // Deactivate all the layers.
  observer_->OnRatesUpdated(kEncoderId, {});
  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());

  observer_->OnEncoderDestroyed(kEncoderId);
  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());
}

TEST_F(VideoEncoderStateObserverImplTest,
       FindHighestActiveEncoding_ActivateLayers_VP8Simulcast_MultipleEncoders) {
  constexpr int kBaseEncoderId = 8;
  constexpr int kSimulcasts = 3;
  constexpr int kTemporalLayers = 3;
  const auto codec = VP8VideoCodec(kSimulcasts, kTemporalLayers);
  webrtc::VideoCodec codec_params[kSimulcasts];

  CreateObserver(media::VP8PROFILE_ANY);

  for (size_t stream_idx = 0; stream_idx < kSimulcasts; stream_idx++) {
    codec_params[stream_idx] =
        CreateStreamCodec(codec, stream_idx, stream_idx == kSimulcasts - 1);
    observer_->OnEncoderCreated(kBaseEncoderId + stream_idx,
                                codec_params[stream_idx]);
  }

  // Deactivate the bottom layer.
  observer_->OnRatesUpdated(kBaseEncoderId, {});
  ExpectTopLayerForSimulcast(2, kBaseEncoderId, codec_params);

  // Deactivate the top layer.
  observer_->OnRatesUpdated(kBaseEncoderId + 2, {});
  ExpectTopLayerForSimulcast(1, kBaseEncoderId, codec_params);

  // Activate the bottom layer.
  observer_->OnRatesUpdated(kBaseEncoderId, {true});
  ExpectTopLayerForSimulcast(1, kBaseEncoderId, codec_params);

  // Deactivate the bottom and middle layers, so that all layers activated.
  observer_->OnRatesUpdated(kBaseEncoderId, {false});
  observer_->OnRatesUpdated(kBaseEncoderId + 1, {false});
  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());

  // Activate the top layer.
  observer_->OnRatesUpdated(kBaseEncoderId + 2, {true});
  ExpectTopLayerForSimulcast(2, kBaseEncoderId, codec_params);

  // Destroy the top layer.
  observer_->OnEncoderDestroyed(kBaseEncoderId + 2);
  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());

  // Activate the bottom and middle layer.
  observer_->OnRatesUpdated(kBaseEncoderId, {true});
  observer_->OnRatesUpdated(kBaseEncoderId + 1, {true});
  ExpectTopLayerForSimulcast(1, kBaseEncoderId, codec_params);

  // Destroy the bottom layer.
  observer_->OnEncoderDestroyed(kBaseEncoderId);
  ExpectTopLayerForSimulcast(1, kBaseEncoderId, codec_params);

  // Destroy the middle layer.
  observer_->OnEncoderDestroyed(kBaseEncoderId + 1);
  EXPECT_FALSE(observer_->FindHighestActiveEncoding().has_value());
}

TEST_F(VideoEncoderStateObserverImplTest,
       OnEncodedImage_VP8Simulcast_SingleEncoder) {
  constexpr int kEncoderId = 8;
  constexpr int kSimulcasts = 3;
  constexpr int kTemporalLayers = 3;
  const auto vp8 = VP8VideoCodec(kSimulcasts, kTemporalLayers);

  CreateObserver(media::VP8PROFILE_ANY);
  observer_->OnEncoderCreated(kEncoderId, vp8);

  constexpr int kEncodeTimes = StatsCollector::kMinSamplesThreshold * 1.1;
  constexpr int kKeyFrameInterval = 40;
  for (size_t i = 0; i < kEncodeTimes; i++) {
    const uint32_t rtp_timestamp = 100 + i;
    const bool keyframe = i % kKeyFrameInterval == 0;
    observer_->OnEncode(kEncoderId, rtp_timestamp);
    for (size_t stream_idx = 0; stream_idx < kSimulcasts; stream_idx++) {
      observer_->OnEncodedImage(
          kEncoderId, EncodeResult{.width = vp8.width,
                                   .height = vp8.height,
                                   .keyframe = keyframe,
                                   .spatial_index = stream_idx,
                                   .rtp_timestamp = rtp_timestamp,
                                   .encode_end_time = base::TimeTicks::Now(),
                                   .is_hardware_accelerated = true});
    }
  }

  ASSERT_EQ(processing_stats_.size(), 1u);
  const auto& [stats_key, video_stats] = processing_stats_.front();
  EXPECT_EQ(stats_key.is_decode, false);
  EXPECT_EQ(stats_key.codec_profile, media::VP8PROFILE_ANY);
  EXPECT_EQ(stats_key.pixel_size, vp8.width * vp8.height);
  EXPECT_EQ(stats_key.hw_accelerated, true);

  constexpr int kKeyFrames =
      (StatsCollector::kMinSamplesThreshold + kKeyFrameInterval - 1) /
      kKeyFrameInterval;
  EXPECT_EQ(video_stats.frame_count, StatsCollector::kMinSamplesThreshold);
  // The first key frame is ignored.
  EXPECT_EQ(video_stats.key_frame_count, kKeyFrames - 1);
  EXPECT_EQ(video_stats.p99_processing_time_ms, 1u);
}

TEST_F(VideoEncoderStateObserverImplTest,
       OnEncodedImage_VP8Simulcast_MultipleEncoders) {
  constexpr int kBaseEncoderId = 8;
  constexpr int kSimulcasts = 3;
  constexpr int kTemporalLayers = 3;
  const auto codec = VP8VideoCodec(kSimulcasts, kTemporalLayers);

  CreateObserver(media::VP8PROFILE_ANY);

  webrtc::VideoCodec codec_params[kSimulcasts];
  for (size_t stream_idx = 0; stream_idx < kSimulcasts; stream_idx++) {
    codec_params[stream_idx] =
        CreateStreamCodec(codec, stream_idx, stream_idx == kSimulcasts - 1);
    observer_->OnEncoderCreated(kBaseEncoderId + stream_idx,
                                codec_params[stream_idx]);
  }

  constexpr int kEncodeTimes = StatsCollector::kMinSamplesThreshold * 1.1;
  constexpr int kKeyFrameInterval = 40;
  for (size_t i = 0; i < kEncodeTimes; i++) {
    const uint32_t rtp_timestamp = 100 + i;
    const bool keyframe = i % kKeyFrameInterval == 0;
    for (size_t stream_idx = 0; stream_idx < kSimulcasts; stream_idx++) {
      observer_->OnEncode(kBaseEncoderId + stream_idx, rtp_timestamp);
      observer_->OnEncodedImage(
          kBaseEncoderId + stream_idx,
          EncodeResult{.width = codec_params[stream_idx].width,
                       .height = codec_params[stream_idx].height,
                       .keyframe = keyframe,
                       .spatial_index = 0,
                       .rtp_timestamp = rtp_timestamp,
                       .encode_end_time = base::TimeTicks::Now(),
                       .is_hardware_accelerated = true});
    }
  }

  // No stats is recorded because multiple encoders run.
  EXPECT_EQ(processing_stats_.size(), 0u);

  // Destroy the encoders that encode top two streams.
  for (size_t stream_idx = 1; stream_idx < kSimulcasts; stream_idx++) {
    observer_->OnEncoderDestroyed(kBaseEncoderId + stream_idx);
  }

  // kCheckUpdateStatsCollectionInterval in
  // VideoEncoderStateObserverImpl::UpdateStatsCollection().
  // To activate stats collection.
  task_environment_.AdvanceClock(base::Seconds(5) + base::Milliseconds(10));

  // Encode() on the encoder for the lowest resolution stream.
  for (size_t i = kEncodeTimes; i < kEncodeTimes * 2; i++) {
    const bool keyframe = (i - kEncodeTimes) % kKeyFrameInterval == 0;
    const uint32_t rtp_timestamp = 100 + i;
    observer_->OnEncode(kBaseEncoderId, rtp_timestamp);
    observer_->OnEncodedImage(
        kBaseEncoderId, EncodeResult{.width = codec_params[0].width,
                                     .height = codec_params[0].height,
                                     .keyframe = keyframe,
                                     .spatial_index = 0,
                                     .rtp_timestamp = rtp_timestamp,
                                     .encode_end_time = base::TimeTicks::Now(),
                                     .is_hardware_accelerated = true});
  }

  EXPECT_EQ(processing_stats_.size(), 1u);
  const auto& [stats_key, video_stats] = processing_stats_.front();
  EXPECT_EQ(stats_key.is_decode, false);
  EXPECT_EQ(stats_key.codec_profile, media::VP8PROFILE_ANY);
  EXPECT_EQ(stats_key.pixel_size,
            codec_params[0].width * codec_params[0].height);
  EXPECT_EQ(stats_key.hw_accelerated, true);

  constexpr int kKeyFrames =
      (StatsCollector::kMinSamplesThreshold + kKeyFrameInterval - 1) /
      kKeyFrameInterval;
  EXPECT_EQ(video_stats.frame_count, StatsCollector::kMinSamplesThreshold);
  // The first key frame is ignored.
  EXPECT_EQ(video_stats.key_frame_count, kKeyFrames - 1);
  EXPECT_EQ(video_stats.p99_processing_time_ms, 1u);
}

TEST_F(VideoEncoderStateObserverImplTest,
       OnEncodedImage_VP9kSVC_SingleEncoder) {
  constexpr int kEncoderId = 8;
  constexpr int kSpatialLayers = 3;
  constexpr int kTemporalLayers = 1;
  const auto vp9 = VP9kSVCVideoCodec(kSpatialLayers, kTemporalLayers);

  CreateObserver(media::VP9PROFILE_PROFILE0);
  observer_->OnEncoderCreated(kEncoderId, vp9);

  constexpr int kEncodeTimes = StatsCollector::kMinSamplesThreshold * 1.1;
  constexpr int kKeyFrameInterval = 40;
  for (size_t i = 0; i < kEncodeTimes; i++) {
    const uint32_t rtp_timestamp = 100 + i;
    observer_->OnEncode(kEncoderId, rtp_timestamp);
    for (size_t sid = 0; sid < kSpatialLayers; sid++) {
      const bool keyframe = i % kKeyFrameInterval == 0 && sid == 0;
      observer_->OnEncodedImage(
          kEncoderId, EncodeResult{.width = vp9.spatialLayers[sid].width,
                                   .height = vp9.spatialLayers[sid].height,
                                   .keyframe = keyframe,
                                   .spatial_index = sid,
                                   .rtp_timestamp = rtp_timestamp,
                                   .encode_end_time = base::TimeTicks::Now(),
                                   .is_hardware_accelerated = true});
    }
  }

  EXPECT_EQ(processing_stats_.size(), 1u);
  const auto& [stats_key, video_stats] = processing_stats_.front();
  EXPECT_EQ(stats_key.is_decode, false);
  EXPECT_EQ(stats_key.codec_profile, media::VP9PROFILE_PROFILE0);
  EXPECT_EQ(stats_key.pixel_size, vp9.width * vp9.height);
  EXPECT_EQ(stats_key.hw_accelerated, true);

  EXPECT_EQ(video_stats.frame_count, StatsCollector::kMinSamplesThreshold);
  // No keyframe exists on the top spatial layer in k-SVC.
  EXPECT_EQ(video_stats.key_frame_count, 0);
  EXPECT_EQ(video_stats.p99_processing_time_ms, 1u);
}

TEST_F(VideoEncoderStateObserverImplTest,
       DynamicLayerChange_OnEncodedImage_VP9kSVC_SingleEncoder) {
  constexpr int kEncoderId = 8;
  constexpr int kSpatialLayers = 3;
  constexpr int kTemporalLayers = 1;
  const auto vp9 = VP9kSVCVideoCodec(kSpatialLayers, kTemporalLayers);

  CreateObserver(media::VP9PROFILE_PROFILE0);
  observer_->OnEncoderCreated(kEncoderId, vp9);

  const Vector<bool> active_layers_queries[] = {
      {true, false, false},  {false, false, true}, {false, true, true},
      {true, false, true},   {true, true, false},  {false, true, true},
      {false, false, false}, {true, true, true}};
  uint32_t rtp_timestamp = 100;
  size_t expected_processing_stats_size = 0;
  for (const Vector<bool>& active_layers : active_layers_queries) {
    observer_->OnRatesUpdated(kEncoderId, Vector<bool>(active_layers));
    auto [num_active_layers, bottom_sid, top_sid] =
        GetActiveIndexInfo(active_layers);
    if (num_active_layers == 0) {
      // No Encode() must be executed if no active layer exists.
      continue;
    }

    // kProcessingStatsReportingPeriod in stats_collector.cc.
    // To invoke ReportStats() for a regular period.
    task_environment_.AdvanceClock(base::Seconds(15) + base::Milliseconds(10));
    constexpr int kEncodeTimes = StatsCollector::kMinSamplesThreshold * 1.1;
    for (size_t i = 0; i < kEncodeTimes; i++) {
      rtp_timestamp++;
      observer_->OnEncode(kEncoderId, rtp_timestamp);
      for (size_t sid = 0; sid < kSpatialLayers; sid++) {
        if (!active_layers[sid]) {
          continue;
        }
        const bool keyframe = i == 0 && sid == bottom_sid;
        observer_->OnEncodedImage(
            kEncoderId, EncodeResult{.width = vp9.spatialLayers[sid].width,
                                     .height = vp9.spatialLayers[sid].height,
                                     .keyframe = keyframe,
                                     .spatial_index = sid,
                                     .rtp_timestamp = rtp_timestamp,
                                     .encode_end_time = base::TimeTicks::Now(),
                                     .is_hardware_accelerated = true});
      }
    }

    expected_processing_stats_size++;
    ASSERT_EQ(processing_stats_.size(), expected_processing_stats_size);
    const auto& [stats_key, video_stats] = processing_stats_.back();
    EXPECT_EQ(stats_key.is_decode, false);
    EXPECT_EQ(stats_key.codec_profile, media::VP9PROFILE_PROFILE0);
    EXPECT_EQ(stats_key.pixel_size, vp9.spatialLayers[top_sid].width *
                                        vp9.spatialLayers[top_sid].height);
    EXPECT_EQ(stats_key.hw_accelerated, true);

    EXPECT_EQ(video_stats.frame_count, StatsCollector::kMinSamplesThreshold);
    // The first key frame is ignored.
    EXPECT_EQ(video_stats.key_frame_count, 0);
    EXPECT_EQ(video_stats.p99_processing_time_ms, 1u);

    // Clear stats to not invoke ReportStats() on the next Encode().
    observer_->ClearStatsCollection();
  }
}
}  // namespace blink
