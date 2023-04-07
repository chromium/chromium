// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/manifest_demuxer.h"

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

ManifestDemuxer::ManifestDemuxerStream::~ManifestDemuxerStream() = default;

ManifestDemuxer::ManifestDemuxerStream::ManifestDemuxerStream(
    DemuxerStream* stream,
    WrapperReadCb cb)
    : read_cb_(std::move(cb)), stream_(stream) {}

void ManifestDemuxer::ManifestDemuxerStream::Read(uint32_t count,
                                                  DemuxerStream::ReadCB cb) {
  stream_->Read(count, base::BindOnce(read_cb_, std::move(cb)));
}

AudioDecoderConfig
ManifestDemuxer::ManifestDemuxerStream::audio_decoder_config() {
  return stream_->audio_decoder_config();
}

VideoDecoderConfig
ManifestDemuxer::ManifestDemuxerStream::video_decoder_config() {
  return stream_->video_decoder_config();
}

DemuxerStream::Type ManifestDemuxer::ManifestDemuxerStream::type() const {
  return stream_->type();
}

StreamLiveness ManifestDemuxer::ManifestDemuxerStream::liveness() const {
  return stream_->liveness();
}

void ManifestDemuxer::ManifestDemuxerStream::EnableBitstreamConverter() {
  stream_->EnableBitstreamConverter();
}

bool ManifestDemuxer::ManifestDemuxerStream::SupportsConfigChanges() {
  return stream_->SupportsConfigChanges();
}

ManifestDemuxer::ManifestDemuxer(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner_,
    base::SequenceBound<HlsDataSourceProvider> data_source_provider,
    GURL root_playlist_uri,
    MediaLog* media_log)
    : media_log_(media_log), media_task_runner_(std::move(media_task_runner_)) {
  // TODO(crbug/1266991): Save `data_source_provider` to make requests for media
  // content.
  DCHECK(data_source_provider);
}

ManifestDemuxer::~ManifestDemuxer() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
}

std::vector<DemuxerStream*> ManifestDemuxer::GetAllStreams() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // For each stream that ChunkDemuxer returns, we need to wrap it so that we
  // can grab the timestamp. Chunk demuxer's streams live forever, so ours
  // might as well also live forever, even if that leaks a small amount of
  // memory.
  // TODO(crbug/1266991): Rearchitect the demuxer stream ownership model to
  // prevent long-lived streams from potentially leaking memory.
  std::vector<DemuxerStream*> streams;
  for (DemuxerStream* chunk_demuxer_stream : chunk_demuxer_->GetAllStreams()) {
    auto it = streams_.find(chunk_demuxer_stream);
    if (it != streams_.end()) {
      streams.push_back(it->second.get());
      continue;
    }
    auto wrapper = std::make_unique<ManifestDemuxerStream>(
        chunk_demuxer_stream,
        base::BindRepeating(&ManifestDemuxer::OnDemuxerStreamRead,
                            weak_factory_.GetWeakPtr()));
    streams.push_back(wrapper.get());
    streams_[chunk_demuxer_stream] = std::move(wrapper);
  }
  return streams;
}

std::string ManifestDemuxer::GetDisplayName() const {
  return "ManifestDemuxer";
}

DemuxerType ManifestDemuxer::GetDemuxerType() const {
  return DemuxerType::kManifestDemuxer;
}

void ManifestDemuxer::Initialize(DemuxerHost* host,
                                 PipelineStatusCallback status_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Save `host` for error handling.

  pending_init_ = std::move(status_cb);
  chunk_demuxer_ = std::make_unique<ChunkDemuxer>(
      base::BindOnce(&ManifestDemuxer::OnChunkDemuxerOpened,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&ManifestDemuxer::OnProgress,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&ManifestDemuxer::OnEncryptedMediaData,
                          weak_factory_.GetWeakPtr()),
      media_log_.get());

  chunk_demuxer_->Initialize(
      host, base::BindOnce(&ManifestDemuxer::OnChunkDemuxerInitialized,
                           weak_factory_.GetWeakPtr()));
}

