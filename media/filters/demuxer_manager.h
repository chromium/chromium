// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DEMUXER_MANAGER_H_
#define MEDIA_FILTERS_DEMUXER_MANAGER_H_

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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

// This class manages both an implementation of media::Demuxer and of
// media::DataSource. DataSource, in particular may be null, since both MSE
// playback and Android's MediaPlayerRenderer do not make use of it. In the
// case that DataSource is present, these objects should have a similar
// lifetime, and both must be destroyed on the media thread, so owning them
// together makes sense. Additionally, the demuxer or data source can change
// during the lifetime of the player that owns them, so encapsulating that
// change logic separately lets the media player impl (WMPI) be a bit simpler,
// and dedicate a higher percentage of its complexity to managing playback
// state.
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

    // Used to determine if the client is additionally a client for Android's
    // MediaPlayerRenderer, which can inform us if we need to create a
    // MediaUrlDemuxer.
    virtual bool IsMediaPlayerRendererClient() = 0;

    virtual void OnError(media::PipelineStatus status) = 0;

    // Used for controlling the client when a demuxer swap happens.
    virtual void StopForDemuxerReset() = 0;
    virtual bool RestartForHls() = 0;

    virtual bool IsSecurityOriginCryptographic() const = 0;

#if BUILDFLAG(ENABLE_FFMPEG)
    virtual void AddAudioTrack(const std::string& id,
                               const std::string& label,
                               const std::string& language,
                               bool is_first_track) = 0;
    virtual void AddVideoTrack(const std::string& id,
                               const std::string& label,
                               const std::string& language,
                               bool is_first_track) = 0;
#endif  // BUILDFLAG(ENABLE_FFMPEG)

    // Returns true if playback would be able to start if data is present.
    virtual bool CouldPlayIfEnoughData() = 0;

    // Given a demuxer, the client should construct an implementation of
    // base::trace_event::MemoryDumpProvider for debugging purposes.
    virtual void MakeDemuxerThreadDumper(media::Demuxer* demuxer) = 0;

    virtual double CurrentTime() const = 0;

    // Allows us to set a loaded url on the client, which might happen when we
    // handle a redirect as part of a restart for switching to HLS.
    virtual void UpdateLoadedUrl(const GURL& url) = 0;
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
                 net::SiteForCookies site_for_cookies,
                 url::Origin top_frame_origin,
                 bool enable_instant_source_buffer_gc,
                 std::unique_ptr<Demuxer> demuxer_override);
  ~DemuxerManager();
  void InvalidateWeakPtrs();

  void OnPipelineError(PipelineStatus error);
  void SetLoadedUrl(GURL url);
  PipelineStatus ResetAfterHlsDetected(bool cryptographic_url);
  void DisallowFallback();

  // Methods that help manage demuxers
  absl::optional<double> GetDemuxerDuration();
  absl::optional<DemuxerType> GetDemuxerType();
  absl::optional<container_names::MediaContainerName> GetContainerForMetrics();
  void RespondToDemuxerMemoryUsageReport(base::OnceCallback<void(int64_t)> cb);

  // Returns a forwarded error/success from |on_demuxer_created|, or an error
  // if a demuxer couldn't be created.
  PipelineStatus CreateDemuxer(bool load_media_source,
                               DataSource::Preload preload,
                               bool has_poster,
                               DemuxerCreatedCB on_demuxer_created);

#if BUILDFLAG(IS_ANDROID)
  void SetAllowMediaPlayerRendererCredentials(bool allow);
#endif  // BUILDFLAG(IS_ANDROID)

  // Methods that help manage or access |data_source_|
  const DataSource* GetDataSourceForTesting() const;
  void SetDataSource(std::unique_ptr<DataSource> data_source);
  void OnBufferingHaveEnough(bool enough);
  void SetPreload(DataSource::Preload preload);

  void StopAndResetClient(Client* client);
  int64_t GetDataSourceMemoryUsage();
  void OnDataSourcePlaybackRateChange(double rate, bool paused);

  bool WouldTaintOrigin() const;
  bool HasDataSource() const;
  bool HasDemuxer() const;
  bool HasDemuxerOverride() const;
  absl::optional<GURL> GetDataSourceUrlAfterRedirects() const;
  bool DataSourceFullyBuffered() const;
  bool IsStreaming() const;
  bool PassedDataSourceTimingAllowOriginCheck() const;

 private:
  // Demuxer creation and helper methods
  std::unique_ptr<media::Demuxer> CreateChunkDemuxer();
#if BUILDFLAG(ENABLE_FFMPEG)
  std::unique_ptr<media::Demuxer> CreateFFmpegDemuxer();
#endif  // BUILDFLAG(ENABLE_FFMPEG)
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<media::Demuxer> CreateMediaUrlDemuxer(bool hls_content);
#endif  // BUILDFLAG(IS_ANDROID)
  void SetDemuxer(std::unique_ptr<Demuxer> demuxer);

  // Memory pressure listener specifically for when using ChunkDemuxer.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

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

  // This is usually just the WebMediaPlayerImpl.
  raw_ptr<Client, DanglingUntriaged> client_;

  // The demuxers need access the the media task runner and media log.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  std::unique_ptr<MediaLog> media_log_;

  // Android's MediaUrlDemuxer needs access to these.
  net::SiteForCookies site_for_cookies_;
  url::Origin top_frame_origin_;

  // When MSE memory pressure based garbage collection is enabled, the
  // |enable_instant_source_buffer_gc| controls whether the GC is done
  // immediately on memory pressure notification or during the next
  // SourceBuffer append (slower, but MSE spec compliant).
  bool enable_instant_source_buffer_gc_ = false;

  // Used for MediaUrlDemuxer when playing HLS content, as well as
  // FFmpegDemuxer in most cases. Also used for creating MemoryDataSource
  // objects.
  GURL loaded_url_;

  // The data source for creating a demuxer. This should be null when using
  // ChunkDemuxer.
  std::unique_ptr<DataSource> data_source_;

  // Holds whichever demuxer implementation is being used.
  std::unique_ptr<Demuxer> demuxer_;

  // Holds an optional demuxer that can be passed in at time of creation,
  // which becomes the default demuxer to use.
  std::unique_ptr<Demuxer> demuxer_override_;

  // RAII member for notifying demuxers of memory pressure.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

#if BUILDFLAG(IS_ANDROID)
  // Used to determine whether to allow credentials or not for
  // MediaPlayerRenderer.
  bool allow_media_player_renderer_credentials_ = false;
#endif  // BUILDFLAG(IS_ANDROID)

  bool demuxer_found_hls_ = false;

  // Are we allowed to switch demuxer mid-stream when fallback error codes
  // are encountered
  bool fallback_allowed_ = true;

  // Weak pointer implementation.
  base::WeakPtrFactory<DemuxerManager> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_DEMUXER_MANAGER_H_
