// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_SECURITY_METADATA_H_
#define MEDIA_FORMATS_HLS_SECURITY_METADATA_H_

#include "base/containers/flat_set.h"
#include "media/base/media_export.h"
#include "url/origin.h"

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

  // Includes all the origins (after redirects) which are represented in the
  // data of this stream. If this data was encrypted, the key request origin
  // is also included.
  base::flat_set<url::Origin> response_origins;

  // A stream is never allowed to have a tainted origin and be a range
  // request.
  bool HasIncompatibleRangeAndOrigin() const {
    return would_taint_origin && has_range_request;
  }

  // "Safety" here means that there are no origins in `response_origins` that
  // differ from the manifest origin. This means a manifest hosted on
  // example.com can load media segments and encryption keys from example.com
  // even if example.com does not provide access-control-allow-origin headers
  // for the current frame's origin. The content will still be marked tainted
  // and the page will be unable to access it. On the other hand, a manifest
  // hosted on example.com would not be considered safe to load media content
  // from example.net, regardless of headers. Other security measures would
  // potentially allow this configuration if both example.com and example.net
  // serve data with the proper access-control-allow-origin headers for the top
  // frame's origin.
  bool IsSafeLoadFromManifestOrigin(const url::Origin&) const;

  // Merge in other metadata, keeping the most strict flag from each, and
  // combining the origins.
  void MergeFrom(const SecurityMetadata& other);

  // Note: the range request isn't flagged as part of testing, because that
  // information comes from the MediaSegment configurations used to construct
  // the stream.
  static SecurityMetadata CreateForTesting(std::string url,
                                           bool would_taint_origin = false,
                                           bool did_redirect = false);
  static SecurityMetadata CreateForTesting(const GURL& url,
                                           bool would_taint_origin = false,
                                           bool did_redirect = false);
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_SECURITY_METADATA_H_
