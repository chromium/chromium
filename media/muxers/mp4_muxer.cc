// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer.h"

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"

namespace media {

Mp4Muxer::Mp4Muxer(AudioCodec audio_codec,
                   bool has_video,
                   bool has_audio,
                   Muxer::WriteDataCB write_data_callback) {
  // Creation can be done on a different sequence than main activities.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Mp4Muxer::~Mp4Muxer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

bool Mp4Muxer::OnEncodedVideo(const Muxer::VideoParameters& params,
                              std::string encoded_data,
                              std::string encoded_alpha,
                              base::TimeTicks timestamp,
                              bool is_key_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();

  return true;
}

bool Mp4Muxer::OnEncodedAudio(
    const AudioParameters& params,
    std::string encoded_data,
    absl::optional<media::AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void Mp4Muxer::SetMaximumDurationToForceDataOutput(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void Mp4Muxer::SetLiveAndEnabled(bool track_live_and_enabled, bool is_video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void Mp4Muxer::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void Mp4Muxer::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

bool Mp4Muxer::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return true;
}

}  // namespace media