void ManifestDemuxer::AbortPendingReads() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  chunk_demuxer_->AbortPendingReads();
}

void ManifestDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  chunk_demuxer_->StartWaitingForSeek(seek_time);
}

void ManifestDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << __func__ << "(seek_time=" << seek_time.InMicroseconds() << "us)";
  // TODO(crbug/1266991): Time remapping.
  // TODO(crbug/1266991): Let the wrapped ChunkDemuxer know to cancel pending
  // seek for `seek_time`.
}

void ManifestDemuxer::Seek(base::TimeDelta time,
                           PipelineStatusCallback status_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  media_time_ = time;

  // TODO(crbug/1266991): Seek the wrapped ChunkDemuxer.
  std::move(status_cb).Run(PIPELINE_ERROR_ABORT);
}

bool ManifestDemuxer::IsSeekable() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // The underlying wrapping ChunkDemuxer is seekable.
  return true;
}

void ManifestDemuxer::Stop() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  chunk_demuxer_->Stop();
  chunk_demuxer_.reset();
}

base::TimeDelta ManifestDemuxer::GetStartTime() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Is any time remapping of HLS start time necessary
  // here?
  return base::TimeDelta();
}

base::Time ManifestDemuxer::GetTimelineOffset() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Implement this with the value of the
  // EXT-X-PROGRAM-DATETIME tag.
  // TODO(crbug/1266991): Moderate that tag with respect to any underlying
  // streams' nonzero timeline offsets that the wrapped ChunkDemuxer may have?
  // And should wrapped ChunkDemuxer's enforcement that any specified (non-null)
  // offset across multiple ChunkDemuxer::OnSourceInitDone() match be relaxed if
  // its wrapped by an HLS demuxer which might ignore those offsets?
  return base::Time();
}

int64_t ManifestDemuxer::GetMemoryUsage() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Consider other potential significant memory usage
  // here, if the data sources, playlist parser(s), rendition metadata or
  // timeline managers are significant memory consumers.
  return chunk_demuxer_->GetMemoryUsage();
}

absl::optional<container_names::MediaContainerName>
ManifestDemuxer::GetContainerForMetrics() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Consider how this is used. HLS can involve multiple
  // stream types (mp2t, mp4, etc). Refactor to report something useful.
  return absl::nullopt;
}

void ManifestDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  chunk_demuxer_->OnEnabledAudioTracksChanged(track_ids, curr_time,
                                              std::move(change_completed_cb));
}

void ManifestDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  chunk_demuxer_->OnSelectedVideoTrackChanged(track_ids, curr_time,
                                              std::move(change_completed_cb));
}

void ManifestDemuxer::OnChunkDemuxerInitialized(PipelineStatus init_status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(pending_init_);
  std::move(pending_init_).Run(std::move(init_status));
}

void ManifestDemuxer::OnChunkDemuxerOpened() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Implement.
  NOTIMPLEMENTED();
}

void ManifestDemuxer::OnProgress() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Implement.
  NOTIMPLEMENTED();
}

void ManifestDemuxer::OnEncryptedMediaData(EmeInitDataType type,
                                           const std::vector<uint8_t>& data) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): This will be required for iOS support in the future.
  NOTIMPLEMENTED();
}

void ManifestDemuxer::OnDemuxerStreamRead(
    DemuxerStream::ReadCB wrapped_read_cb,
    DemuxerStream::Status status,
    DemuxerStream::DecoderBufferVector buffers) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (status == DemuxerStream::Status::kOk) {
    // The entire vector must be checked as timestamps are often out of order.
    for (const auto& buffer : buffers) {
      if (buffer->timestamp() > media_time_) {
        media_time_ = buffer->timestamp();
      }
    }
  }

  std::move(wrapped_read_cb).Run(status, std::move(buffers));
}

}  // namespace media
