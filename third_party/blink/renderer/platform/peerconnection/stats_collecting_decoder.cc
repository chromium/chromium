// Copyright 2022 The Chromium Authors
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
// Limit data collection to when only a single decoder is active. This gives an
// optimistic estimate of the performance.
constexpr int kMaximumDecodersToCollectStats = 1;
constexpr base::TimeDelta kCheckSimultaneousDecodersInterval = base::Seconds(5);

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
    StatsCollector::StoreProcessingStatsCB stats_callback)
    : decoder_(std::move(decoder)),
      stats_callback_(stats_callback),
      stats_collector_(
          /*is_decode=*/true,
          WebRtcVideoFormatToMediaVideoCodecProfile(format)) {
  DVLOG(3) << __func__;
  CHECK(decoder_);
  DETACH_FROM_SEQUENCE(decoding_sequence_checker_);
}

StatsCollectingDecoder::~StatsCollectingDecoder() {
  DVLOG(3) << __func__;
}

void StatsCollectingDecoder::ReportStats(
    const StatsCollector::Stats& stats) const {
  stats_callback_.Run(stats.key, stats.video_stats);
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
  {
    base::AutoLock auto_lock(lock_);
    if (!first_frame_decoded_) {
      first_frame_decoded_ = true;
      ++(*GetDecoderCounter());
    }
    number_of_new_keyframes_ += input_image.IsKey();
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

  std::optional<StatsCollector::Stats> stats_to_report;
  {
    base::AutoLock auto_lock(lock_);
    // There shouldn't be any new calls to Decoded() after the call to
    // decoder_->Release(). Any outstanding calls to Decoded() will typically
    // finish before decoder_->Release() returns. Use lock to be safe since this
    // is not guaranteed.
    if (stats_collector_.is_active() &&
        stats_collector_.samples_collected() >=
            StatsCollector::kMinSamplesThreshold) {
      stats_to_report = stats_collector_.ComputeVideoStats();
    }

    if (first_frame_decoded_) {
      --(*GetDecoderCounter());
      first_frame_decoded_ = false;
    }
  }
  if (stats_to_report) {
    ReportStats(*stats_to_report);
  }
  return ret;
}

webrtc::VideoDecoder::DecoderInfo StatsCollectingDecoder::GetDecoderInfo()
    const {
  return decoder_->GetDecoderInfo();
}

// Implementation of webrtc::DecodedImageCallback.
int32_t StatsCollectingDecoder::Decoded(webrtc::VideoFrame& decodedImage) {
  Decoded(decodedImage, std::nullopt, std::nullopt);
  return WEBRTC_VIDEO_CODEC_OK;
}

void StatsCollectingDecoder::Decoded(webrtc::VideoFrame& decodedImage,
                                     std::optional<int32_t> decode_time_ms,
                                     std::optional<uint8_t> qp) {
  // Decoded() may be called on either the decoding sequence (SW decoding) or
  // media sequence (HW decoding). While these calls typically do not happen at
  // the same time, hardware decoders can be unpredictable. If there is a
  // fallback from HW decoding to SW decoding, a call to HW decoder->Release()
  // is expected to ensure that any potential callbacks on the media sequence
  // are finished. However, in rare cases, a delayed "straggler" callback could
  // fire on the media sequence concurrently with the active decoding sequence.
  DCHECK(decoded_callback_);
  decoded_callback_->Decoded(decodedImage, decode_time_ms, qp);

  std::optional<StatsCollector::Stats> stats_to_report;
  {
    base::AutoLock auto_lock(lock_);
    if (stats_collector_.has_finished()) {
      // Return early if stats collection is already finished.
      return;
    }

    base::TimeTicks now = base::TimeTicks::Now();
    // Verify that there's only a single decoder when data collection is taking
    // place.
    if ((now - last_check_for_simultaneous_decoders_) >
        kCheckSimultaneousDecodersInterval) {
      last_check_for_simultaneous_decoders_ = now;
      DVLOG(3) << "Simultaneous decoders: " << *GetDecoderCounter();
      if (stats_collector_.is_active()) {
        if (*GetDecoderCounter() > kMaximumDecodersToCollectStats) {
          // Too many decoders, cancel stats collection.
          stats_collector_.Clear();
        }
      } else if (*GetDecoderCounter() <= kMaximumDecodersToCollectStats) {
        // Start up stats collection since there's only a single decoder active.
        stats_collector_.Start();
      }
    }

    if (stats_collector_.is_active() && decodedImage.processing_time()) {
      int pixel_size = static_cast<int>(decodedImage.size());
      bool is_hardware_accelerated =
          decoder_->GetDecoderInfo().is_hardware_accelerated;
      float processing_time_ms = decodedImage.processing_time()->Elapsed().ms();

      stats_to_report = stats_collector_.AddProcessingTimeAndGetStats(
          pixel_size, is_hardware_accelerated, processing_time_ms,
          number_of_new_keyframes_, now);
      number_of_new_keyframes_ = 0;
    }
  }
  if (stats_to_report) {
    ReportStats(*stats_to_report);
  }
}

}  // namespace blink
