// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DEMUXER_MANAGER_H_
#define MEDIA_FILTERS_DEMUXER_MANAGER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/data_source.h"
#include "media/base/demuxer.h"
#include "media/base/eme_constants.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"
#include "media/filters/chunk_demuxer.h"
#include "net/storage_access_api/status.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
#include "base/threading/sequence_bound.h"
#include "media/filters/hls_data_source_provider.h"
#include "url/gurl.h"
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

namespace media {

// This class manages both an implementation of Demuxer and of
// DataSource. DataSource, in particular may be null, since MSE playback
// does not make use of it. In the case that DataSource is present, these
// objects should have a similar lifetime, and both must be destroyed on the
// media thread, so owning them together makes sense. Additionally, the demuxer
// or data source can change during the lifetime of the player that owns them,
// so encapsulating that change logic separately lets the media player impl
// (WMPI) be a bit simpler, and dedicate a higher percentage of its complexity
// to managing playback state.
class MEDIA_EXPORT DemuxerManager {
 public:
  class Client {
   public:
    virtual void OnEncryptedMediaInitData(
        EmeInitDataType init_data_type,
        const std::vector<uint8_t>& init_data) = 0;

    virtual void OnChunkDemuxerOpened(ChunkDemuxer* demuxer) = 0;

    // Called by the data source (for src=) or demuxer (for mse) when loading
    // progresses.
    // Can be called quite often.
    virtual void OnProgress() = 0;

    virtual void OnError(PipelineStatus status) = 0;

    // Used for controlling the client when a demuxer swap happens.
    virtual void StopForDemuxerReset() = 0;
    virtual void RestartForHls() = 0;

#if BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)
    virtual void AddTrack(const MediaTrack&) = 0;
    virtual void RemoveTrack(const MediaTrack&) = 0;
    virtual void SetTrackState(const MediaTrack&, MediaTrack::State) = 0;
#endif  // BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
    virtual base::SequenceBound<HlsDataSourceProvider>
    GetHlsDataSourceProvider() = 0;
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

    // Returns true if playback would be able to start if data is present.
    virtual bool CouldPlayIfEnoughData() = 0;

    // Given a demuxer, the client should construct an implementation of
    // base::trace_event::MemoryDumpProvider for debugging purposes.
    virtual void MakeDemuxerThreadDumper(Demuxer* demuxer) = 0;

    virtual double CurrentTime() const = 0;

    // Allows us to set a loaded url on the client, which might happen when we
    // handle a redirect as part of a restart for switching to HLS.
    virtual void UpdateLoadedUrl(const GURL& url) = 0;

    // Allows a seek triggered by a demuxer (mainly used for live content)
    virtual void DemuxerRequestsSeek(base::TimeDelta seek_time) = 0;
  };

  // Demuxer, StartType, IsStreaming, IsStatic
  using DemuxerCreatedCB =
      base::OnceCallback<PipelineStatus(Demuxer* demuxer,
                                        Pipeline::StartType start_type,
                                        bool /*is_streaming*/,
                                        bool /*is_static*/)>;

  DemuxerManager(Client* client,
                 scoped_refptr<base::SequencedTaskRunner> media_task_runner,
                 MediaLog* log,
                 std::unique_ptr<Demuxer> demuxer_override);
  ~DemuxerManager();
  void InvalidateWeakPtrs();

  void OnPipelineError(PipelineStatus error);
  void SetLoadedUrl(GURL url);
  const GURL& LoadedUrl() const;
#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  PipelineStatus SelectHlsFallbackMechanism(bool cryptographic_url);
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)
  void DisallowFallback();

  // Methods that help manage demuxers
  std::optional<double> GetDemuxerDuration();
  std::optional<DemuxerType> GetDemuxerType() const;
  std::optional<container_names::MediaContainerName> GetContainerForMetrics();
  void RespondToDemuxerMemoryUsageReport(base::OnceCallback<void(int64_t)> cb);
  void DisableDemuxerCanChangeType();

