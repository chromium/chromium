// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MANIFEST_DEMUXER_H_
#define MEDIA_FILTERS_MANIFEST_DEMUXER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "media/base/container_names.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/hls_data_source_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// Declared and defined in manifest_demuxer.cc.
class ManifestDemuxerStream;

class MEDIA_EXPORT ManifestDemuxer final : public Demuxer {
 public:
  explicit ManifestDemuxer(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::SequenceBound<HlsDataSourceProvider> data_source_provider,
      GURL root_playlist_uri,
      MediaLog* media_log);
  ~ManifestDemuxer() override;
  ManifestDemuxer(const ManifestDemuxer&) = delete;
  ManifestDemuxer(ManifestDemuxer&&) = delete;
  ManifestDemuxer& operator=(const ManifestDemuxer&) = delete;
  ManifestDemuxer& operator=(ManifestDemuxer&&) = delete;

  // `media::MediaResource` implementation
  std::vector<DemuxerStream*> GetAllStreams() override;

  // `media::Demuxer` implementation
  std::string GetDisplayName() const override;
  DemuxerType GetDemuxerType() const override;
  void Initialize(DemuxerHost* host, PipelineStatusCallback status_cb) override;
  void AbortPendingReads() override;
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;
  void Seek(base::TimeDelta time, PipelineStatusCallback status_cb) override;
  bool IsSeekable() const override;
  void Stop() override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override;
  int64_t GetMemoryUsage() const override;
  void SetPlaybackRate(double rate) override {}

  absl::optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override;

  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

 private:
  // This wrapper class allows us to capture the results of Read() and use
  // DecoderBuffer timestamps to update the current media time within the
  // loaded buffer, without having to make modifications to ChunkDemuxer.
  class ManifestDemuxerStream : public DemuxerStream {
   public:
    ~ManifestDemuxerStream() override;
    using WrapperReadCb =
        base::RepeatingCallback<void(DemuxerStream::ReadCB,
                                     DemuxerStream::Status,
                                     DemuxerStream::DecoderBufferVector)>;
    ManifestDemuxerStream(DemuxerStream* stream, WrapperReadCb cb);
    void Read(uint32_t count, DemuxerStream::ReadCB cb) override;
    AudioDecoderConfig audio_decoder_config() override;
    VideoDecoderConfig video_decoder_config() override;
    DemuxerStream::Type type() const override;
    StreamLiveness liveness() const override;
    void EnableBitstreamConverter() override;
    bool SupportsConfigChanges() override;

   private:
    WrapperReadCb read_cb_;
    DemuxerStream* stream_;
  };

  void OnChunkDemuxerInitialized(PipelineStatus init_status);
  void OnChunkDemuxerOpened();
  void OnProgress();
  void OnEncryptedMediaData(EmeInitDataType type,
                            const std::vector<uint8_t>& data);
  void OnDemuxerStreamRead(DemuxerStream::ReadCB wrapped_read_cb,
                           DemuxerStream::Status status,
                           DemuxerStream::DecoderBufferVector buffers);

  std::unique_ptr<MediaLog> media_log_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Pending callbacks.
  PipelineStatusCallback pending_init_;

  // Wrapped chunk demuxer that actually does the parsing and demuxing of the
  // raw data we feed it.
  std::unique_ptr<ChunkDemuxer> chunk_demuxer_;

  // Updated by seek, and by updates from outgoing frames.
  base::TimeDelta media_time_ = base::Seconds(0);

  // Keeps a map of demuxer streams to their wrapper implementations which
  // can be used to set the current media time. ChunkDemuxer's streams live
  // forever due to the use of raw pointers in the pipeline, so these must
  // also live for the duration of `this` lifetime.
  base::flat_map<DemuxerStream*, std::unique_ptr<ManifestDemuxerStream>>
      streams_;

  base::WeakPtrFactory<ManifestDemuxer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_MANIFEST_DEMUXER_H_
