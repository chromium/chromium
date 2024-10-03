// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_
#define MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "crypto/encryptor.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_demuxer_status.h"
#include "media/filters/hls_network_access_impl.h"
#include "media/filters/hls_rendition.h"
#include "media/filters/hls_stats_reporter.h"
#include "media/filters/manifest_demuxer.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/rendition_manager.h"

namespace media {

// A HLS-Parser/Player implementation of ManifestDemuxer's Engine interface.
// This will use the HLS parsers and rendition selectors to fetch and parse
// playlists, followed by fetching and appending media segments.
class MEDIA_EXPORT HlsManifestDemuxerEngine : public ManifestDemuxer::Engine,
                                              public HlsRenditionHost,
                                              public DataSourceInfo {
 public:
  HlsManifestDemuxerEngine(
      base::SequenceBound<HlsDataSourceProvider> dsp,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::RepeatingCallback<void(const MediaTrack&)> add_track,
      base::RepeatingCallback<void(const MediaTrack&)> remove_track,
      bool was_already_tainted,
      GURL root_playlist_uri,
      MediaLog* media_log);
  ~HlsManifestDemuxerEngine() override;

  // DataSourceInfo implementation
  int64_t GetMemoryUsage() override;
  bool WouldTaintOrigin() override;
  bool IsStreaming() override;

  // ManifestDemuxer::Engine implementation
  std::string GetName() const override;
  void Initialize(ManifestDemuxerEngineHost* host,
                  PipelineStatusCallback status_cb) override;
  void OnTimeUpdate(base::TimeDelta time,
                    double playback_rate,
                    ManifestDemuxer::DelayCallback cb) override;
  void Seek(base::TimeDelta time, ManifestDemuxer::SeekCallback cb) override;
  void StartWaitingForSeek() override;
  void AbortPendingReads(base::OnceClosure cb) override;
  bool IsSeekable() const override;
  int64_t GetMemoryUsage() const override;
  void Stop() override;

  // HlsRenditionHost implementation.
  void ReadKey(const hls::MediaSegment::EncryptionData& data,
               HlsDataSourceProvider::ReadCb) override;
  void ReadManifest(const GURL& uri, HlsDataSourceProvider::ReadCb cb) override;
  void ReadMediaSegment(const hls::MediaSegment& segment,
                        bool read_chunked,
                        bool include_init,
                        HlsDataSourceProvider::ReadCb cb) override;
  void ReadStream(std::unique_ptr<HlsDataSourceStream> stream,
                  HlsDataSourceProvider::ReadCb cb) override;
  void UpdateNetworkSpeed(uint64_t bps) override;
  void UpdateRenditionManifestUri(std::string role,
                                  GURL uri,
                                  base::OnceCallback<void(bool)> cb) override;
  void SetEndOfStream(bool ended) override;

  // Test helpers.
  void AddRenditionForTesting(std::string role,
                              std::unique_ptr<HlsRendition> test_rendition);

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

  // Adds an asynchronous action to the action queue, based on both a bound
  // response callback and an action callback.
  // ProcessAsyncAction<T> is specialized for the T which represents the type
  // of argument passed back in the response callback. The first arg, `cb`,
  // which is of type OnceCallback<PipelineStatus> would be added to the queue
  // using ProcessAsyncCallback<PipelineStatus>(cb, ...).
  // The second arg, `thunk`, is the actual action callback. This callback
  // should only take a OnceCallback<T> as a single unbound argument.
  template <typename Response,
            typename CB = base::OnceCallback<void(Response)>,
            typename Thunk = base::OnceCallback<void(CB)>>
  void ProcessAsyncAction(CB cb, Thunk thunk) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
    // Capture both `thunk` and `cb` in a closure, which can be added to the
    // queue. This allows effectively erasing the `Response` type and storing
    // multiple different types of bound actions.
    base::OnceCallback<void(base::OnceClosure)> action = base::BindOnce(
        [](Thunk thunk, CB cb, base::OnceClosure finished) {
          // When this action is processed, we call the action callback `thunk`
          // and pass it a new bound callback with the same type as `cb` that
          // can also advance the queue after `cb` has been executed.
          std::move(thunk).Run(base::BindOnce(
              [](CB cb, base::OnceClosure finished, Response value) {
                std::move(cb).Run(value);
                std::move(finished).Run();
              },
              std::move(cb), std::move(finished)));
        },
        std::move(thunk), std::move(cb));

    pending_action_queue_.push(std::move(action));
    ProcessActionQueue();
  }

  // Pops the next action off the stack if its not empty and if another action
  // is not already running.
  void ProcessActionQueue();

  // When an action is complete, check its runtime and call ProcessActionQueue
  // again.
  void OnActionComplete();

  // Actions:

  // Posted by `::Seek()`
  void SeekAction(base::TimeDelta time, ManifestDemuxer::SeekCallback cb);
  void ContinueSeekInternal(base::TimeDelta time,
                            ManifestDemuxer::SeekCallback cb);

  // Posted by `::Initialize()`
  void InitAction(PipelineStatusCallback status_cb);
  void FinishInitialization(PipelineStatusCallback cb, PipelineStatus status);

  // Posted by `::OnTimeUpdate()`
  void OnTimeUpdateAction(base::TimeDelta time,
                          double playback_rate,
                          ManifestDemuxer::DelayCallback cb);
  void FinishTimeUpdate(ManifestDemuxer::DelayCallback cb,
                        base::TimeDelta delay_time);
  void CheckState(base::TimeDelta time,
                  double playback_rate,
                  std::string role,
                  ManifestDemuxer::DelayCallback cb,
                  base::TimeDelta delay_time);
  void UpdateMediaPlaylistForRole(
      std::string role,
      GURL uri,
      base::OnceCallback<void(bool)> cb,
      HlsDataSourceProvider::ReadResult maybe_stream);

