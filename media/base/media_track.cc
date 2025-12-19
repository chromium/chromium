// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_track.h"

#include "base/notreached.h"

namespace media {

MediaTrack::~MediaTrack() = default;

const char* TrackTypeToStr(MediaTrack::Type type) {
  switch (type) {
    case MediaTrack::Type::kAudio:
      return "audio";
    case MediaTrack::Type::kVideo:
      return "video";
  }
  NOTREACHED();
}

// static
MediaTrack::Kind MediaTrack::VideoKindToString(MediaTrack::VideoKind kind) {
  switch (kind) {
    case MediaTrack::VideoKind::kAlternative:
      return MediaTrack::Kind{"alternative"};
    case MediaTrack::VideoKind::kCaptions:
      return MediaTrack::Kind{"captions"};
    case MediaTrack::VideoKind::kMain:
      return MediaTrack::Kind{"main"};
    case MediaTrack::VideoKind::kSign:
      return MediaTrack::Kind{"sign"};
    case MediaTrack::VideoKind::kSubtitles:
      return MediaTrack::Kind{"subtitles"};
    case MediaTrack::VideoKind::kCommentary:
      return MediaTrack::Kind{"commentary"};
    case MediaTrack::VideoKind::kNone:
      return MediaTrack::Kind{""};
  }
}

// static
MediaTrack::Kind MediaTrack::AudioKindToString(MediaTrack::AudioKind kind) {
  switch (kind) {
    case MediaTrack::AudioKind::kAlternative:
      return MediaTrack::Kind{"alternative"};
    case MediaTrack::AudioKind::kDescriptions:
      return MediaTrack::Kind{"descriptions"};
    case MediaTrack::AudioKind::kMain:
      return MediaTrack::Kind{"main"};
    case MediaTrack::AudioKind::kMainDescriptions:
      return MediaTrack::Kind{"main-desc"};
    case MediaTrack::AudioKind::kTranslation:
      return MediaTrack::Kind{"translation"};
    case MediaTrack::AudioKind::kCommentary:
      return MediaTrack::Kind{"commentary"};
    case MediaTrack::AudioKind::kNone:
      return MediaTrack::Kind{""};
  }
}

// static
MediaTrack MediaTrack::CreateVideoTrack(const std::string& id,
                                        VideoKind kind,
                                        const std::string& label,
                                        const std::string& language,
                                        bool enabled,
                                        StreamParser::TrackId stream_id) {
  return MediaTrack{MediaTrack::Type::kVideo,
                    stream_id,
                    MediaTrack::Id{id},
                    VideoKindToString(kind),
                    Label{label},
                    Language{language},
                    enabled,
                    true};
}

// static
MediaTrack MediaTrack::CreateAudioTrack(const std::string& id,
                                        AudioKind kind,
                                        const std::string& label,
                                        const std::string& language,
                                        bool enabled,
                                        StreamParser::TrackId stream_id,
                                        bool exclusive) {
  return MediaTrack{MediaTrack::Type::kAudio,
                    stream_id,
                    MediaTrack::Id{id},
                    AudioKindToString(kind),
                    Label{label},
                    Language{language},
                    enabled,
                    exclusive};
}

MediaTrack::MediaTrack(MediaTrack::Type type,
                       StreamParser::TrackId stream_id,
                       const MediaTrack::Id& track_id,
                       const MediaTrack::Kind& kind,
                       const MediaTrack::Label& label,
                       const MediaTrack::Language& language,
                       bool enabled,
                       bool exclusive)
    : type_(type),
      enabled_(enabled),
      exclusive_(exclusive),
      stream_id_(stream_id),
      track_id_(track_id),
      kind_(kind),
      label_(label),
      language_(language) {}

MediaTrack::MediaTrack(const MediaTrack& track)
    : type_(track.type()),
      enabled_(track.enabled()),
      exclusive_(track.exclusive()),
      stream_id_(track.stream_id()),
      track_id_(track.track_id()),
      kind_(track.kind()),
      label_(track.label()),
      language_(track.language()) {}

ForwardingTrackManager::~ForwardingTrackManager() = default;
ForwardingTrackManager::ForwardingTrackManager(
    base::RepeatingCallback<void(const MediaTrack&)> add,
    base::RepeatingCallback<void(const MediaTrack&)> remove,
    base::RepeatingCallback<void(const MediaTrack&, MediaTrack::State)> update)
    : add_(std::move(add)),
      remove_(std::move(remove)),
      update_(std::move(update)) {}

void ForwardingTrackManager::AddTrack(const MediaTrack& track) {
  add_.Run(track);
}

void ForwardingTrackManager::RemoveTrack(const MediaTrack& track) {
  remove_.Run(track);
}

void ForwardingTrackManager::SetTrackState(const MediaTrack& track,
                                           MediaTrack::State state) {
  update_.Run(track, state);
}

}  // namespace media
