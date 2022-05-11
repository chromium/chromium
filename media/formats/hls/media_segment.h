// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MEDIA_SEGMENT_H_
#define MEDIA_FORMATS_HLS_MEDIA_SEGMENT_H_

#include "media/base/media_export.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

class MEDIA_EXPORT MediaSegment {
 public:
  MediaSegment(types::DecimalFloatingPoint duration,
               types::DecimalInteger media_sequence_number,
               GURL uri,
               bool has_discontinuity,
               bool is_gap);
  ~MediaSegment();
  MediaSegment(const MediaSegment&);
  MediaSegment(MediaSegment&&);
  MediaSegment& operator=(const MediaSegment&);
  MediaSegment& operator=(MediaSegment&&);

  // The approximate duration of this media segment in seconds.
  types::DecimalFloatingPoint GetDuration() const { return duration_; }

  // Returns the media sequence number of this media segment.
  types::DecimalInteger GetMediaSequenceNumber() const {
    return media_sequence_number_;
  }

  // The URI of the media resource. This will have already been resolved against
  // the playlist URI. This is guaranteed to be valid and non-empty, unless
  // `gap` is true, in which case this URI should not be used.
  const GURL& GetUri() const { return uri_; }

  // Whether there is a decoding discontinuity between the previous media
  // segment and this one.
  bool HasDiscontinuity() const { return has_discontinuity_; }

  // If this is `true`, it indicates that the resource for this media segment is
  // absent and the client should not attempt to fetch it.
  bool IsGap() const { return is_gap_; }

 private:
  types::DecimalFloatingPoint duration_;
  types::DecimalInteger media_sequence_number_;
  GURL uri_;
  bool has_discontinuity_;
  bool is_gap_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_MEDIA_SEGMENT_H_
