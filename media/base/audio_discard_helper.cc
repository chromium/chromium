// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_discard_helper.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/audio_buffer.h"

namespace media {

static void WarnOnNonMonotonicTimestamps(base::TimeDelta last_timestamp,
                                         base::TimeDelta current_timestamp) {
  if (last_timestamp == kNoTimestamp || last_timestamp <= current_timestamp)
    return;

  const base::TimeDelta diff = current_timestamp - last_timestamp;
  DLOG(WARNING) << "Input timestamps are not monotonically increasing! "
                << " ts " << current_timestamp.InMicroseconds() << " us"
                << " diff " << diff.InMicroseconds() << " us";
}

AudioDiscardHelper::AudioDiscardHelper(int sample_rate,
                                       size_t decoder_delay,
                                       bool delayed_discard)
    : sample_rate_(sample_rate),
      decoder_delay_(decoder_delay),
      timestamp_helper_(sample_rate_),
      discard_frames_(0),
      last_input_timestamp_(kNoTimestamp),
      delayed_discard_(delayed_discard),
      delayed_end_discard_(0) {
  DCHECK_GT(sample_rate_, 0);
}

AudioDiscardHelper::~AudioDiscardHelper() = default;

size_t AudioDiscardHelper::TimeDeltaToFrames(base::TimeDelta duration) const {
  DCHECK(duration >= base::TimeDelta());
  return duration.InSecondsF() * sample_rate_ + 0.5;
}

void AudioDiscardHelper::Reset(size_t initial_discard) {
  discard_frames_ = initial_discard;
  last_input_timestamp_ = kNoTimestamp;
  timestamp_helper_.Reset();
  delayed_discard_padding_ = DecoderBuffer::DiscardPadding();
}

bool AudioDiscardHelper::ProcessBuffers(
    const DecoderBuffer::TimeInfo& time_info,
    AudioBuffer* decoded_buffer) {
  DCHECK(time_info.timestamp != kNoTimestamp);

  // Issue a debug warning when we see non-monotonic timestamps.  Only a warning
  // to allow chained OGG playback.
  WarnOnNonMonotonicTimestamps(last_input_timestamp_, time_info.timestamp);
  last_input_timestamp_ = time_info.timestamp;

  // If this is the first buffer seen, setup the timestamp helper.
  if (!initialized()) {
    // Clamp the base timestamp to zero.
    timestamp_helper_.SetBaseTimestamp(
        std::max(base::TimeDelta(), time_info.timestamp));
  }
  DCHECK(initialized());

  if (!decoded_buffer) {
    // If there's a one buffer delay for decoding, we need to save it so it can
    // be processed with the next decoder buffer.
    if (delayed_discard_)
      delayed_discard_padding_ = time_info.discard_padding;
    return false;
  }

  const size_t original_frame_count = decoded_buffer->frame_count();

  // If there's a one buffer delay for decoding, pick up the last encoded
  // buffer's discard padding for processing with the current decoded buffer.
  DecoderBuffer::DiscardPadding current_discard_padding =
      time_info.discard_padding;
  if (delayed_discard_) {
    // For simplicity disallow cases where decoder delay is present with delayed
    // discard (no codecs at present).  Doing so allows us to avoid complexity
    // around endpoint tracking when handling complete buffer discards.
    DCHECK_EQ(decoder_delay_, 0u);
    std::swap(current_discard_padding, delayed_discard_padding_);
  }

  if (discard_frames_ > 0) {
    const size_t decoded_frames = decoded_buffer->frame_count();
    const size_t frames_to_discard = std::min(discard_frames_, decoded_frames);
    discard_frames_ -= frames_to_discard;

    DVLOG(1) << "Initial discard of " << frames_to_discard << " out of "
             << decoded_frames << " frames.";

    // If everything would be discarded, indicate a new buffer is required.
    if (frames_to_discard == decoded_frames) {
      // For simplicity, we just drop any discard padding if |discard_frames_|
      // consumes the entire buffer.
      return false;
    }

    decoded_buffer->TrimStart(frames_to_discard);
  }

  // Process any delayed end discard from the previous buffer.
  if (delayed_end_discard_ > 0) {
    DCHECK_GT(decoder_delay_, 0u);

    const size_t discard_index = decoder_delay_ - delayed_end_discard_;
    DCHECK_LT(discard_index, decoder_delay_);

    const size_t decoded_frames = decoded_buffer->frame_count();
    DCHECK_LT(delayed_end_discard_, decoded_frames);

    DVLOG(1) << "Delayed end discard of " << delayed_end_discard_ << " out of "
             << decoded_frames << " frames starting at " << discard_index;

    decoded_buffer->TrimRange(discard_index,
                              discard_index + delayed_end_discard_);
    delayed_end_discard_ = 0;
  }

  // Handle front discard padding.
  if (current_discard_padding.first.is_positive()) {
    const size_t decoded_frames = decoded_buffer->frame_count();

    // If a complete buffer discard is requested and there's no decoder delay,
    // just discard all remaining frames from this buffer.  With decoder delay
    // we have to estimate the correct number of frames to discard based on the
    // duration of the encoded buffer.
    const size_t start_frames_to_discard =
        current_discard_padding.first == kInfiniteDuration
            ? (decoder_delay_ > 0 ? TimeDeltaToFrames(time_info.duration)
                                  : decoded_frames)
            : TimeDeltaToFrames(current_discard_padding.first);

    // Regardless of the timestamp on the encoded buffer, the corresponding
    // decoded output will appear |decoder_delay_| frames later.
    size_t discard_start = decoder_delay_;
    if (decoder_delay_ > 0) {
      // If we have a |decoder_delay_| and have already discarded frames from
      // this buffer, the |discard_start| must be adjusted by the number of
      // frames already discarded.
      const size_t frames_discarded_so_far =
          original_frame_count - decoded_buffer->frame_count();
      CHECK_LE(frames_discarded_so_far, decoder_delay_);
      discard_start -= frames_discarded_so_far;
    }

    // For simplicity require the start of the discard to be within the current
    // buffer.  Doing so allows us avoid complexity around tracking discards
    // across buffers.
    if (discard_start >= decoded_frames) {
      DLOG(ERROR)
          << "Unsupported discard padding and decoder delay mix. Due to "
             "decoder delay, discard padding indicates data beyond the current "
             "buffer should be discarded. This is not supported.";
      return false;
    }

    const size_t frames_to_discard =
        std::min(start_frames_to_discard, decoded_frames - discard_start);

    // Carry over any frames which need to be discarded from the front of the
    // next buffer.
    DCHECK(!discard_frames_);
    discard_frames_ = start_frames_to_discard - frames_to_discard;

    DVLOG(1) << "Front discard of " << frames_to_discard << " out of "
             << decoded_frames << " frames starting at " << discard_start;

    // If everything would be discarded, indicate a new buffer is required.
    if (frames_to_discard == decoded_frames) {
      // The buffer should not have been marked with end discard if the front
      // discard removes everything, though incorrect or imprecise duration
      // metadata, combined with various trimming operations, might still have
      // end discard marked here. For simplicity, we do not carry over any such
      // end discard for handling later.
      return false;
    }

    decoded_buffer->TrimRange(discard_start, discard_start + frames_to_discard);
  } else {
    DCHECK(current_discard_padding.first.is_zero());
  }

  // Handle end discard padding.
  if (current_discard_padding.second.is_positive()) {
    const size_t decoded_frames = decoded_buffer->frame_count();
    size_t end_frames_to_discard =
        TimeDeltaToFrames(current_discard_padding.second);

    if (decoder_delay_) {
      // Delayed end discard only works if the decoder delay is less than a
      // single buffer.
      if (decoder_delay_ >= original_frame_count) {
        DLOG(ERROR) << "Encountered invalid discard padding value.";
        return false;
      }

      // If the discard is >= the decoder delay, trim everything we can off the
      // end of this buffer and the rest from the start of the next.
      if (end_frames_to_discard >= decoder_delay_) {
        DCHECK(!discard_frames_);
        discard_frames_ = decoder_delay_;
        end_frames_to_discard -= decoder_delay_;
      } else {
        DCHECK(!delayed_end_discard_);
        std::swap(delayed_end_discard_, end_frames_to_discard);
      }
    }

    if (end_frames_to_discard > decoded_frames) {
      DLOG(ERROR) << "Encountered invalid discard padding value.";
      return false;
    }

    if (end_frames_to_discard > 0) {
      DVLOG(1) << "End discard of " << end_frames_to_discard << " out of "
               << decoded_frames;

      // If everything would be discarded, indicate a new buffer is required.
      if (end_frames_to_discard == decoded_frames)
        return false;

      decoded_buffer->TrimEnd(end_frames_to_discard);
    }
  } else {
    DCHECK(current_discard_padding.second.is_zero());
  }

  DVLOG(3) << __func__ << " ts: " << timestamp_helper_.GetTimestamp()
           << " frames: " << decoded_buffer->frame_count();

  // Assign timestamp to the buffer.
  decoded_buffer->set_timestamp(timestamp_helper_.GetTimestamp());
  timestamp_helper_.AddFrames(decoded_buffer->frame_count());
  return true;
}

}  // namespace media
