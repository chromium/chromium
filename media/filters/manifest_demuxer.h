// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MANIFEST_DEMUXER_H_
#define MEDIA_FILTERS_MANIFEST_DEMUXER_H_

#include <vector>

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
#include "media/filters/hls_data_source_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

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
  MediaLog* media_log_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_MANIFEST_DEMUXER_H_
