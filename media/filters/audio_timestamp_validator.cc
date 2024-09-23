// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_timestamp_validator.h"

#include <memory>

namespace media {

// Defines how many milliseconds of DecoderBuffer timestamp gap will be allowed
// before warning the user. See CheckForTimestampGap(). Value of 50 chosen, as
// this is low enough to catch issues early, but high enough to avoid noise for
// containers like WebM that default to low granularity timestamp precision.
const int kGapWarningThresholdMsec = 50;

// Limits the number of adjustments to |audio_ts_offset_| in order to reach a
// stable state where gaps between encoded timestamps match decoded output
// intervals. See CheckForTimestampGap().
const int kLimitTriesForStableTiming = 5;

// Limits the milliseconds of difference between expected and actual timestamps
// gaps to consider timestamp expectations "stable". 1 chosen because some
// containers (WebM) default to millisecond timestamp precision. See
// CheckForTimestampGap().
const int kStableTimeGapThrsholdMsec = 1;

// Maximum number of timestamp gap warnings sent to MediaLog.
const int kMaxTimestampGapWarnings = 10;

AudioTimestampValidator::AudioTimestampValidator(
    const AudioDecoderConfig& decoder_config,
    MediaLog* media_log)
    : has_codec_delay_(decoder_config.codec_delay() > 0),
      media_log_(media_log),
      audio_base_ts_(kNoTimestamp),
      reached_stable_state_(false),
      num_unstable_audio_tries_(0),
      limit_unstable_audio_tries_(kLimitTriesForStableTiming),
      drift_warning_threshold_msec_(kGapWarningThresholdMsec) {
  DCHECK(decoder_config.IsValidConfig());
}

AudioTimestampValidator::~AudioTimestampValidator() = default;

void AudioTimestampValidator::CheckForTimestampGap(
    const DecoderBuffer& buffer) {
  if (buffer.end_of_stream())
    return;
  DCHECK_NE(kNoTimestamp, buffer.timestamp());

  // If audio_base_ts_ == kNoTimestamp, we are processing our first buffer.
  // If stream has neither codec delay nor discard padding, we should expect
  // timestamps and output durations to line up from the start (i.e. be stable).
  if (audio_base_ts_ == kNoTimestamp && !has_codec_delay_ &&
      buffer.discard_padding().first == base::TimeDelta() &&
      buffer.discard_padding().second == base::TimeDelta()) {
    DVLOG(3) << __func__ << " Expecting stable timestamps - stream has neither "
             << "codec delay nor discard padding.";
    limit_unstable_audio_tries_ = 0;
  }

  // Don't continue checking timestamps if we've exhausted tries to reach stable
  // state. This suggests the media's encoded timestamps are way off.
  if (num_unstable_audio_tries_ > limit_unstable_audio_tries_)
    return;

  // Keep resetting encode base ts until we start getting decode output. Some
  // codecs/containers (e.g. chained Ogg) will take several encoded buffers
  // before producing the first decoded output.
  if (!audio_output_ts_helper_) {
    audio_base_ts_ = buffer.timestamp();
    DVLOG(3) << __func__
             << " setting audio_base:" << audio_base_ts_.InMicroseconds();
    return;
  }

  // If we have `audio_output_ts_helper_` we must have a base timestamp.
  DCHECK(audio_output_ts_helper_->base_timestamp());

  base::TimeDelta expected_ts = audio_output_ts_helper_->GetTimestamp();
  base::TimeDelta ts_delta = buffer.timestamp() - expected_ts;

  // Reconciling encoded buffer timestamps with decoded output often requires
  // adjusting expectations by some offset. This accounts for varied (and at
  // this point unknown) handling of front trimming and codec delay. Codec delay
  // and skip trimming may or may not be accounted for in the encoded timestamps
  // depending on the codec (e.g. MP3 vs Opus) and  demuxers used (e.g. FFmpeg
  // vs MSE stream parsers).
  if (!reached_stable_state_) {
    if (std::abs(ts_delta.InMilliseconds()) < kStableTimeGapThrsholdMsec) {
      reached_stable_state_ = true;
      DVLOG(3) << __func__ << " stabilized! tries:" << num_unstable_audio_tries_
               << " offset:"
               << audio_output_ts_helper_->base_timestamp()->InMicroseconds();
    } else {
      base::TimeDelta orig_offset = *audio_output_ts_helper_->base_timestamp();

      // Save since this gets reset when we set new base time.
      int64_t decoded_frame_count = audio_output_ts_helper_->frame_count();
      audio_output_ts_helper_->SetBaseTimestamp(orig_offset + ts_delta);
      audio_output_ts_helper_->AddFrames(decoded_frame_count);

      DVLOG(3) << __func__
               << " NOT stabilized. tries:" << num_unstable_audio_tries_
               << " offset was:" << orig_offset.InMicroseconds() << " now:"
               << audio_output_ts_helper_->base_timestamp()->InMicroseconds();
      num_unstable_audio_tries_++;

      // Let developers know if their files timestamps are way off from
      if (num_unstable_audio_tries_ > limit_unstable_audio_tries_) {
        MEDIA_LOG(WARNING, media_log_)
            << "Failed to reconcile encoded audio times with decoded output.";
      }
    }

    // Don't bother with further checking until we reach stable state.
    return;
  }

  if (std::abs(ts_delta.InMilliseconds()) > drift_warning_threshold_msec_) {
    LIMITED_MEDIA_LOG(WARNING, media_log_, num_timestamp_gap_warnings_,
                      kMaxTimestampGapWarnings)
        << " Large timestamp gap detected; may cause AV sync to drift."
        << " time:" << buffer.timestamp().InMicroseconds() << "us"
        << " expected:" << expected_ts.InMicroseconds() << "us"
        << " delta:" << ts_delta.InMicroseconds() << "us";
    // Increase threshold to avoid log spam but, let us know if gap widens.
    drift_warning_threshold_msec_ = std::abs(ts_delta.InMilliseconds());
  }
  DVLOG(3) << __func__ << " delta:" << ts_delta.InMicroseconds()
           << " expected_ts:" << expected_ts.InMicroseconds()
           << " actual_ts:" << buffer.timestamp().InMicroseconds()
           << " audio_ts_offset:"
           << audio_output_ts_helper_->base_timestamp()->InMicroseconds();
}

void AudioTimestampValidator::RecordOutputDuration(
    const AudioBuffer& audio_buffer) {
  if (!audio_output_ts_helper_) {
    DCHECK_NE(audio_base_ts_, kNoTimestamp);
    // SUBTLE: deliberately creating this with output buffer sample rate because
    // demuxer stream config is potentially stale for implicit AAC.
    audio_output_ts_helper_ =
        std::make_unique<AudioTimestampHelper>(audio_buffer.sample_rate());
    audio_output_ts_helper_->SetBaseTimestamp(audio_base_ts_);
  }

  DVLOG(3) << __func__ << " " << audio_buffer.frame_count() << " frames";
  audio_output_ts_helper_->AddFrames(audio_buffer.frame_count());
}

}  // namespace media
