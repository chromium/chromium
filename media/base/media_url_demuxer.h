// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_URL_DEMUXER_H_
#define MEDIA_BASE_MEDIA_URL_DEMUXER_H_

#include <stddef.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/demuxer.h"
#include "net/storage_access_api/status.h"
#include "url/gurl.h"

namespace net {
class SiteForCookies;
}  // namespace net

namespace media {

// Class that saves a URL for later retrieval. To be used in conjunction with
// the MediaPlayerRenderer.
//
// Its primary purpose is to act as a dummy Demuxer, when there is no need
// for DemuxerStreams (e.g. in the MediaPlayerRenderer case). For the most part,
// its implementation of the Demuxer are NOPs that return the default values and
// fire any provided callbacks immediately.
//
// If Pipeline where to be refactored to use a MediaResource instead of
// a Demuxer, MediaUrlDemuxer should be refactored to inherit directly from
// MediaResource.
class MEDIA_EXPORT MediaUrlDemuxer : public Demuxer {
 public:
  MediaUrlDemuxer(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                  const GURL& media_url,
                  const net::SiteForCookies& site_for_cookies,
                  const url::Origin& top_frame_origin,
                  net::StorageAccessApiStatus storage_access_api_status,
                  bool allow_credentials,
                  bool is_hls);

  MediaUrlDemuxer(const MediaUrlDemuxer&) = delete;
  MediaUrlDemuxer& operator=(const MediaUrlDemuxer&) = delete;

  ~MediaUrlDemuxer() override;

  // MediaResource interface.
  std::vector<DemuxerStream*> GetAllStreams() override;
  const MediaUrlParams& GetMediaUrlParams() const override;
  MediaResource::Type GetType() const override;
  void ForwardDurationChangeToDemuxerHost(base::TimeDelta duration) override;
  void SetHeaders(base::flat_map<std::string, std::string> headers) override;

  // Demuxer interface.
  std::string GetDisplayName() const override;
  DemuxerType GetDemuxerType() const override;
  void Initialize(DemuxerHost* host, PipelineStatusCallback status_cb) override;
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;
  void Seek(base::TimeDelta time, PipelineStatusCallback status_cb) override;
  bool IsSeekable() const override;
  void Stop() override;
  void AbortPendingReads() override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override;
  int64_t GetMemoryUsage() const override;
  std::optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override;
  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;
  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;
  void SetPlaybackRate(double rate) override {}

 private:
  MediaUrlParams params_;
  raw_ptr<DemuxerHost> host_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_URL_DEMUXER_H_
