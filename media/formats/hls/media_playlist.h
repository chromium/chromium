// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_H_
#define MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_H_

#include <vector>
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

class MediaSegment;

class MEDIA_EXPORT MediaPlaylist {
 public:
  MediaPlaylist(const MediaPlaylist&);
  MediaPlaylist(MediaPlaylist&&);
  MediaPlaylist& operator=(const MediaPlaylist&);
  MediaPlaylist& operator=(MediaPlaylist&&);
  ~MediaPlaylist();

  // Returns the resolved URI of this playlist.
  const GURL& Uri() const { return uri_; }

  // Returns the HLS version number defined by the playlist. The default version
  // is `1`.
  types::DecimalInteger GetVersion() const { return version_; }

  // Indicates that all media samples in a Segment can be decoded without
  // information from other segments. It applies to every Media Segment in the
  // Playlist.
  bool AreSegmentsIndependent() const { return independent_segments_; }

  // Returns all segments in this playlist, in chronological order. This vector
  // may be copied independently of this Playlist.
  const std::vector<MediaSegment>& GetSegments() const { return segments_; }

  // Returns the sum of the duration of all segments in this playlist.
  // Computed via the 'EXTINF' attribute, so may be slightly longer than the
  // actual duration.
  base::TimeDelta GetComputedDuration() const { return computed_duration_; }

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
                std::vector<MediaSegment> segments);

  GURL uri_;
  types::DecimalInteger version_;
  bool independent_segments_;
  std::vector<MediaSegment> segments_;
  base::TimeDelta computed_duration_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_H_
