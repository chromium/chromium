// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_demuxer.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/container_names.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

HlsDemuxer::HlsDemuxer(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::SequenceBound<HlsDataSourceProvider> data_source_provider,
    GURL root_playlist_uri,
    MediaLog* media_log)
    : media_log_(media_log), task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_);
  MEDIA_LOG(INFO, media_log_) << GetDisplayName();
  DCHECK(data_source_provider);
}

HlsDemuxer::~HlsDemuxer() {
  DVLOG(1) << __func__;
}

std::vector<DemuxerStream*> HlsDemuxer::GetAllStreams() {
  DVLOG(1) << __func__;

  // TODO(crbug/1266991): Consult underlying ChunkDemuxer for its streams
  // instead of doing this:
  return std::vector<DemuxerStream*>();
}

std::string HlsDemuxer::GetDisplayName() const {
  return "HlsDemuxer";
}

DemuxerType HlsDemuxer::GetDemuxerType() const {
  return DemuxerType::kHlsDemuxer;
}

void HlsDemuxer::Initialize(DemuxerHost* host,
                            PipelineStatusCallback status_cb) {
  DVLOG(1) << __func__ << "(host=" << host << ")";
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // TODO(crbug/1266991): Save the host, create and wrap a ChunkDemuxerHost and
  // ChunkDemuxer. Initialize the wrapped ChunkDemuxer, giving it `status_cb`.
  // Also begin fetching the root playlist URI. Verify and update internal state
  // machine, too.
  // TODO(crbug/1266991): Consider suppressing the wrapped ChunkDemuxer's
  // displayname log when it is constructed (perhaps subtype it?).
}

void HlsDemuxer::AbortPendingReads() {
  DVLOG(1) << __func__;
  // TODO(crbug/1266991): Let the wrapped ChunkDemuxer know to abort pending
  // reads, if any.
}

void HlsDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {
  DVLOG(1) << __func__ << "(seek_time=" << seek_time.InMicroseconds() << "us)";
  // TODO(crbug/1266991): Time Remapping.
  // TODO(crbug/1266991): Let the wrapped ChunkDemuxer know to start waiting for
  // a seek to `seek_time`.
}

void HlsDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
  DVLOG(1) << __func__ << "(seek_time=" << seek_time.InMicroseconds() << "us)";
  // TODO(crbug/1266991): Time remapping.
  // TODO(crbug/1266991): Let the wrapped ChunkDemuxer know to cancel pending
  // seek for `seek_time`.
}

void HlsDemuxer::Seek(base::TimeDelta time, PipelineStatusCallback status_cb) {
  DVLOG(1) << __func__ << "(time=" << time.InMicroseconds() << "us)";
  // TODO(crbug/1266991): This should be intercepted when performing time
  // remapping.
  // TODO(crbug/1266991): Let the wrapped ChunkDemuxer know to seek to `time`
  // and give it `status_cb`.
}

bool HlsDemuxer::IsSeekable() const {
  // The underlying wrapping ChunkDemuxer is seekable.
  return true;
}

void HlsDemuxer::Stop() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // TODO(crbug/1266991): Let the wrapped ChunkDemuxer know to stop, clear it,
  // clear the host for it, invalidate any weak pointers we may have bound.
}

base::TimeDelta HlsDemuxer::GetStartTime() const {
  // TODO(crbug/1266991): Is any time remapping of HLS start time necessary
  // here?
  DVLOG(2) << __func__ << " -> 0";
  return base::TimeDelta();
}

base::Time HlsDemuxer::GetTimelineOffset() const {
  // TODO(crbug/1266991): Implement this with the value of the
  // EXT-X-PROGRAM-DATETIME tag.
  // TODO(crbug/1266991): Moderate that tag with respect to any underlying
  // streams' nonzero timeline offsets that the wrapped ChunkDemuxer may have?
  // And should wrapped ChunkDemuxer's enforcement that any specified (non-null)
  // offset across multiple ChunkDemuxer::OnSourceInitDone() match be relaxed if
  // its wrapped by an HLS demuxer which might ignore those offsets?
  DVLOG(2) << __func__ << " -> null time (0)";
  return base::Time();
}

int64_t HlsDemuxer::GetMemoryUsage() const {
  // TODO(crbug/1266991): If we have a wrapped ChunkDemuxer, consider returning
  // its usage here.
  // TODO(crbug/1266991): Consider other potential significant memory usage
  // here, if the data sources, playlist parser(s), rendition metadata or
  // timeline managers are significant memory consumers.
  DVLOG(1) << __func__ << " -> 0";
  return 0;
}

absl::optional<container_names::MediaContainerName>
HlsDemuxer::GetContainerForMetrics() const {
  DVLOG(1) << __func__;
  // TODO(crbug/1266991): Consider how this is used. HLS can involve multiple
  // stream types (mp2ts, mp4, etc). Refactor to report something useful.
  return absl::nullopt;
}

void HlsDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DVLOG(1) << __func__ << "(curr_time=" << curr_time.InMicroseconds() << "us)";
  // TODO(crbug/1266991): Handle this as necessary.
}

void HlsDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DVLOG(1) << __func__ << "(curr_time=" << curr_time.InMicroseconds() << "us)";
  // TODO(crbug/1266991): Handle this as necessary.
}

}  // namespace media