  // Returns a forwarded error/success from |on_demuxer_created|, or an error
  // if a demuxer couldn't be created.
  PipelineStatus CreateDemuxer(
      bool load_media_source,
      DataSource::Preload preload,
      bool needs_first_frame,
      DemuxerCreatedCB on_demuxer_created,
      base::flat_map<std::string, std::string> headers);

  // Methods that help manage or access |data_source_|
  DataSource* GetDataSourceForTesting() const;
  void SetDataSource(std::unique_ptr<DataSource> data_source);
  void StopPreloading();
  void SetPreload(DataSource::Preload preload);

  void StopAndResetClient();
  int64_t GetDataSourceMemoryUsage();
  void OnDataSourcePlaybackRateChange(double rate, bool paused);

  // Signal that a demuxer (or renderer) has caused a duration change.
  void DurationChanged();

  bool WouldTaintOrigin() const;
  bool HasDataSource() const;
  bool HasDemuxer() const;
  bool HasDemuxerOverride() const;
  std::optional<GURL> GetDataSourceUrlAfterRedirects() const;
  bool DataSourceFullyBuffered() const;
  bool IsStreaming() const;
  bool IsLiveContent() const;

 private:
  // Demuxer creation and helper methods
  std::unique_ptr<Demuxer> CreateChunkDemuxer();

#if BUILDFLAG(ENABLE_FFMPEG)
  std::unique_ptr<Demuxer> CreateFFmpegDemuxer();
#endif  // BUILDFLAG(ENABLE_FFMPEG)

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  std::tuple<raw_ptr<DataSourceInfo>, std::unique_ptr<Demuxer>>
  CreateHlsDemuxer();
#endif

#if BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)
  void AddTrack(const MediaTrack&);
  void RemoveTrack(const MediaTrack&);
#endif  // BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)

  void SetDemuxer(std::unique_ptr<Demuxer> demuxer);

  // Trampoline methods for binding with |weak_this_| that call into |client_|.;
  void OnEncryptedMediaInitData(EmeInitDataType init_data_type,
                                const std::vector<uint8_t>& init_data);
  void OnChunkDemuxerOpened();
  void OnProgress();
  void RestartClientForHLS();
  void FreeResourcesAfterMediaThreadWait(base::OnceClosure cb);

#if BUILDFLAG(ENABLE_FFMPEG)
  void OnFFmpegMediaTracksUpdated(std::unique_ptr<MediaTracks> tracks);
#endif  // BUILDFLAG(ENABLE_FFMPEG)

  void DemuxerRequestsSeek(base::TimeDelta time);

  // This is usually just the WebMediaPlayerImpl.
  raw_ptr<Client, DanglingUntriaged> client_;

  // The demuxers need access the the media task runner and media log.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  std::unique_ptr<MediaLog> media_log_;

  // Used for FFmpegDemuxer in most cases and for creating MemoryDataSource
  // objects.
  // Note: this may be very large, take care when making copies.
  GURL loaded_url_;

  // The data source for creating a demuxer. This should be null when using
  // ChunkDemuxer.
  std::unique_ptr<DataSource> data_source_;

  // Holds whichever demuxer implementation is being used.
  std::unique_ptr<Demuxer> demuxer_;

  // Refers to the owned object that can query information about a data source.
  // For most playbacks, this is a raw ptr to `data_source_`, and so it is safe,
  // since this class also owns that object. For HLS playback, this object is
  // the HlsManifestDemuxerEngine, owned by `demuxer_`, so again, it is safe to
  // keep a raw ptr here.
  raw_ptr<DataSourceInfo> data_source_info_ = nullptr;

  // Holds an optional demuxer that can be passed in at time of creation,
  // which becomes the default demuxer to use.
  std::unique_ptr<Demuxer> demuxer_override_;

  bool hls_fallback_ = false;

  // Are we allowed to switch demuxer mid-stream when fallback error codes
  // are encountered
  bool fallback_allowed_ = true;

  // Weak pointer implementation.
  base::WeakPtrFactory<DemuxerManager> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_DEMUXER_MANAGER_H_
