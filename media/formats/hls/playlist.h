// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_PLAYLIST_H_
#define MEDIA_FORMATS_HLS_PLAYLIST_H_

#include "media/base/media_export.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

class MEDIA_EXPORT Playlist {
 public:
  Playlist(const Playlist&) = delete;
  Playlist& operator=(const Playlist&) = delete;

  // Returns the resolved URI of this playlist.
  const GURL& Uri() const { return uri_; }

  // Returns the HLS version number defined by the playlist. The default version
  // is `1`.
  types::DecimalInteger GetVersion() const { return version_; }

  // Indicates that all media samples in a Segment can be decoded without
  // information from other segments. It applies to every Media Segment in the
  // Playlist. If this is a Multivariant Playlist, it applies to every Media
  // Segment in every Media Playlist referenced by this playlist.
  bool AreSegmentsIndependent() const { return independent_segments_; }

 protected:
  Playlist(GURL uri, types::DecimalInteger version, bool independent_segments);
  Playlist(Playlist&&);
  Playlist& operator=(Playlist&&);
  ~Playlist();

 private:
  GURL uri_;
  types::DecimalInteger version_;
  bool independent_segments_;
};

}  // namespace media::hls

#endif
