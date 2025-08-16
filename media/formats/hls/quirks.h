// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_QUIRKS_H_
#define MEDIA_FORMATS_HLS_QUIRKS_H_

#include "media/base/media_export.h"

namespace media::hls {

struct MEDIA_EXPORT HLSQuirks {
  // datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.3
  // A Media Playlist tag MUST NOT appear in a Multivariant Playlist.
  // All other major implementations (safari, hls.js, shaka) do not consider
  // this restriction, and will play content that voilates it. This is
  // especially common with `EXT-X-ENDLIST` at the end of a multivariant
  // playlist.
  static constexpr bool AllowMediaTagsInMultivariantPlaylists() { return true; }

  // datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.6.1.1
  // All EXT-X-MEDIA tags in the same Group MUST have different NAME attributes.
  // All other major implementations (safari, hls.js, shaka) do not consider
  // this restriction, and play content that voilates it.
  static constexpr bool DeduplicateRenditionNamesInGroup() { return true; }

  // datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.6.1.1
  // A Group MUST NOT have more than one member with a DEFAULT attribute of YES.
  // Apple's own example content violates this requirement.
  static constexpr bool AllowMultipleDefaultRenditionsInGroup() { return true; }

  // datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.4.1
  // The format of the EXTINF tag is:
  // #EXTINF:<duration>,[<title>]
  // The trailing comma is not optional when the optional title is absent. All
  // other major implementations do not consider this restriction and play
  // content which is missing the trailing comma.
  static constexpr bool AllowMissingSegmentInfCommas() { return true; }

  // The spec says that the segment duration should not exceed the target
  // duration after rounding to the nearest integer.
  // https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.3.1
  // However, nearly 30% of all parser errors are caused by this error, so it
  // might make sense to allow segments which exceed this duration by small
  // numbers of seconds. NOTE: this may cause issues in playback, as the delays
  // in network fetching between segments is based on calculations on the target
  // duration.
  static constexpr int AllowExceedingTargetDurationBySeconds() { return 2; }
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_QUIRKS_H_
