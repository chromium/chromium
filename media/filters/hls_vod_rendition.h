// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_VOD_RENDITION_H_
#define MEDIA_FILTERS_HLS_VOD_RENDITION_H_

#include "media/base/moving_average.h"
#include "media/filters/hls_rendition.h"

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
  bool Seek(base::TimeDelta seek_time) override;
  void CancelPendingNetworkRequests() override;

 private:
  struct SegmentInfo {
    scoped_refptr<hls::MediaSegment> segment;

    // The absolute timestamps that this segment would be presented at.
    base::TimeDelta absolute_start;
    base::TimeDelta absolute_end;

    // What index in |segments_| this informational struct is stored.
    size_t index = 0;

    SegmentInfo();
    SegmentInfo(const SegmentInfo&);
    ~SegmentInfo();
  };

  struct PendingSegment {
    HlsDataSourceStream stream;
    size_t index = 0;
    PendingSegment(HlsDataSourceStream&& stream, size_t index);
    PendingSegment(const PendingSegment&) = delete;
    PendingSegment(PendingSegment&&) = default;
  };

  // Clears old data and returns the amount of time taken to do so, in order to
  // aid the delay calculations.
  base::TimeDelta ClearOldSegments(base::TimeDelta media_time);
  void FetchNext(base::OnceClosure cb, base::TimeDelta required_time);

  // Loads the given segment.
  void LoadSegment(SegmentInfo* segment,
                   base::TimeDelta fetch_required_time,
                   base::OnceClosure cb);
  void FetchMoreDataFromPendingStream(base::OnceClosure cb,
                                      base::TimeDelta fetch_required_time);
  void OnSegmentData(base::OnceClosure cb,
                     base::TimeDelta fetch_required_time,
                     size_t segment_index,
                     base::TimeTicks net_req_start,
                     HlsDataSourceStream::ReadResult result);

  // `ManifestDemuxerEngineHost` owns the `HlsRenditionHost` which in
  // turn owns |this|, so it's safe to keep these as raw ptrs. |host_| is needed
  // to access the chunk demuxer, and |engine_| is needed to make network
  // requests.
  raw_ptr<ManifestDemuxerEngineHost> engine_host_;
  raw_ptr<HlsRenditionHost> rendition_host_;

  // The chunk demuxer role for this rendition.
  std::string role_;

  // A list of all segments, which is used for seeking and calculating offsets.
  std::vector<SegmentInfo> segments_;

  // The parser offset timestamp for this stream.
  base::TimeDelta parse_offset_;

  // Manifests should declare an upper limit on the length of segments.
  base::TimeDelta segment_duration_upper_limit_;
  base::TimeDelta duration_;

  // If this is set, then use it to get more data. Otherwise, fetch another
  // segment.
  absl::optional<PendingSegment> pending_stream_fetch_ = absl::nullopt;

  // Record the time it takes to download content.
  MovingAverage fetch_time_;

  // Fetch segments in order always.
  std::vector<SegmentInfo>::iterator fetch_queue_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HlsVodRendition> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_VOD_RENDITION_H_
