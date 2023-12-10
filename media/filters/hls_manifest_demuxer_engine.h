// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_
#define MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/filters/hls_codec_detector.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_demuxer_status.h"
#include "media/filters/hls_rendition.h"
#include "media/filters/manifest_demuxer.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/rendition_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// A HLS-Parser/Player implementation of ManifestDemuxer's Engine interface.
// This will use the HLS parsers and rendition selectors to fetch and parse
// playlists, followed by fetching and appending media segments.
class MEDIA_EXPORT HlsManifestDemuxerEngine : public ManifestDemuxer::Engine,
                                              public HlsRenditionHost {
 public:
  HlsManifestDemuxerEngine(base::SequenceBound<HlsDataSourceProvider> dsp,
                           scoped_refptr<base::SequencedTaskRunner> task_runner,
                           GURL root_playlist_uri,
                           MediaLog* media_log);
  ~HlsManifestDemuxerEngine() override;

  // ManifestDemuxer::Engine implementation
  std::string GetName() const override;
  void Initialize(ManifestDemuxerEngineHost* host,
                  PipelineStatusCallback status_cb) override;
  void OnTimeUpdate(base::TimeDelta time,
                    double playback_rate,
                    ManifestDemuxer::DelayCallback cb) override;
  void Seek(base::TimeDelta time, ManifestDemuxer::SeekCallback cb) override;
  void StartWaitingForSeek() override;
  void AbortPendingReads() override;
  bool IsSeekable() const override;
  int64_t GetMemoryUsage() const override;
  void Stop() override;

  // HlsRenditionHost implementation.
  void ReadFromUrl(GURL uri,
                   bool read_chunked,
                   absl::optional<hls::types::ByteRange> range,
                   HlsDataSourceProvider::ReadCb cb) override;
  void ReadStream(std::unique_ptr<HlsDataSourceStream> stream,
                  HlsDataSourceProvider::ReadCb cb) override;
  void UpdateNetworkSpeed(uint64_t bps) override;
  void UpdateRenditionManifestUri(std::string role,
                                  GURL uri,
                                  base::OnceClosure cb) override;

  // Test helpers.
  void AddRenditionForTesting(std::string role,
                              std::unique_ptr<HlsRendition> test_rendition);
  void InitializeWithMockCodecDetectorForTesting(
      ManifestDemuxerEngineHost* host,
      PipelineStatusCallback cb,
      std::unique_ptr<HlsCodecDetector> codec_detector);

 private:
  struct PlaylistParseInfo {
    PlaylistParseInfo(GURL uri,
                      std::vector<std::string> codecs,
                      std::string role,
                      bool allow_multivariant_playlist = false);
    PlaylistParseInfo(const PlaylistParseInfo& copy);
    ~PlaylistParseInfo();

    // The url that this media playlist came from. We might need to update it
    // if its a live playlist, so it's vital to keep it around.
    GURL uri;

    // Any detected codecs associated with this stream.
    std::vector<std::string> codecs;

    // The name given to this stream in chunk demuxer.
    std::string role;

    // Only root playlists are allowed to be multivariant.
    bool allow_multivariant_playlist;
  };

  // Allows continuing any pending seeks after important network requests have
  // completed.
  void FinishInitialization(PipelineStatusCallback cb, PipelineStatus status);
  void OnAdaptationComplete(PipelineStatus status);

  // Calls Rendition::CheckState and binds OnStateChecked to it's closure arg,
  // and records the timetick when the state checking happened.
  void CheckState(base::TimeDelta time,
                  double playback_rate,
                  std::string role,
                  ManifestDemuxer::DelayCallback cb,
                  base::TimeDelta delay_time);

  // The `prior_delay` arg represents the time that was previously calculated
  // for delay by another rendition. If it is kNoTimestamp, then the other
  // rendition has no need of a new event, so we can use whatever the response
  // from our current state check is, and vice-versa. The response to `cb`
  // othwewise should be the lower of `new_delay` and `prior_delay` - calctime
  // where calctime is Now() - `start`. For example:
  // Rendition1 requests 5 seconds, takes 2 seconds
  // Rendition2 requests 4 seconds, takes 1.5 seconds
  // Rendition3 requests kNoTimestamp, takes 1 second
  // First the 5 second response is carried forward, then after the second
  // response is acquired, the lesser of (5 - 1.5) and 4 is selected, so 3.5
  // seconds is carried forward as a delay time. Finally after the kNoTimestamp
  // response is acquired, the duration is once again subtracted and a final
  // delay time of 2.5 seconds is returned via cb.
  void OnStateChecked(base::TimeTicks start,
                      base::TimeDelta prior_delay,
                      ManifestDemuxer::DelayCallback cb,
                      base::TimeDelta new_delay);

  // Helpers to call |PlayerImplDemuxer::OnDemuxerError|.
  void Abort(HlsDemuxerStatus status);
  void Abort(hls::ParseStatus status);
  void Abort(HlsDataSourceProvider::ReadStatus status);

  // Read the entire contents of a data source stream before calling cb.
  void ReadUntilExhausted(HlsDataSourceProvider::ReadCb cb,
                          HlsDataSourceProvider::ReadResult result);

  void ParsePlaylist(PipelineStatusCallback parse_complete_cb,
                     PlaylistParseInfo parse_info,
                     HlsDataSourceProvider::ReadResult m_stream);

  void OnMultivariantPlaylist(
      PipelineStatusCallback parse_complete_cb,
      scoped_refptr<hls::MultivariantPlaylist> playlist);
  void OnRenditionsReselected(
      const hls::VariantStream* variant,
      const hls::AudioRendition* audio_override_rendition);

  void OnRenditionsSelected(
      PipelineStatusCallback on_complete,
      const hls::VariantStream* variant,
      const hls::AudioRendition* audio_override_rendition);

  void LoadPlaylist(PlaylistParseInfo parse_info,
                    PipelineStatusCallback on_complete);

  void OnMediaPlaylist(PipelineStatusCallback parse_complete_cb,
                       PlaylistParseInfo parse_info,
                       scoped_refptr<hls::MediaPlaylist> playlist);
  void DetermineStreamContainerAndCodecs(
      hls::MediaPlaylist* playlist,
      HlsDemuxerStatusCb<HlsCodecDetector::ContainerAndCodecs> container_cb);
  void OnPlaylistContainerDetermined(
      PipelineStatusCallback parse_complete_cb,
      PlaylistParseInfo parse_info,
      scoped_refptr<hls::MediaPlaylist> playlist,
      HlsDemuxerStatus::Or<HlsCodecDetector::ContainerAndCodecs> maybe_info);
  void PeekFirstSegment(
      HlsDemuxerStatusCb<HlsCodecDetector::ContainerAndCodecs> cb,
      HlsDataSourceProvider::ReadResult maybe_stream);

  void OnChunkDemuxerParseWarning(std::string role,
                                  SourceBufferParseWarning warning);
  void OnChunkDemuxerTracksChanged(std::string role,
                                   std::unique_ptr<MediaTracks> tracks);
  void ContinueSeekInternal(base::TimeDelta time,
                            ManifestDemuxer::SeekCallback cb);

  void InitializeWithCodecDetector(
      ManifestDemuxerEngineHost* host,
      PipelineStatusCallback status_cb,
      std::unique_ptr<HlsCodecDetector> codec_detector);
  void UpdateMediaPlaylistForRole(
      std::string role,
      GURL uri,
      base::OnceClosure cb,
      HlsDataSourceProvider::ReadResult maybe_stream);

  // Parses a playlist using the multivariant playlist, if it's being used.
  hls::ParseStatus::Or<scoped_refptr<hls::MediaPlaylist>>
  ParseMediaPlaylistFromStringSource(base::StringPiece source,
                                     GURL uri,
                                     hls::types::DecimalInteger version);

  base::SequenceBound<HlsDataSourceProvider> data_source_provider_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // root playlist, either multivariant or media.
  GURL root_playlist_uri_;

  std::unique_ptr<MediaLog> media_log_;
  raw_ptr<ManifestDemuxerEngineHost> host_
      GUARDED_BY_CONTEXT(media_sequence_checker_) = nullptr;

  // The codec detector is a reusable way for determining codecs in a media
  // stream.
  std::unique_ptr<HlsCodecDetector> codec_detector_
      GUARDED_BY_CONTEXT(media_sequence_checker_);

  // If the root playlist is multivariant, we need to store it for parsing the
  // dependant media playlists.
  scoped_refptr<hls::MultivariantPlaylist> multivariant_root_
      GUARDED_BY_CONTEXT(media_sequence_checker_);
  std::unique_ptr<hls::RenditionManager> rendition_manager_
      GUARDED_BY_CONTEXT(media_sequence_checker_);
  std::vector<std::string> selected_variant_codecs_
      GUARDED_BY_CONTEXT(media_sequence_checker_);

  // Multiple renditions are allowed, and have to be synchronized.
  base::flat_map<std::string, std::unique_ptr<HlsRendition>> renditions_
      GUARDED_BY_CONTEXT(media_sequence_checker_);

  size_t pending_playlist_network_requests_
      GUARDED_BY_CONTEXT(media_sequence_checker_) = 0;

  // This captures a pending seek and prevents it from interrupting manifest
  // updates. When the last manifest update completes, the seek closure can
  // continue.
  base::OnceClosure pending_seek_closure_
      GUARDED_BY_CONTEXT(media_sequence_checker_);

  // Disallow seeking until all renditions are parsed.
  bool pending_initialization_ GUARDED_BY_CONTEXT(media_sequence_checker_) =
      false;
  bool pending_adaptation_ GUARDED_BY_CONTEXT(media_sequence_checker_) = false;

  // When renditions are added, this ensures that they are all of the same
  // liveness, and allows access to the liveness check later.
  absl::optional<bool> is_seekable_ = absl::nullopt;

  // Ensure that safe member fields are only accessed on the media sequence.
  SEQUENCE_CHECKER(media_sequence_checker_);

  base::WeakPtrFactory<HlsManifestDemuxerEngine> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_
