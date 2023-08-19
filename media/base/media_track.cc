// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_track.h"

namespace media {

MediaTrack::MediaTrack(Type type,
                       StreamParser::TrackId bytestream_track_id,
                       const Kind& kind,
                       const Label& label,
                       const Language& lang)
    : type_(type),
      bytestream_track_id_(bytestream_track_id),
      kind_(kind),
      label_(label),
      language_(lang) {}

MediaTrack::~MediaTrack() = default;

const char* TrackTypeToStr(MediaTrack::Type type) {
  switch (type) {
    case MediaTrack::Type::kAudio:
      return "audio";
    case MediaTrack::Type::kText:
      return "text";
    case MediaTrack::Type::kVideo:
      return "video";
  }
  NOTREACHED_NORETURN();
}

}  // namespace media
