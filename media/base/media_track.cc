// Copyright 2016 The Chromium Authors. All rights reserved.
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
    case MediaTrack::Audio:
      return "audio";
    case MediaTrack::Text:
      return "text";
    case MediaTrack::Video:
      return "video";
  }
  NOTREACHED();
  return "INVALID";
}

}  // namespace media
