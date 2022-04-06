// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_H_
#define MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_H_

#include <vector>
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/formats/hls/playlist.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"

namespace media::hls {

class MediaSegment;

class MEDIA_EXPORT MediaPlaylist final : public Playlist {
 public:
  MediaPlaylist(const MediaPlaylist&) = delete;
  MediaPlaylist(MediaPlaylist&&);
  MediaPlaylist& operator=(const MediaPlaylist&) = delete;
  MediaPlaylist& operator=(MediaPlaylist&&);
  ~MediaPlaylist();

  // Returns all segments in this playlist, in chronological order. This vector
  // may be copied independently of this Playlist.
  const std::vector<MediaSegment>& GetSegments() const { return segments_; }

  // Returns the sum of the duration of all segments in this playlist.
  // Computed via the 'EXTINF' attribute, so may be slightly longer than the
  // actual duration.
  base::TimeDelta GetComputedDuration() const { return computed_duration_; }

  // Returns the type of this playlist (as specified by the
  // 'EXT-X-PLAYLIST-TYPE' tag). If this is present, then the server must follow
  // the constraints detailed on `PlaylistType` when the playlist is reloaded.
  // If this property is absent, that implies that the server may append new
  // segments to the end or remove old segments from the beginning of this
  // playlist upon reloading.
  absl::optional<PlaylistType> GetPlaylistType() const {
    return playlist_type_;
  }

  // Attempts to parse the playlist represented by `source`. `uri` must be a
  // valid, non-empty GURL referring to the URI of this playlist. If the
  // playlist is invalid, returns an error. Otherwise, returns the parsed
  // playlist.
  static ParseStatus::Or<MediaPlaylist> Parse(base::StringPiece source,
                                              GURL uri);

 private:
  MediaPlaylist(GURL uri,
                types::DecimalInteger version,
                bool independent_segments,
                std::vector<MediaSegment> segments,
                absl::optional<PlaylistType> playlist_type);

  std::vector<MediaSegment> segments_;
  base::TimeDelta computed_duration_;
  absl::optional<PlaylistType> playlist_type_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_H_
