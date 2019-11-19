// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_URL_DEMUXER_H_
#define MEDIA_BASE_MEDIA_URL_DEMUXER_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

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
  MediaUrlDemuxer(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const GURL& media_url,
      const GURL& site_for_cookies,
      const url::Origin& top_frame_origin,
      bool allow_credentials,
      bool is_hls);
  ~MediaUrlDemuxer() override;

  // MediaResource interface.
  std::vector<DemuxerStream*> GetAllStreams() override;
  const MediaUrlParams& GetMediaUrlParams() const override;
  MediaResource::Type GetType() const override;
  void ForwardDurationChangeToDemuxerHost(base::TimeDelta duration) override;

  // Demuxer interface.
  std::string GetDisplayName() const override;
  void Initialize(DemuxerHost* host, PipelineStatusCallback status_cb) override;
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;
  void Seek(base::TimeDelta time, PipelineStatusCallback status_cb) override;
  void Stop() override;
  void AbortPendingReads() override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override;
  int64_t GetMemoryUsage() const override;
  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;
  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

 private:
  MediaUrlParams params_;
  DemuxerHost* host_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MediaUrlDemuxer);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_URL_DEMUXER_H_
