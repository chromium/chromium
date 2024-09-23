// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_RENDITION_IMPL_H_
#define MEDIA_FILTERS_HLS_RENDITION_IMPL_H_

#include "crypto/encryptor.h"
#include "media/filters/hls_rendition.h"
#include "media/formats/hls/segment_stream.h"

namespace media {

class MEDIA_EXPORT HlsRenditionImpl : public HlsRendition {
 public:
  ~HlsRenditionImpl() override;

  // `ManifestDemuxerEngineHost` owns the `HlsRenditionHost` which in
  // turn owns |this|, so it's safe to keep these as raw ptrs. |host_| is needed
  // to access the chunk demuxer, and |engine_| is needed to make network
  // requests.
  HlsRenditionImpl(ManifestDemuxerEngineHost* engine_host,
                   HlsRenditionHost* rendition_host,
                   std::string role,
                   scoped_refptr<hls::MediaPlaylist> playlist,
                   std::optional<base::TimeDelta> duration,
                   GURL media_playlist_uri);

  // `HlsRendition` implementation
  std::optional<base::TimeDelta> GetDuration() override;
  void CheckState(base::TimeDelta media_time,
                  double playback_rate,
                  ManifestDemuxer::DelayCallback time_remaining_cb) override;
  ManifestDemuxer::SeekResponse Seek(base::TimeDelta seek_time) override;
  void StartWaitingForSeek() override;
  void Stop() override;
  void UpdatePlaylist(scoped_refptr<hls::MediaPlaylist> playlist,
                      std::optional<GURL> new_playlist_uri) override;

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
  void OnSegmentData(scoped_refptr<hls::MediaSegment> segment,
                     base::OnceClosure cb,
                     base::TimeDelta fetch_required_time,
                     base::TimeDelta parse_end,
                     base::TimeTicks net_req_start,
                     bool fetched_new_key,
                     HlsDataSourceProvider::ReadResult result);

  // This allows calculating the ideal buffer size, based on adaptability,
  // network speed, and playback type.
  base::TimeDelta GetIdealBufferSize() const;

  // A helper method for CheckState, which will fetch new manifests for live
  // content if needed, and also fetch the next segment for both live and vod
  // content.
  void TryFillingBuffers(ManifestDemuxer::DelayCallback delay,
                         base::TimeDelta media_time);

  // Live playback helpers which enforce section 6.3.4 of the HLS spec regarding
  // the delay between fetching new playlists for live content.
  void FetchManifestUpdates(ManifestDemuxer::DelayCallback, base::TimeDelta);
  void MaybeFetchManifestUpdates(ManifestDemuxer::DelayCallback,
                                 base::TimeDelta);

  // Callback helper to receive notice when a new manifest has been updated.
  void OnManifestUpdate(ManifestDemuxer::DelayCallback cb,
                        base::TimeDelta delay,
                        bool success);

  // Helper method to use duration to determine stream liveness.
  bool IsLive() const;

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
  std::optional<base::TimeDelta> duration_;

  // The URI of the active rendition's playlist.
  GURL media_playlist_uri_;

  // The last time the manifest was downloaded.
  base::TimeTicks last_download_time_;

  // The time that a livestream was paused at.
  std::optional<base::TimeTicks> livestream_pause_time_ = std::nullopt;

  // Decrypt full segments if using AES128 or AES256.
  std::unique_ptr<crypto::Encryptor> decryptor_;
  scoped_refptr<hls::MediaSegment> segment_with_key_;

  // toggleable bool flags.
  bool set_stream_end_ = false;
  bool is_stopped_for_shutdown_ = false;
  bool has_ever_played_ = false;
  bool requires_init_segment_ = true;

  std::optional<hls::types::DecimalInteger> last_discontinuity_sequence_num_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HlsRenditionImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_RENDITION_IMPL_H_
