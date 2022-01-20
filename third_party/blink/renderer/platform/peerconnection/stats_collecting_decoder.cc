// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/stats_collecting_decoder.h"

#include <algorithm>
#include <atomic>

#include "base/check.h"
#include "base/logging.h"
#include "media/base/video_codecs.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"

namespace blink {
namespace {
// Histogram parameters.
constexpr float kDecodeTimeHistogramMinValue_ms = 1.0;
constexpr float kDecodeTimeHistogramMaxValue_ms = 35;
constexpr size_t kDecodeTimeHistogramBuckets = 80;
constexpr float kDecodeTimePercentileToReport = 0.99;

// Limit data collection to when only a single decoder is active. This gives an
// optimistic estimate of the performance.
constexpr int kMaximumDecodersToCollectStats = 1;
constexpr base::TimeDelta kCheckSimultaneousDecodersInterval = base::Seconds(5);

// Only store data if at least 100 samples were collected. This is the minimum
// number of samples needed for the 99th percentile to be meaningful.
constexpr size_t kMinDecodeTimeSamplesThreshold = 100;
// Stop collecting data after 36000 samples (10 minutes at 60 fps).
constexpr int kMaxDecodeTimeSamplesThreshold = 10 * 60 * 60;
// Report intermediate results every 15 seconds.
constexpr base::TimeDelta kDecodeStatsReportingPeriod = base::Seconds(15);

// Number of StatsCollectingDecoder instances right now that have started
// decoding.
std::atomic_int* GetDecoderCounter() {
  static std::atomic_int s_counter(0);
  return &s_counter;
}
}  // namespace

StatsCollectingDecoder::StatsCollectingDecoder(
    const webrtc::SdpVideoFormat& format,
    std::unique_ptr<webrtc::VideoDecoder> decoder,
    StatsCollectingDecoder::StoreProcessingStatsCB stats_callback)
    : codec_profile_(WebRtcVideoFormatToMediaVideoCodecProfile(format)),
      decoder_(std::move(decoder)),
      stats_callback_(stats_callback) {
  DVLOG(3) << __func__ << " (" << media::GetProfileName(codec_profile_) << ")";
  CHECK(decoder_);
  ClearStatsCollection();
  DETACH_FROM_SEQUENCE(decoding_sequence_checker_);
}

StatsCollectingDecoder::~StatsCollectingDecoder() {
  DVLOG(3) << __func__;
}

// Implementation of webrtc::VideoDecoder.
bool StatsCollectingDecoder::Configure(const Settings& settings) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  return decoder_->Configure(settings);
}

int32_t StatsCollectingDecoder::Decode(const webrtc::EncodedImage& input_image,
                                       bool missing_frames,
                                       int64_t render_time_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  if (!first_frame_decoded_) {
    first_frame_decoded_ = true;
    ++(*GetDecoderCounter());
  }
  {
    base::AutoLock auto_lock(lock_);
    number_of_new_keyframes_ +=
        input_image._frameType == webrtc::VideoFrameType::kVideoFrameKey;
  }
  return decoder_->Decode(input_image, missing_frames, render_time_ms);
}

int32_t StatsCollectingDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  decoded_callback_ = callback;
  return decoder_->RegisterDecodeCompleteCallback(this);
}

int32_t StatsCollectingDecoder::Release() {
  // Release is called after decode_sequence has been stopped.
  DVLOG(3) << __func__;
  int32_t ret = decoder_->Release();

  // There will be no new calls to Decoded() after the call to
  // decoder_->Release(). Any outstanding calls to Decoded() will also finish
  // before decoder_->Release() returns. It's therefore safe to access member
  // variables here.
  if (decode_time_ms_histogram_ && decode_time_ms_histogram_->NumValues() >=
                                       kMinDecodeTimeSamplesThreshold) {
    ReportStats();
  }

  if (first_frame_decoded_) {
    --(*GetDecoderCounter());
  }

  return ret;
}

webrtc::VideoDecoder::DecoderInfo StatsCollectingDecoder::GetDecoderInfo()
    const {
  return decoder_->GetDecoderInfo();
}

// Implementation of webrtc::DecodedImageCallback.
int32_t StatsCollectingDecoder::Decoded(webrtc::VideoFrame& decodedImage) {
  Decoded(decodedImage, absl::nullopt, absl::nullopt);
  return WEBRTC_VIDEO_CODEC_OK;
}

