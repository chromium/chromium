// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/stats_collecting_encoder.h"

#include <algorithm>
#include <atomic>

#include "base/check.h"
#include "base/logging.h"
#include "media/base/video_codecs.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"

namespace blink {
namespace {
// Limit data collection to when only a single encoder is active. This gives an
// optimistic estimate of the performance.
constexpr int kMaximumEncodersToCollectStats = 1;
constexpr base::TimeDelta kCheckSimultaneousEncodersInterval = base::Seconds(5);

// Number of StatsCollectingEncoder instances right now that have started
// encoding.
std::atomic_int* GetEncoderCounter() {
  static std::atomic_int s_counter(0);
  return &s_counter;
}
}  // namespace

StatsCollectingEncoder::StatsCollectingEncoder(
    const webrtc::SdpVideoFormat& format,
    std::unique_ptr<webrtc::VideoEncoder> encoder,
    StatsCollector::StoreProcessingStatsCB stats_callback)
    : StatsCollector(
          /*is_decode=*/false,
          WebRtcVideoFormatToMediaVideoCodecProfile(format),
          stats_callback),
      encoder_(std::move(encoder)) {
  DVLOG(3) << __func__;
  CHECK(encoder_);
  ClearStatsCollection();
  DETACH_FROM_SEQUENCE(encoding_sequence_checker_);
}

StatsCollectingEncoder::~StatsCollectingEncoder() {
  DVLOG(3) << __func__;
}

void StatsCollectingEncoder::SetFecControllerOverride(
    webrtc::FecControllerOverride* fec_controller_override) {
  encoder_->SetFecControllerOverride(fec_controller_override);
}

int StatsCollectingEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    const webrtc::VideoEncoder::Settings& settings) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoding_sequence_checker_);
  // In the case the underlying encoder is RTCVideoEncoder,
  // encoder_->InitEncode() doesn't return until any previously existing HW
  // encoder has been deleted and the new encoder is initialized.
  // `highest_observed_stream_index_` can therefore be safely accessed after
  // the call to encoder->InitEncode().
  int ret = encoder_->InitEncode(codec_settings, settings);
  // Reset to the default value.
  highest_observed_stream_index_ = 0;
  return ret;
}

int32_t StatsCollectingEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  DVLOG(3) << __func__;
  DCHECK(callback);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoding_sequence_checker_);
  encoded_callback_ = callback;
  return encoder_->RegisterEncodeCompleteCallback(this);
}

int32_t StatsCollectingEncoder::Release() {
  // Release is called after encode_sequence has been stopped.
  DVLOG(3) << __func__;
  int32_t ret = encoder_->Release();
  // There will be no new calls to Encoded() after the call to
  // encoder_->Release(). Any outstanding calls to Encoded() will also finish
  // before encoder_->Release() returns. It's therefore safe to access member
  // variables here.
  if (active_stats_collection() &&
      samples_collected() >= kMinSamplesThreshold) {
    ReportStats();
  }

  if (first_frame_encoded_) {
    --(*GetEncoderCounter());
    first_frame_encoded_ = false;
  }

  return ret;
}

int32_t StatsCollectingEncoder::Encode(
    const webrtc::VideoFrame& frame,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoding_sequence_checker_);
  if (!first_frame_encoded_) {
    first_frame_encoded_ = true;
    ++(*GetEncoderCounter());
  }

  base::TimeTicks now = base::TimeTicks::Now();
  {
    // Store the timestamp.
    base::AutoLock auto_lock(lock_);
    constexpr size_t kMaxEncodeStartInfoSize = 10;
    // If encode_start_info_.size() increases it means that stats collection is
    // not active in the OnEncodedImage() callback. Pop the oldest element here
    // to keep the encode_start_info_ current in case stats collection begins.
    if (encode_start_info_.size() > kMaxEncodeStartInfoSize) {
      encode_start_info_.pop_front();
    }
    encode_start_info_.push_back(EncodeStartInfo{frame.rtp_timestamp(), now});
  }
  return encoder_->Encode(frame, frame_types);
}

