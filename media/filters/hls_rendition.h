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
class MEDIA_EXPORT HlsRenditionHost {
 public:
  virtual ~HlsRenditionHost() = 0;

  // Reads the entirety of an HLS manifest from `uri`, and posts the result back
  // through `cb`.
  virtual void ReadManifest(const GURL& uri,
                            HlsDataSourceProvider::ReadCb cb) = 0;

  // Reads media data from a media segment. If `read_chunked` is false, then
  // the resulting stream will be fully read until either EOS, or its optional
  // range is fully satisfied. If `read_chunked` is true, then only some data
  // will be present in the resulting stream, and more data can be requested
  // through the `ReadStream` method. If `include_init_segment` is true, then
  // the init segment data will be prepended to the buffer returned if this
  // segment has an initialization_segment.
  // TODO (crbug.com/1266991): Remove `read_chunked`, which should ideally
  // always be true for segments. HlsRenditionImpl needs to handle chunked reads
  // more effectively first.
  virtual void ReadMediaSegment(const hls::MediaSegment& segment,
                                bool read_chunked,
                                bool include_init_segment,
                                HlsDataSourceProvider::ReadCb cb) = 0;

  // Continue reading from a partially read stream.
  virtual void ReadStream(std::unique_ptr<HlsDataSourceStream> stream,
                          HlsDataSourceProvider::ReadCb cb) = 0;

  // Fetch a new playlist for live content at the requested URI.
  virtual void UpdateRenditionManifestUri(
      std::string role,
      GURL uri,
      base::OnceCallback<void(bool)> cb) = 0;

  // Used to set network speed (bits per second) for the adaptation selector.
  virtual void UpdateNetworkSpeed(uint64_t bps) = 0;

  // Notifies the rendition host that this rendition's ended state has changed.
  // When all renditions are ended, the rendition host can notify the engine
  // host as well.
  virtual void SetEndOfStream(bool ended);
};

class MEDIA_EXPORT HlsRendition {
 public:
  virtual ~HlsRendition() = default;

  // Checks the current playback time and starts any required network requests
  // for more data, or clears out old data.
  virtual void CheckState(base::TimeDelta media_time,
                          double playback_rate,
                          ManifestDemuxer::DelayCallback time_remaining_cb) = 0;

  // Does any necessary seeking work, and returns true iff more data is needed
  // as the seek was outside of a loaded range.
  virtual ManifestDemuxer::SeekResponse Seek(base::TimeDelta seek_time) = 0;

  // Lets the rendition know that any network requests which respond with an
  // aborted status are not to be treated as errors until the seek is finished.
  virtual void StartWaitingForSeek() = 0;

  // Live renditions should return a nullopt for duration.
  virtual std::optional<base::TimeDelta> GetDuration() = 0;

  // Stop the rendition, including canceling pending seeks. After stopping,
  // `CheckState` and `Seek` should be no-ops.
  virtual void Stop() = 0;

  // Update playlist because we've adapted to a network or resolution change.
  virtual void UpdatePlaylist(scoped_refptr<hls::MediaPlaylist> playlist,
                              std::optional<GURL> new_playlist_uri) = 0;

  static std::unique_ptr<HlsRendition> CreateRendition(
      ManifestDemuxerEngineHost* engine_host,
      HlsRenditionHost* rendition_host,
      std::string role,
      scoped_refptr<hls::MediaPlaylist> playlist,
      GURL uri);
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_RENDITION_H_
