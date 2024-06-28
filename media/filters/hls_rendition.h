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
#include "media/filters/hls_network_access.h"
#include "media/filters/manifest_demuxer.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/media_segment.h"

namespace media {

// Forward declare.
class ManifestDemuxerEngineHost;

// An extension to the HlsNetworkAccess interface, with additional operations
// that the renditions must be able to apply to their host.
class MEDIA_EXPORT HlsRenditionHost : public HlsNetworkAccess {
 public:
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
