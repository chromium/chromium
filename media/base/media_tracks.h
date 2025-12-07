// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_TRACKS_H_
#define MEDIA_BASE_MEDIA_TRACKS_H_

#include <map>
#include <memory>
#include <vector>

#include "media/base/media_export.h"
#include "media/base/media_track.h"
#include "media/base/stream_parser.h"

namespace media {

class AudioDecoderConfig;
class VideoDecoderConfig;

class MEDIA_EXPORT MediaTracks {
 public:
  using MediaTracksCollection = std::vector<std::unique_ptr<MediaTrack>>;

  template <typename T>
  using ConfigMap = std::map<StreamParser::TrackId, T>;

  MediaTracks();

  MediaTracks(const MediaTracks&) = delete;
  MediaTracks& operator=(const MediaTracks&) = delete;

  ~MediaTracks();

  // Adds a new audio track. The |bytestreamTrackId| must uniquely identify the
  // track within the bytestream.
  MediaTrack* AddAudioTrack(const AudioDecoderConfig& config,
                            bool enabled,
                            StreamParser::TrackId stream_id,
                            const MediaTrack::Kind& kind,
                            const MediaTrack::Label& label,
                            const MediaTrack::Language& language,
                            bool exclusive = true);
  // Adds a new video track. The |bytestreamTrackId| must uniquely identify the
  // track within the bytestream.
  MediaTrack* AddVideoTrack(const VideoDecoderConfig& config,
                            bool enabled,
                            StreamParser::TrackId stream_id,
                            const MediaTrack::Kind& kind,
                            const MediaTrack::Label& label,
                            const MediaTrack::Language& language);

  const MediaTracksCollection& tracks() const { return tracks_; }

  const ConfigMap<AudioDecoderConfig>& GetAudioConfigs() const {
    return audio_configs_;
  }
  const ConfigMap<VideoDecoderConfig>& GetVideoConfigs() const {
    return video_configs_;
  }

  const AudioDecoderConfig& getAudioConfig(
      StreamParser::TrackId stream_id) const;
  const VideoDecoderConfig& getVideoConfig(
      StreamParser::TrackId stream_id) const;

 private:
  MediaTracksCollection tracks_;
  ConfigMap<AudioDecoderConfig> audio_configs_;
  ConfigMap<VideoDecoderConfig> video_configs_;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_TRACKS_H_
