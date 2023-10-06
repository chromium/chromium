// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_LIVE_RENDITION_H_
#define MEDIA_FILTERS_HLS_LIVE_RENDITION_H_

#include <queue>

#include "base/containers/queue.h"
#include "base/moving_window.h"
#include "media/filters/hls_rendition.h"

namespace media {

// Represents the playback logic for livestreams.
class MEDIA_EXPORT HlsLiveRendition : public HlsRendition {
 public:
  ~HlsLiveRendition() override;
  HlsLiveRendition(ManifestDemuxerEngineHost* engine_host,
                   HlsRenditionHost* rendition_host,
                   std::string role,
                   scoped_refptr<hls::MediaPlaylist> playlist,
                   GURL media_playlist_uri);

  // `HlsRendition` implementation
  absl::optional<base::TimeDelta> GetDuration() override;
  void CheckState(base::TimeDelta media_time,
                  double playback_rate,
                  ManifestDemuxer::DelayCallback time_remaining_cb) override;
  ManifestDemuxer::SeekResponse Seek(base::TimeDelta seek_time) override;
  void StartWaitingForSeek() override;
  void Stop() override;

 private:
  base::TimeDelta GetForwardBufferSize() const;

  // Segment fetching and appending.
  void LoadSegment(const hls::MediaSegment& segment, base::OnceClosure cb);
  void FetchMoreDataFromPendingStream(base::OnceClosure cb);
  void OnSegmentData(base::OnceClosure cb,
                     base::TimeTicks net_req_start,
                     HlsDataSourceProvider::ReadResult result);

  // Manifest fetching.
  void AppendSegments(hls::MediaPlaylist* playlist);
  void MaybeFetchManifestUpdates(base::TimeDelta delay,
                                 ManifestDemuxer::DelayCallback cb);
  void FetchManifestUpdates(base::TimeDelta delay,
                            ManifestDemuxer::DelayCallback cb);
  void OnManifestUpdates(base::TimeTicks download_start_time,
                         base::TimeDelta delay_time,
                         ManifestDemuxer::DelayCallback cb,
                         HlsDataSourceProvider::ReadResult result);
  void ClearOldData(base::TimeDelta time);
  void ResetForPause();
  void ContinuePartialFetching(base::OnceClosure cb);

  // `ManifestDemuxerEngineHost` owns the `HlsRenditionHost` which in
  // turn owns |this|, so it's safe to keep these as raw ptrs. |host_| is needed
  // to access the chunk demuxer, and |engine_| is needed to make network
  // requests.
  raw_ptr<ManifestDemuxerEngineHost> engine_host_;
  raw_ptr<HlsRenditionHost> rendition_host_;

  // The chunk demuxer role for this rendition.
  std::string role_;

  // The current playlist. Needs to be updated periodically.
  GURL media_playlist_uri_;

  // A queue of segments.
  base::queue<scoped_refptr<hls::MediaSegment>> segments_;

  // The parser offset timestamp for this stream.
  base::TimeDelta parse_offset_;
  base::TimeDelta parse_end_ = base::Seconds(0);

  // Keep state for reloading playlists.
  base::TimeTicks last_download_time_;
  hls::types::DecimalInteger last_sequence_number_ = 0;
  hls::types::DecimalInteger first_sequence_number_ = 0;

  // Manifests should declare an upper limit on the length of segments.
  base::TimeDelta segment_duration_upper_limit_;

  // If this is set, then use it to get more data. Otherwise, fetch another
  // segment.
  std::unique_ptr<HlsDataSourceStream> partial_stream_;

  // Record the time it takes to download content.
  base::MovingAverage<base::TimeDelta, base::TimeDelta> fetch_time_{32};

  bool has_ever_played_ = false;
  bool require_seek_after_unpause_ = false;
  bool is_stopped_for_shutdown_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HlsLiveRendition> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_LIVE_RENDITION_H_