void StatsCollectingEncoder::SetRates(const RateControlParameters& parameters) {
  encoder_->SetRates(parameters);
}

void StatsCollectingEncoder::OnPacketLossRateUpdate(float packet_loss_rate) {
  encoder_->OnPacketLossRateUpdate(packet_loss_rate);
}

void StatsCollectingEncoder::OnRttUpdate(int64_t rtt_ms) {
  encoder_->OnRttUpdate(rtt_ms);
}

void StatsCollectingEncoder::OnLossNotification(
    const LossNotification& loss_notification) {
  encoder_->OnLossNotification(loss_notification);
}

webrtc::VideoEncoder::EncoderInfo StatsCollectingEncoder::GetEncoderInfo()
    const {
  return encoder_->GetEncoderInfo();
}

webrtc::EncodedImageCallback::Result StatsCollectingEncoder::OnEncodedImage(
    const webrtc::EncodedImage& encoded_image,
    const webrtc::CodecSpecificInfo* codec_specific_info) {
  // OnEncodedImage may be called on either the encoding sequence (SW encoding)
  // or gpu sequence (HW encoding). However, these calls are not happening at
  // the same time. If there's a fallback from SW encoding to HW encoding, a
  // call to HW encoder->Release() ensures that any potential callbacks on the
  // gpu sequence are finished before the encoding continues on the encoding
  // sequence.
  DCHECK(encoded_callback_);
  webrtc::EncodedImageCallback::Result result =
      encoded_callback_->OnEncodedImage(encoded_image, codec_specific_info);

  const size_t encoded_image_stream_index =
      encoded_image.SimulcastIndex().value_or(
          encoded_image.SpatialIndex().value_or(0));
  highest_observed_stream_index_ =
      std::max(highest_observed_stream_index_, encoded_image_stream_index);

  if (stats_collection_finished() ||
      encoded_image_stream_index != highest_observed_stream_index_) {
    // Return early if we've already finished the stats collection or if this is
    // a lower stream layer. We only do stats collection for the highest
    // observed stream layer.
    return result;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  // Verify that there's only a single encoder when data collection is taking
  // place.
  if ((now - last_check_for_simultaneous_encoders_) >
      kCheckSimultaneousEncodersInterval) {
    last_check_for_simultaneous_encoders_ = now;
    DVLOG(3) << "Simultaneous encoders: " << *GetEncoderCounter();
    if (active_stats_collection()) {
      if (*GetEncoderCounter() > kMaximumEncodersToCollectStats) {
        // Too many encoders, cancel stats collection.
        ClearStatsCollection();
      }
    } else if (*GetEncoderCounter() <= kMaximumEncodersToCollectStats) {
      // Start up stats collection since there's only a single encoder active.
      StartStatsCollection();
    }
  }

  if (active_stats_collection()) {
    std::optional<base::TimeTicks> encode_start;
    {
      // Read out encode start timestamp if we can find a matching RTP
      // timestamp.
      base::AutoLock auto_lock(lock_);
      while (encode_start_info_.size() > 0 &&
             encode_start_info_.front().rtp_timestamp !=
                 encoded_image.RtpTimestamp()) {
        encode_start_info_.pop_front();
      }
      if (!encode_start_info_.empty())
        encode_start = encode_start_info_.front().encode_start;
    }

    if (encode_start) {
      float encode_time_ms = (now - *encode_start).InMillisecondsF();
      int pixel_size =
          encoded_image._encodedWidth * encoded_image._encodedHeight;
      bool is_hardware_accelerated =
          encoder_->GetEncoderInfo().is_hardware_accelerated;
      bool is_keyframe =
          encoded_image._frameType == webrtc::VideoFrameType::kVideoFrameKey;
      AddProcessingTime(pixel_size, is_hardware_accelerated, encode_time_ms,
                        is_keyframe, now);
    }
  }
  return result;
}

void StatsCollectingEncoder::OnDroppedFrame(DropReason reason) {
  DCHECK(encoded_callback_);
  encoded_callback_->OnDroppedFrame(reason);
}

}  // namespace blink
