// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_VOD_RENDITION_H_
#define MEDIA_FILTERS_HLS_VOD_RENDITION_H_

#include "base/moving_window.h"
#include "media/filters/hls_rendition.h"
#include "media/formats/hls/segment_stream.h"

namespace media {

class MEDIA_EXPORT HlsVodRendition : public HlsRendition {
 public:
  ~HlsVodRendition() override;

  // `ManifestDemuxerEngineHost` owns the `HlsRenditionHost` which in
  // turn owns |this|, so it's safe to keep these as raw ptrs. |host_| is needed
  // to access the chunk demuxer, and |engine_| is needed to make network
  // requests.
  HlsVodRendition(ManifestDemuxerEngineHost* engine_host,
                  HlsRenditionHost* rendition_host,
                  std::string role,
                  scoped_refptr<hls::MediaPlaylist> playlist,
                  base::TimeDelta duration);

  // `HlsRendition` implementation
  absl::optional<base::TimeDelta> GetDuration() override;
  void CheckState(base::TimeDelta media_time,
                  double playback_rate,
                  ManifestDemuxer::DelayCallback time_remaining_cb) override;
  ManifestDemuxer::SeekResponse Seek(base::TimeDelta seek_time) override;
  void StartWaitingForSeek() override;
  void Stop() override;

 private:
  // A pending segment consists of the stream from which network data is fetched
  // and the time to which the parser should run until.
  using PendingSegment =
      std::tuple<std::unique_ptr<HlsDataSourceStream>, base::TimeDelta>;

  // Clears old data and returns the amount of time taken to do so, in order to
  // aid the delay calculations.
  base::TimeDelta ClearOldSegments(base::TimeDelta media_time);
  void FetchNext(base::OnceClosure cb, base::TimeDelta required_time);

  // Continues loading from a stored pending network request.
  void FetchMoreDataFromPendingStream(base::OnceClosure cb,
                                      base::TimeDelta fetch_required_time);

  // Appends and parses data on network read. Will additionally set a pending
  // request if there is more to read.
  void OnSegmentData(base::OnceClosure cb,
                     base::TimeDelta fetch_required_time,
                     base::TimeDelta parse_end,
                     base::TimeTicks net_req_start,
                     HlsDataSourceProvider::ReadResult result);

  // `ManifestDemuxerEngineHost` owns the `HlsRenditionHost` which in
  // turn owns |this|, so it's safe to keep these as raw ptrs. |host_| is needed
  // to access the chunk demuxer, and |engine_| is needed to make network
  // requests.
  raw_ptr<ManifestDemuxerEngineHost> engine_host_;
  raw_ptr<HlsRenditionHost> rendition_host_;

  std::unique_ptr<hls::SegmentStream> segments_;

  // The chunk demuxer role for this rendition.
  std::string role_;

  // The parser offset timestamp for this stream.
  base::TimeDelta parse_offset_;

  // Total duration of the playback.
  base::TimeDelta duration_;

  // If this is set, then use it to get more data. Otherwise, fetch another
  // segment.
  absl::optional<PendingSegment> pending_stream_fetch_ = absl::nullopt;

  // Record the time it takes to download content.
  base::MovingAverage<base::TimeDelta, base::TimeDelta> fetch_time_;

  bool set_stream_end_ = false;

  bool is_stopped_for_shutdown_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HlsVodRendition> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_VOD_RENDITION_H_
