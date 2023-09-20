// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_RENDITION_H_
#define MEDIA_FILTERS_HLS_RENDITION_H_

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/ranges.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_demuxer_status.h"
#include "media/filters/manifest_demuxer.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/media_segment.h"

namespace media {

// Forward declare.
class ManifestDemuxerEngineHost;

// Interface for `HlsRendition` to make data requests to avoid having to own or
// create data sources.
class MEDIA_EXPORT HlsRenditionHost : public HlsDataSourceStreamManager {
 public:
  // Lets a rendition read URL data from `uri`. Usually this will be a chunked
  // read, but can be configured with `read_chunked`, since live video needs to
  // download full manifests. Additionally, some manifests can specify a custom
  // byte range, which can be forwarded as `range`.
  virtual void ReadFromUrl(GURL uri,
                           bool read_chunked,
                           absl::optional<hls::types::ByteRange> range,
                           HlsDataSourceStreamManager::ReadCb cb) = 0;

  virtual hls::ParseStatus::Or<scoped_refptr<hls::MediaPlaylist>>
  ParseMediaPlaylistFromStringSource(base::StringPiece source,
                                     GURL uri,
                                     hls::types::DecimalInteger version) = 0;
};

class MEDIA_EXPORT HlsRendition {
 public:
  virtual ~HlsRendition() {}

  // Checks the current playback time and starts any required network requests
  // for more data, or clears out old data.
  virtual void CheckState(base::TimeDelta media_time,
                          double playback_rate,
                          ManifestDemuxer::DelayCallback time_remaining_cb) = 0;

  // Does any necessary seeking work, and returns true iff more data is needed
  // as the seek was outside of a loaded range.
  virtual bool Seek(base::TimeDelta seek_time) = 0;

  // Cancels any outstanding pending network requests.
  virtual void CancelPendingNetworkRequests() = 0;

  // Live renditions should return a nullopt for duration.
  virtual absl::optional<base::TimeDelta> GetDuration() = 0;

  // Stop the rendition, including canceling pending seeks. After stopping,
  // `CheckState` and `Seek` should be no-ops.
  virtual void Stop() = 0;

  static HlsDemuxerStatus::Or<std::unique_ptr<HlsRendition>> CreateRendition(
      ManifestDemuxerEngineHost* engine_host,
      HlsRenditionHost* rendition_host,
      std::string role,
      scoped_refptr<hls::MediaPlaylist> playlist,
      GURL uri);
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_RENDITION_H_
