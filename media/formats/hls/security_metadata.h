// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_SECURITY_METADATA_H_
#define MEDIA_FORMATS_HLS_SECURITY_METADATA_H_

#include "media/base/media_export.h"

namespace media::hls {

// A combined structure for the security metadata associated with any HLS
// network data. This includes redirections, origin tainting information, and
// more. It's combined into a single structure so that it can move easily
// between the streams and the parsed results of those streams like media,
// keys, and manifests.
struct MEDIA_EXPORT SecurityMetadata {
  // Once set to true, these flags must _never_ be set back to false.
  bool would_taint_origin = false;
  bool did_redirect = false;
  bool has_range_request = false;

  // A stream is never allowed to have a tainted origin and be a range
  // request.
  bool HasIncompatibleRangeAndOrigin() const {
    return would_taint_origin && has_range_request;
  }

  // Note: the range request isn't flagged as part of testing, because that
  // information comes from the MediaSegment configurations used to construct
  // the stream.
  static SecurityMetadata CreateForTesting(bool would_taint_origin = false,
                                           bool did_redirect = false);
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_SECURITY_METADATA_H_