void StatsCollectingDecoder::Decoded(webrtc::VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
  // Decoded may be called on either the decoding sequence (SW decoding) or
  // media sequence (HW decoding). However, these calls are not happening at the
  // same time. If there's a fallback from SW decoding to HW decoding, a call to
  // HW decoder->Release() ensures that any potential callbacks on the media
  // sequence are finished before the decoding continues on the decoding
  // sequence.
  DCHECK(decoded_callback_);
  decoded_callback_->Decoded(decodedImage, decode_time_ms, qp);
  if (stats_collection_finished_) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  // Verify that there's only a single decoder when data collection is taking
  // place.
  if ((now - last_check_for_simultaneous_decoders_) >
      kCheckSimultaneousDecodersInterval) {
    last_check_for_simultaneous_decoders_ = now;
    DVLOG(3) << "Simultaneous decoders: " << *GetDecoderCounter();
    if (decode_time_ms_histogram_) {
      if (*GetDecoderCounter() > kMaximumDecodersToCollectStats) {
        // Too many decoders, cancel stats collection.
        ClearStatsCollection();
      }
    } else if (*GetDecoderCounter() <= kMaximumDecodersToCollectStats) {
      // Start up stats collection since there's only a single decoder active.
      StartStatsCollection();
    }
  }

  {
    base::AutoLock auto_lock(lock_);
    number_of_keyframes_ += number_of_new_keyframes_;
    number_of_new_keyframes_ = 0;
  }

  if (decode_time_ms_histogram_ && decodedImage.processing_time()) {
    int pixel_size = static_cast<int>(decodedImage.size());
    bool is_hardware_accelerated =
        decoder_->GetDecoderInfo().is_hardware_accelerated;
    if (pixel_size == current_stats_key_.pixel_size &&
        is_hardware_accelerated == current_stats_key_.hw_accelerated) {
      // Store data.
      decode_time_ms_histogram_->Add(
          decodedImage.processing_time()->Elapsed().ms());
    } else {
      // New config, report data if enough samples have been collected,
      // otherwise just start over.
      if (decode_time_ms_histogram_->NumValues() >=
          kMinDecodeTimeSamplesThreshold) {
        ReportStats();
      }
      if (decode_time_ms_histogram_->NumValues() > 0) {
        // No need to start over unless some samples have been collected.
        StartStatsCollection();
      }
      current_stats_key_.pixel_size = pixel_size;
      current_stats_key_.hw_accelerated = is_hardware_accelerated;
    }

    // Report data regularly if enough samples have been collected.
    if (decode_time_ms_histogram_->NumValues() >=
            kMinDecodeTimeSamplesThreshold &&
        (now - last_report_) > kDecodeStatsReportingPeriod) {
      // Report intermediate values.
      last_report_ = now;
      ReportStats();

      if (decode_time_ms_histogram_->NumValues() >=
          kMaxDecodeTimeSamplesThreshold) {
        // Stop collecting more stats if we reach the max samples threshold.
        DVLOG(3) << "Enough samples collected, stop stats collection.";
        decode_time_ms_histogram_.reset();
        stats_collection_finished_ = true;
      }
    }
  }
}

void StatsCollectingDecoder::StartStatsCollection() {
  DVLOG(3) << __func__;
  decode_time_ms_histogram_ = std::make_unique<LinearHistogram>(
      kDecodeTimeHistogramMinValue_ms, kDecodeTimeHistogramMaxValue_ms,
      kDecodeTimeHistogramBuckets);
  last_report_ = base::TimeTicks();
}

void StatsCollectingDecoder::ClearStatsCollection() {
  DVLOG(3) << __func__;
  decode_time_ms_histogram_.reset();
  number_of_keyframes_ = 0;
  current_stats_key_ = {/*is_decode=*/true, codec_profile_, 0,
                        /*hw_accelerated=*/false};
}

void StatsCollectingDecoder::ReportStats() const {
  DCHECK(decode_time_ms_histogram_);
  StatsCollectingDecoder::VideoStats stats = {
      static_cast<int>(decode_time_ms_histogram_->NumValues()),
      static_cast<int>(number_of_keyframes_),
      decode_time_ms_histogram_->GetPercentile(kDecodeTimePercentileToReport)};
  DVLOG(3) << __func__ << "Pixel size: " << current_stats_key_.pixel_size
           << ", HW: " << current_stats_key_.hw_accelerated
           << ", P99: " << stats.p99_processing_time_ms
           << " ms, frames: " << stats.frame_count
           << ", key frames:: " << stats.key_frame_count;

  stats_callback_.Run(current_stats_key_, stats);
}

}  // namespace blink
