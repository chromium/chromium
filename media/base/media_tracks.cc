// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_tracks.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace media {

MediaTracks::MediaTracks() = default;

MediaTracks::~MediaTracks() = default;

MediaTrack* MediaTracks::AddAudioTrack(const AudioDecoderConfig& config,
                                       bool enabled,
                                       StreamParser::TrackId stream_id,
                                       const MediaTrack::Kind& kind,
                                       const MediaTrack::Label& label,
                                       const MediaTrack::Language& language,
                                       bool exclusive) {
  DCHECK(config.IsValidConfig());
  CHECK(audio_configs_.find(stream_id) == audio_configs_.end());
  auto track = base::WrapUnique(
      new MediaTrack(MediaTrack::Type::kAudio, stream_id, MediaTrack::Id{""},
                     kind, label, language, enabled, exclusive));
  MediaTrack* track_ptr = track.get();
  tracks_.push_back(std::move(track));
  audio_configs_[stream_id] = config;
  return track_ptr;
}

MediaTrack* MediaTracks::AddVideoTrack(const VideoDecoderConfig& config,
                                       bool enabled,
                                       StreamParser::TrackId stream_id,
                                       const MediaTrack::Kind& kind,
                                       const MediaTrack::Label& label,
                                       const MediaTrack::Language& language) {
  DCHECK(config.IsValidConfig());
  CHECK(video_configs_.find(stream_id) == video_configs_.end());
  auto track = base::WrapUnique(
      new MediaTrack(MediaTrack::Type::kVideo, stream_id, MediaTrack::Id{""},
                     kind, label, language, enabled, true));
  MediaTrack* track_ptr = track.get();
  tracks_.push_back(std::move(track));
  video_configs_[stream_id] = config;
  return track_ptr;
}

const AudioDecoderConfig& MediaTracks::getAudioConfig(
    StreamParser::TrackId stream_id) const {
  auto it = audio_configs_.find(stream_id);
  if (it != audio_configs_.end())
    return it->second;
  static base::NoDestructor<AudioDecoderConfig> invalidConfig;
  return *invalidConfig;
}

const VideoDecoderConfig& MediaTracks::getVideoConfig(
    StreamParser::TrackId stream_id) const {
  auto it = video_configs_.find(stream_id);
  if (it != video_configs_.end())
    return it->second;
  static base::NoDestructor<VideoDecoderConfig> invalidConfig;
  return *invalidConfig;
}

}  // namespace media
