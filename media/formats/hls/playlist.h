// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_PLAYLIST_H_
#define MEDIA_FORMATS_HLS_PLAYLIST_H_

#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

class MEDIA_EXPORT Playlist : public base::RefCounted<Playlist> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  // Unless explicitly specified via the `EXT-X-VERSION` tag, the default
  // playlist version is `1`.
  static constexpr types::DecimalInteger kDefaultVersion = 1;

  // The min and max HLS version supported by this implementation.
  static constexpr types::DecimalInteger kMinSupportedVersion = 1;
  static constexpr types::DecimalInteger kMaxSupportedVersion = 10;

  enum class Kind {
    kMultivariantPlaylist,
    kMediaPlaylist,
  };

  struct Identification {
    // The playlist version. If none was specified this will be
    // `kDefaultVersion`.
    types::DecimalInteger version;

    // The playlist kind.
    Kind kind;
  };

  // Identifies the type and version of the given playlist.
  // This function does the minimum amount of parsing necessary to determine
  // these properties, so it is not a guarantee that this playlist is valid.
  static ParseStatus::Or<Identification> IdentifyPlaylist(std::string_view src);

  Playlist(const Playlist&) = delete;
  Playlist(Playlist&&) = delete;
  Playlist& operator=(const Playlist&) = delete;
  Playlist& operator=(Playlist&&) = delete;

  // Returns the resolved URI of this playlist.
  const GURL& Uri() const { return uri_; }

  // Returns the HLS version number defined by the playlist.
  types::DecimalInteger GetVersion() const { return version_; }

  // Indicates that all media samples in a Segment can be decoded without
  // information from other segments. It applies to every Media Segment in the
  // Playlist. If this is a Multivariant Playlist, it applies to every Media
  // Segment in every Media Playlist referenced by this playlist.
  bool AreSegmentsIndependent() const { return independent_segments_; }

  // Returns the kind of playlist this instance is.
  virtual Kind GetKind() const = 0;

 protected:
  Playlist(GURL uri, types::DecimalInteger version, bool independent_segments);

  friend base::RefCounted<Playlist>;
  virtual ~Playlist();

 private:
  GURL uri_;
  types::DecimalInteger version_;
  bool independent_segments_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_PLAYLIST_H_