  // Posted by `::OnRenditionsReselected()`
  void AdaptationAction(const hls::VariantStream* variant,
                        const hls::AudioRendition* audio_override_rendition,
                        PipelineStatusCallback status_cb);

  // Allows continuing any pending seeks after important network requests have
  // completed.
  void OnAdaptationComplete(PipelineStatus status);

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
  void OnStatus(PipelineStatus status);
  void Abort(HlsDemuxerStatus status);
  void Abort(hls::ParseStatus status);
  void Abort(HlsDataSourceProvider::ReadStatus status);

  // Capture the stream before it gets posted to `cb` and update the internal
  // memory state and origin tainting.
  void UpdateHlsDataSourceStats(
      HlsDataSourceProvider::ReadCb cb,
      HlsDataSourceProvider::ReadStatus::Or<
          std::unique_ptr<HlsDataSourceStream>> result);

  // Helper to bind `UpdateHlsDataSourceStats` around a response CB.
  HlsDataSourceProvider::ReadCb BindStatsUpdate(
      HlsDataSourceProvider::ReadCb cb);

  void ParsePlaylist(PipelineStatusCallback parse_complete_cb,
                     PlaylistParseInfo parse_info,
                     HlsDataSourceProvider::ReadResult m_stream);

  void OnMultivariantPlaylist(
      PipelineStatusCallback parse_complete_cb,
      scoped_refptr<hls::MultivariantPlaylist> playlist);
  void OnRenditionsReselected(
      hls::AdaptationReason reason,
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
  void DetermineStreamContainer(
      hls::MediaPlaylist* playlist,
      HlsDemuxerStatusCb<RelaxedParserSupportedType> container_cb);
  void OnStreamContainerDetermined(
      PipelineStatusCallback parse_complete_cb,
      PlaylistParseInfo parse_info,
      scoped_refptr<hls::MediaPlaylist> playlist,
      HlsDemuxerStatus::Or<RelaxedParserSupportedType> maybe_info);
  void DetermineBitstreamContainer(
      scoped_refptr<hls::MediaSegment> segment,
      HlsDemuxerStatusCb<RelaxedParserSupportedType> cb,
      HlsDataSourceProvider::ReadResult maybe_stream);

  void OnChunkDemuxerParseWarning(std::string role,
                                  SourceBufferParseWarning warning);
  void OnChunkDemuxerTracksChanged(std::string role,
                                   std::unique_ptr<MediaTracks> tracks);

  // Parses a playlist using the multivariant playlist, if it's being used.
  hls::ParseStatus::Or<scoped_refptr<hls::MediaPlaylist>>
  ParseMediaPlaylistFromStringSource(std::string_view source,
                                     GURL uri,
                                     hls::types::DecimalInteger version);

  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Track helper functions
  base::RepeatingCallback<void(const MediaTrack&)> add_track_;
  base::RepeatingCallback<void(const MediaTrack&)> remove_track_;

  // root playlist, either multivariant or media.
  GURL root_playlist_uri_;

  std::unique_ptr<MediaLog> media_log_;
  raw_ptr<ManifestDemuxerEngineHost> host_
      GUARDED_BY_CONTEXT(media_sequence_checker_) = nullptr;

  // The network access implementation.
  std::unique_ptr<HlsNetworkAccess> network_access_
      GUARDED_BY_CONTEXT(media_sequence_checker_);

  // If the root playlist is multivariant, we need to store it for parsing the
  // dependent media playlists.
  scoped_refptr<hls::MultivariantPlaylist> multivariant_root_
      GUARDED_BY_CONTEXT(media_sequence_checker_);
  std::unique_ptr<hls::RenditionManager> rendition_manager_
      GUARDED_BY_CONTEXT(media_sequence_checker_);
  std::vector<std::string> selected_variant_codecs_
      GUARDED_BY_CONTEXT(media_sequence_checker_);

  // Multiple renditions are allowed, and have to be synchronized.
  base::flat_map<std::string, std::unique_ptr<HlsRendition>> renditions_
      GUARDED_BY_CONTEXT(media_sequence_checker_);

  // Events like time update, resolution change, manifest updating, etc, all
  // add an action to this queue, and then try to process it. When the queue is
  // empty, `pending_time_check_response_cb_` is posted if it is set.
  base::queue<base::OnceCallback<void(base::OnceClosure)>> pending_action_queue_
      GUARDED_BY_CONTEXT(media_sequence_checker_);

  // The count of ended streams. When this count reaches the length of
  // `renditions_`, all streams are ended. When this count drops to one below
  // the length, then the playback is considered un-ended.
  size_t ended_stream_count_ GUARDED_BY_CONTEXT(media_sequence_checker_) = 0;

  // Is an action currently running?
  bool action_in_progress_ GUARDED_BY_CONTEXT(media_sequence_checker_) = false;

  // The informational class which can combine memory usage and origin tainting
  // for all sub-data sources.
  bool origin_tainted_ = false;

  // The total amount of memory that is used by data sources. It gets updated
  // after every successful read.
  uint64_t total_stream_memory_ = 0;

  // When renditions are added, this ensures that they are all of the same
  // liveness, and allows access to the liveness check later.
  std::optional<bool> is_seekable_ = std::nullopt;

  hls::HlsStatsReporter stats_reporter_
      GUARDED_BY_CONTEXT(media_sequence_checker_);

  // Ensure that safe member fields are only accessed on the media sequence.
  SEQUENCE_CHECKER(media_sequence_checker_);

  base::WeakPtrFactory<HlsManifestDemuxerEngine> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_
