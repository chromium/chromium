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
    StatsCollectingDecoder::StoreProcessingStatsCB stats_callback)
    : StatsCollector(
          /*is_decode=*/true,
          WebRtcVideoFormatToMediaVideoCodecProfile(format),
          stats_callback),
      decoder_(std::move(decoder)) {
  DVLOG(3) << __func__;
  CHECK(decoder_);
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
  if (active_stats_collection() &&
      samples_collected() >= kMinSamplesThreshold) {
    ReportStats();
  }

  if (first_frame_decoded_) {
    --(*GetDecoderCounter());
    first_frame_decoded_ = false;
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
  // Decoded may be called on either the decoding sequence (SW decoding) or
  // media sequence (HW decoding). However, these calls are not happening at the
  // same time. If there's a fallback from SW decoding to HW decoding, a call to
  // HW decoder->Release() ensures that any potential callbacks on the media
  // sequence are finished before the decoding continues on the decoding
  // sequence.
  DCHECK(decoded_callback_);
  decoded_callback_->Decoded(decodedImage, decode_time_ms, qp);
  if (stats_collection_finished()) {
    // Return early if we've already finished the stats collection.
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  // Verify that there's only a single decoder when data collection is taking
  // place.
  if ((now - last_check_for_simultaneous_decoders_) >
      kCheckSimultaneousDecodersInterval) {
    last_check_for_simultaneous_decoders_ = now;
    DVLOG(3) << "Simultaneous decoders: " << *GetDecoderCounter();
    if (active_stats_collection()) {
      if (*GetDecoderCounter() > kMaximumDecodersToCollectStats) {
        // Too many decoders, cancel stats collection.
        ClearStatsCollection();
      }
    } else if (*GetDecoderCounter() <= kMaximumDecodersToCollectStats) {
      // Start up stats collection since there's only a single decoder active.
      StartStatsCollection();
    }
  }

  // Read out number of new processed keyframes since last Decoded() callback.
  size_t number_of_new_keyframes = 0;
  {
    base::AutoLock auto_lock(lock_);
    number_of_new_keyframes += number_of_new_keyframes_;
    number_of_new_keyframes_ = 0;
  }

  if (active_stats_collection() && decodedImage.processing_time()) {
    int pixel_size = static_cast<int>(decodedImage.size());
    bool is_hardware_accelerated =
        decoder_->GetDecoderInfo().is_hardware_accelerated;
    float processing_time_ms = decodedImage.processing_time()->Elapsed().ms();

    AddProcessingTime(pixel_size, is_hardware_accelerated, processing_time_ms,
                      number_of_new_keyframes, now);
  }
}

}  // namespace blink
