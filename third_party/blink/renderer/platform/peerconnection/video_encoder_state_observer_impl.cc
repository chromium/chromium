// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/peerconnection/video_encoder_state_observer_impl.h"

#include <queue>

#include "base/atomic_ref_count.h"
#include "base/containers/contains.h"
#include "base/sequence_checker.h"
#include "third_party/webrtc/api/video/encoded_image.h"

namespace blink {
namespace {

std::atomic_int g_encoder_counter_{0};

}  // namespace

class VideoEncoderStateObserverImpl::EncoderState {
 public:
  struct CodecConfig {
    webrtc::VideoCodec codec;
    Vector<bool> active_spatial_layers;
  };
  struct EncodeStart {
    // RTIP timestamp that is the unique identifier for the frame to be encoded.
    uint32_t rtp_timestamp;
    // The actual time at which encoding of that frame started.
    base::TimeTicks time;
  };

  explicit EncoderState(const CodecConfig& codec_config)
      : codec_config_(codec_config) {}

  ~EncoderState() = default;

  bool FirstFrameEncodeCalled() const { return first_frame_encode_called_; }
  void MarkFirstFrameEncodeCalled() { first_frame_encode_called_ = true; }

  void SetActiveSpatialLayers(const Vector<bool>& active_spatial_layers) {
    codec_config_.active_spatial_layers = active_spatial_layers;
  }

  void AppendEncodeStart(uint32_t rtp_timestamp, base::TimeTicks time) {
    constexpr size_t kMaxEncodeStartQueueSize = 10;
    if (encode_starts_.size() > kMaxEncodeStartQueueSize) {
      encode_starts_.pop();
    }
    encode_starts_.push(EncodeStart{rtp_timestamp, time});
  }

  std::optional<EncodeStart> GetEncodeStart(uint32_t rtp_timestamp) {
    while (!encode_starts_.empty() &&
           encode_starts_.front().rtp_timestamp != rtp_timestamp) {
      encode_starts_.pop();
    }
    if (encode_starts_.empty()) {
      return std::nullopt;
    }
    return encode_starts_.front();
  }

  std::optional<VideoEncoderStateObserverImpl::TopLayerInfo> TopLayer() const {
    if (!codec_config_.active_spatial_layers.Contains(true)) {
      // No Active layers.
      return std::nullopt;
    }

    const webrtc::VideoCodec& codec = codec_config_.codec;
    int active_vec_size =
        base::saturated_cast<int>(codec_config_.active_spatial_layers.size());

    using TopLayerInfo = VideoEncoderStateObserverImpl::TopLayerInfo;
    std::optional<TopLayerInfo> top_layer;
    if (codec.codecType == webrtc::VideoCodecType::kVideoCodecVP9 &&
        codec.VP9().numberOfSpatialLayers > 0) {
      for (int i = 0; i < codec.VP9().numberOfSpatialLayers; ++i) {
        const webrtc::SpatialLayer& stream = codec.spatialLayers[i];
        int pixel_rate =
            (active_vec_size >= i + 1 && codec_config_.active_spatial_layers[i]
                 ? 1
                 : 0) *
            (stream.active ? 1 : 0) * stream.width * stream.height *
            base::checked_cast<int>(stream.maxFramerate);
        if (!top_layer || top_layer->pixel_rate <= pixel_rate) {
          top_layer = TopLayerInfo{
              .encoder_id = 0, .spatial_id = i, .pixel_rate = pixel_rate};
        }
      }
    } else {
      for (int i = 0; i < codec.numberOfSimulcastStreams; ++i) {
        const webrtc::SimulcastStream& stream = codec.simulcastStream[i];
        int pixel_rate =
            (active_vec_size >= i + 1 && codec_config_.active_spatial_layers[i]
                 ? 1
                 : 0) *
            (stream.active ? 1 : 0) * stream.width * stream.height *
            base::checked_cast<int>(stream.maxFramerate);
        if (!top_layer || top_layer->pixel_rate <= pixel_rate) {
          top_layer = TopLayerInfo{
              .encoder_id = 0, .spatial_id = i, .pixel_rate = pixel_rate};
        }
      }
    }
    if (!top_layer) {
      // No layering configured.
      top_layer = TopLayerInfo{
          .encoder_id = 0,
          .spatial_id = 0,
          .pixel_rate = codec.width * codec.height *
                        base::checked_cast<int>(codec.maxFramerate)};
    }
    return top_layer;
  }

 private:
  CodecConfig codec_config_;
  std::queue<EncodeStart> encode_starts_;
  bool first_frame_encode_called_ = false;
};

VideoEncoderStateObserverImpl::VideoEncoderStateObserverImpl(
    media::VideoCodecProfile profile,
    const StatsCollector::StoreProcessingStatsCB& store_processing_stats_cb)
    : StatsCollector(/*is_decode=*/false, profile, store_processing_stats_cb) {
  DETACH_FROM_SEQUENCE(encoder_sequence_);
}

VideoEncoderStateObserverImpl::~VideoEncoderStateObserverImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  for (const auto& kv : encoder_state_by_id_) {
    OnEncoderDestroyed(kv.first);
  }
}

void VideoEncoderStateObserverImpl::OnEncoderCreated(
    int encoder_id,
    const webrtc::VideoCodec& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  DCHECK(!base::Contains(encoder_state_by_id_, encoder_id));

  // Initially, assume all layers active.
  // TODO(hiroh): Set the number of layers to the currently configured layers?
  Vector<bool> active_spatial_layers(webrtc::kMaxSpatialLayers, true);

  CHECK(encoder_state_by_id_
            .insert_or_assign(
                encoder_id,
                std::make_unique<EncoderState>(EncoderState::CodecConfig{
                    config, std::move(active_spatial_layers)}))
            .second);
  top_encoder_info_ = FindHighestActiveEncoding();
}

