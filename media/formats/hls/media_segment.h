// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MEDIA_SEGMENT_H_
#define MEDIA_FORMATS_HLS_MEDIA_SEGMENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

class MEDIA_EXPORT MediaSegment : public base::RefCounted<MediaSegment> {
 public:
  class MEDIA_EXPORT InitializationSegment
      : public base::RefCounted<InitializationSegment> {
   public:
    InitializationSegment(GURL uri, absl::optional<types::ByteRange>);
    InitializationSegment(const InitializationSegment& copy) = delete;
    InitializationSegment(InitializationSegment&& copy) = delete;
    InitializationSegment& operator=(const InitializationSegment& copy) =
        delete;
    InitializationSegment& operator=(InitializationSegment&& copy) = delete;

    // The URI, resolved against the playlist URI, of the resource containing
    // the initialization segment.
    const GURL& GetUri() const { return uri_; }

    // If the initialization segment is a subrange of its resource, this
    // indicates the range.
    absl::optional<types::ByteRange> GetByteRange() const {
      return byte_range_;
    }

   private:
    friend class base::RefCounted<InitializationSegment>;
    ~InitializationSegment();

    GURL uri_;
    absl::optional<types::ByteRange> byte_range_;
  };

  MediaSegment(base::TimeDelta duration,
               types::DecimalInteger media_sequence_number,
               types::DecimalInteger discontinuity_sequence_number,
               GURL uri,
               scoped_refptr<InitializationSegment> initialization_segment,
               absl::optional<types::ByteRange> byte_range,
               absl::optional<types::DecimalInteger> bitrate,
               bool has_discontinuity,
               bool is_gap);
  MediaSegment(const MediaSegment&) = delete;
  MediaSegment(MediaSegment&&) = delete;
  MediaSegment& operator=(const MediaSegment&) = delete;
  MediaSegment& operator=(MediaSegment&&) = delete;

  // The approximate duration of this media segment.
  base::TimeDelta GetDuration() const { return duration_; }

  // Returns the media sequence number of this media segment.
  types::DecimalInteger GetMediaSequenceNumber() const {
    return media_sequence_number_;
  }

  // Returns the discontinuity sequence number of this media segment.
  types::DecimalInteger GetDiscontinuitySequenceNumber() const {
    return discontinuity_sequence_number_;
  }

  // The URI of the media resource. This will have already been resolved against
  // the playlist URI. This is guaranteed to be valid and non-empty, unless
  // `gap` is true, in which case this URI should not be used.
  const GURL& GetUri() const { return uri_; }

  // Returns the initialization segment for this media segment, which may be
  // null if this segment has none. Subsequent media segments may also share the
  // same initialization segment.
  scoped_refptr<InitializationSegment> GetInitializationSegment() const {
    return initialization_segment_;
  }

  // If this media segment is a subrange of its resource, this indicates the
  // range.
  absl::optional<types::ByteRange> GetByteRange() const { return byte_range_; }

  // Whether there is a decoding discontinuity between the previous media
  // segment and this one.
  bool HasDiscontinuity() const { return has_discontinuity_; }

  // If this is `true`, it indicates that the resource for this media segment is
  // absent and the client should not attempt to fetch it.
  bool IsGap() const { return is_gap_; }

  // Returns the approximate bitrate of this segment (+-10%), expressed in
  // bits-per-second.
  absl::optional<types::DecimalInteger> GetBitRate() const { return bitrate_; }

 private:
  friend class base::RefCounted<MediaSegment>;
  ~MediaSegment();

  base::TimeDelta duration_;
  types::DecimalInteger media_sequence_number_;
  types::DecimalInteger discontinuity_sequence_number_;
  GURL uri_;
  scoped_refptr<InitializationSegment> initialization_segment_;
  absl::optional<types::ByteRange> byte_range_;
  absl::optional<types::DecimalInteger> bitrate_;
  bool has_discontinuity_;
  bool is_gap_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_MEDIA_SEGMENT_H_
