// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer.h"

#include <algorithm>
#include <memory>

#include "base/time/time.h"

namespace media {

namespace {
// Force new MP4 blob at a maximum rate of 10 Hz.
constexpr base::TimeDelta kMinimumForcedBlobDuration = base::Seconds(1);
}  // namespace

Mp4Muxer::Mp4Muxer(AudioCodec audio_codec,
                   bool has_video,
                   bool has_audio,
                   Muxer::WriteDataCB write_data_callback)
    : has_video_(has_video), has_audio_(has_audio) {
  CHECK(has_video_ || has_audio_);
  CHECK(!has_audio || audio_codec == AudioCodec::kAAC);

  mp4_muxer_delegate_ =
      std::make_unique<Mp4MuxerDelegate>(std::move(write_data_callback));

  // Creation can be done on a different sequence than main activities.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Mp4Muxer::~Mp4Muxer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // There is no Stop command, instead the caller destroys the muxer
  // to finish the recording.
  Flush();
}

bool Mp4Muxer::OnEncodedVideo(
    const Muxer::VideoParameters& params,
    std::string encoded_data,
    std::string encoded_alpha,
    absl::optional<VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp,
    bool is_key_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(has_video_);
  CHECK_EQ(params.codec, VideoCodec::kH264);

  // TODO(crbug.com/1473492) Ensure params.color_space information is in
  // the `codec_description`.
  if (encoded_data.empty()) {
    return true;
  }

  DCHECK(!is_key_frame || codec_description.has_value());

  base::TimeTicks adjusted_timestamp = AdjustTimestamp(timestamp, false);

  mp4_muxer_delegate_->AddVideoFrame(params, encoded_data, codec_description,
                                     adjusted_timestamp, is_key_frame);
  MaybeForceFlush();
  return true;
}

bool Mp4Muxer::OnEncodedAudio(
    const AudioParameters& params,
    std::string encoded_data,
    absl::optional<AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(has_audio_);

  if (encoded_data.empty()) {
    return true;
  }

  // The first audio sample should have code description.
  DCHECK(latest_audio_timestamp_ != base::TimeTicks::Min() ||
         codec_description.has_value());

  base::TimeTicks adjusted_timestamp = AdjustTimestamp(timestamp, false);

  mp4_muxer_delegate_->AddAudioFrame(params, encoded_data, codec_description,
                                     adjusted_timestamp);
  MaybeForceFlush();
  return true;
}

void Mp4Muxer::SetMaximumDurationToForceDataOutput(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_data_output_interval_ = std::max(interval, kMinimumForcedBlobDuration);
}

void Mp4Muxer::MaybeForceFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It follows pattern of webm muxer where it does not respect
  // interval flush time unless video stream exists.
  if (!has_video_ || max_data_output_interval_.is_zero()) {
    return;
  }

  if (start_or_last_flushed_timestamp_.is_null()) {
    start_or_last_flushed_timestamp_ = base::TimeTicks::Now();
    return;
  }

  if (base::TimeTicks::Now() - start_or_last_flushed_timestamp_ >=
      max_data_output_interval_) {
    Flush();
  }
}

void Mp4Muxer::SetLiveAndEnabled(bool track_live_and_enabled, bool is_video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1476947): We don't use these ready/muted state of the track
  // like WebM yet.
}

void Mp4Muxer::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!elapsed_time_in_pause_) {
    elapsed_time_in_pause_.emplace();
  }
}

void Mp4Muxer::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (elapsed_time_in_pause_) {
    total_time_in_pause_ += elapsed_time_in_pause_->Elapsed();
    elapsed_time_in_pause_.reset();
  }
}

bool Mp4Muxer::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!mp4_muxer_delegate_->Flush()) {
    return false;
  }

  Reset();
  return true;
}

void Mp4Muxer::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  elapsed_time_in_pause_.reset();
  start_or_last_flushed_timestamp_ = base::TimeTicks();
}

base::TimeTicks Mp4Muxer::AdjustTimestamp(base::TimeTicks timestamp,
                                          bool audio) {
  // Subtract paused duration.
  base::TimeTicks timestamp_minus_paused = timestamp - total_time_in_pause_;

  // TODO(crbug.com/1475338) We need to ensure that the current out of order
  // algorithm is sufficient, otherwise we need to adjust the timestamps on
  // writer, use queue like WebM muxer or something else.
  base::TimeTicks& latest_timestamp =
      audio ? latest_audio_timestamp_ : latest_video_timestamp_;

  // Adjust timestamp if out of order arrival.
  latest_timestamp = std::max(latest_timestamp, timestamp_minus_paused);
  return latest_timestamp;
}

}  // namespace media