void VideoEncoderStateObserverImpl::OnEncoderDestroyed(int encoder_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  EncoderState* encoder_state = GetEncoderState(encoder_id);
  if (!encoder_state) {
    return;
  }

  if (active_stats_collection() &&
      samples_collected() >= kMinSamplesThreshold) {
    ReportStats();
    ClearStatsCollection();
  }

  if (encoder_state->FirstFrameEncodeCalled()) {
    CHECK_GE(--g_encoder_counter_, 0);
  }

  CHECK_EQ(encoder_state_by_id_.erase(encoder_id), 1u);
  top_encoder_info_ = FindHighestActiveEncoding();
}

void VideoEncoderStateObserverImpl::OnRatesUpdated(
    int encoder_id,
    const Vector<bool>& active_spatial_layers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  EncoderState* encoder_state = GetEncoderState(encoder_id);
  if (!encoder_state) {
    return;
  }

  encoder_state->SetActiveSpatialLayers(active_spatial_layers);
  top_encoder_info_ = FindHighestActiveEncoding();
}

VideoEncoderStateObserverImpl::EncoderState*
VideoEncoderStateObserverImpl::GetEncoderState(int encoder_id,
                                               base::Location location) {
  auto it = encoder_state_by_id_.find(encoder_id);
  if (it == encoder_state_by_id_.end()) {
    LOG(WARNING) << "No encoder id: " << encoder_id << " ("
                 << location.function_name() << ")";
    return nullptr;
  }
  return it->second.get();
}

void VideoEncoderStateObserverImpl::OnEncode(int encoder_id,
                                             uint32_t rtp_timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  EncoderState* encoder_state = GetEncoderState(encoder_id);
  if (!encoder_state) {
    return;
  }
  if (!encoder_state->FirstFrameEncodeCalled()) {
    g_encoder_counter_++;
    encoder_state->MarkFirstFrameEncodeCalled();
    return;
  }
  encoder_state->AppendEncodeStart(rtp_timestamp, base::TimeTicks::Now());
}

void VideoEncoderStateObserverImpl::OnEncodedImage(int encoder_id,
                                                   const EncodeResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  if (!top_encoder_info_) {
    LOG(WARNING) << "Received encoded frame while no active encoder "
                    "configured, ignoring.";
    return;
  }
  if (encoder_id != top_encoder_info_->encoder_id ||
      result.spatial_index.value_or(0) != top_encoder_info_->spatial_id) {
    return;
  }

  if (stats_collection_finished()) {
    return;
  }

  // Frame from highest active encoder.
  auto now = base::TimeTicks::Now();
  EncoderState* encoder_state = GetEncoderState(encoder_id);
  if (!encoder_state) {
    return;
  }

  auto encode_start = encoder_state->GetEncodeStart(result.rtp_timestamp);
  if (!encode_start) {
    return;
  }

  UpdateStatsCollection(now);

  if (!active_stats_collection()) {
    return;
  }

  const float encode_time_ms =
      (result.encode_end_time - encode_start->time).InMillisecondsF();
  const int pixel_size = result.width * result.height;
  AddProcessingTime(pixel_size, result.is_hardware_accelerated, encode_time_ms,
                    result.keyframe, now);
}

void VideoEncoderStateObserverImpl::UpdateStatsCollection(base::TimeTicks now) {
  constexpr base::TimeDelta kCheckUpdateStatsCollectionInterval =
      base::Seconds(5);
  if ((now - last_update_stats_collection_time_) <
      kCheckUpdateStatsCollectionInterval) {
    return;
  }
  DVLOG(3) << "The number of simultaneous encoders: " << g_encoder_counter_;
  last_update_stats_collection_time_ = now;

  // Limit data collection to when only a single encoder is active. This gives
  // an optimistic estimate of the performance.
  constexpr int kMaximumEncodersToCollectStats = 1;
  if (active_stats_collection()) {
    if (g_encoder_counter_ > kMaximumEncodersToCollectStats) {
      // Too many encoders, cancel stats collection.
      ClearStatsCollection();
    }
  } else if (g_encoder_counter_ <= kMaximumEncodersToCollectStats) {
    // Start up stats collection since there's only a single encoder active.
    StartStatsCollection();
  }
}

std::optional<VideoEncoderStateObserverImpl::TopLayerInfo>
VideoEncoderStateObserverImpl::FindHighestActiveEncoding() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  std::optional<TopLayerInfo> top_info;
  for (const auto& kv : encoder_state_by_id_) {
    std::optional<TopLayerInfo> top_of_encoder = kv.second->TopLayer();
    if (top_of_encoder &&
        (!top_info || top_info->pixel_rate < top_of_encoder->pixel_rate)) {
      top_of_encoder->encoder_id = kv.first;
      top_info = top_of_encoder;
    }
  }

#if DCHECK_IS_ON()
  if (top_info && (!top_encoder_info_ ||
                   top_encoder_info_->encoder_id != top_info->encoder_id ||
                   top_encoder_info_->spatial_id != top_info->spatial_id ||
                   top_encoder_info_->pixel_rate != top_info->pixel_rate)) {
    DVLOG(3) << "New top resolution configured for video encoder: encoder id = "
             << top_info->encoder_id
             << ", spatial id = " << top_info->spatial_id
             << ", pixel rate = " << top_info->pixel_rate;
  }
#endif  // DCHECK_IS_ON()

  return top_info;
}
}  // namespace blink
