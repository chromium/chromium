// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_url_demuxer.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/demuxer.h"
#include "net/storage_access_api/status.h"

namespace media {

MediaUrlDemuxer::MediaUrlDemuxer(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const GURL& media_url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::StorageAccessApiStatus storage_access_api_status,
    bool allow_credentials,
    bool is_hls)
    : params_{media_url,         site_for_cookies,
              top_frame_origin,  storage_access_api_status,
              allow_credentials, is_hls},
      task_runner_(task_runner) {}

MediaUrlDemuxer::~MediaUrlDemuxer() = default;

// Should never be called since MediaResource::Type is URL.
std::vector<DemuxerStream*> MediaUrlDemuxer::GetAllStreams() {
  NOTREACHED();
}

const MediaUrlParams& MediaUrlDemuxer::GetMediaUrlParams() const {
  return params_;
}

MediaResource::Type MediaUrlDemuxer::GetType() const {
  return MediaResource::Type::KUrl;
}

std::string MediaUrlDemuxer::GetDisplayName() const {
  return "MediaUrlDemuxer";
}

DemuxerType MediaUrlDemuxer::GetDemuxerType() const {
  return DemuxerType::kMediaUrlDemuxer;
}

void MediaUrlDemuxer::ForwardDurationChangeToDemuxerHost(
    base::TimeDelta duration) {
  DCHECK(host_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  host_->SetDuration(duration);
}

void MediaUrlDemuxer::SetHeaders(
    base::flat_map<std::string, std::string> headers) {
  params_.headers = std::move(headers);
}

void MediaUrlDemuxer::Initialize(DemuxerHost* host,
                                 PipelineStatusCallback status_cb) {
  DVLOG(1) << __func__;
  host_ = host;
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(status_cb), PIPELINE_OK));
}

void MediaUrlDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {}

void MediaUrlDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {}

void MediaUrlDemuxer::Seek(base::TimeDelta time,
                           PipelineStatusCallback status_cb) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(status_cb), PIPELINE_OK));
}

bool MediaUrlDemuxer::IsSeekable() const {
  // While the demuxer itself is not seekable, the underlying player is.
  return true;
}

void MediaUrlDemuxer::Stop() {}

void MediaUrlDemuxer::AbortPendingReads() {}

base::TimeDelta MediaUrlDemuxer::GetStartTime() const {
  // TODO(tguilbert): Investigate if we need to fetch information from the
  // MediaPlayerRender in order to return a sensible value here.
  return base::TimeDelta();
}
base::Time MediaUrlDemuxer::GetTimelineOffset() const {
  return base::Time();
}

int64_t MediaUrlDemuxer::GetMemoryUsage() const {
  return 0;
}

std::optional<container_names::MediaContainerName>
MediaUrlDemuxer::GetContainerForMetrics() const {
  return std::nullopt;
}

void MediaUrlDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  std::vector<DemuxerStream*> streams;
  std::move(change_completed_cb).Run(streams);
  DLOG(WARNING) << "Track changes are not supported.";
}

void MediaUrlDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  std::vector<DemuxerStream*> streams;
  std::move(change_completed_cb).Run(streams);
  DLOG(WARNING) << "Track changes are not supported.";
}

}  // namespace media
