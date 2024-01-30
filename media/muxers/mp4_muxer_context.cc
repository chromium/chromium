// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer_context.h"

#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/output_position_tracker.h"

namespace media {

Mp4MuxerContext::Mp4MuxerContext(
    std::unique_ptr<OutputPositionTracker> output_position_tracker)
    : output_position_tracker_(std::move(output_position_tracker)) {}

Mp4MuxerContext::~Mp4MuxerContext() = default;

// Track will be created and inserted to vector whatever arrives at
// Muxer.

void Mp4MuxerContext::SetVideoTrack(Track track) {
  CHECK(!video_track_.has_value());
  CHECK_NE(track.timescale, 0u);

  video_track_ = track;
}

void Mp4MuxerContext::SetAudioTrack(Track track) {
  CHECK(!audio_track_.has_value());
  CHECK_NE(track.timescale, 0u);

  audio_track_ = track;
}

std::optional<Mp4MuxerContext::Track> Mp4MuxerContext::GetVideoTrack() const {
  return video_track_;
}

std::optional<Mp4MuxerContext::Track> Mp4MuxerContext::GetAudioTrack() const {
  return audio_track_;
}

OutputPositionTracker& Mp4MuxerContext::GetOutputPositionTracker() const {
  return *output_position_tracker_;
}

}  // namespace media
