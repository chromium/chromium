// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/manifest_demuxer.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/container_names.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/formats/hls/audio_rendition.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
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

ManifestDemuxer::~ManifestDemuxer() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
}

ManifestDemuxer::ManifestDemuxer(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    std::unique_ptr<ManifestDemuxer::Engine> impl,
    MediaLog* media_log)
    : media_log_(media_log->Clone()),
      media_task_runner_(std::move(media_task_runner)),
      impl_(std::move(impl)) {}

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
  return impl_->GetName();
}

DemuxerType ManifestDemuxer::GetDemuxerType() const {
  return DemuxerType::kManifestDemuxer;
}

void ManifestDemuxer::Initialize(DemuxerHost* host,
                                 PipelineStatusCallback status_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!pending_init_);

  host_ = host;
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

  impl_->Initialize(this, base::BindOnce(&ManifestDemuxer::OnEngineInitialized,
                                         weak_factory_.GetWeakPtr()));
}

void ManifestDemuxer::AbortPendingReads() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  chunk_demuxer_->AbortPendingReads();
  impl_->AbortPendingReads();
}

void ManifestDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  chunk_demuxer_->StartWaitingForSeek(seek_time);
  impl_->StartWaitingForSeek();
}

void ManifestDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): In the current implementation, if a seek happens while
  // another seek is pending, the first seek is allowed to finish before
  // starting the second seek. As a result, there isn't really a good way to
  // cancel a pending one, so we don't do anything.
  NOTIMPLEMENTED();
}

void ManifestDemuxer::Seek(base::TimeDelta time,
                           PipelineStatusCallback status_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  CHECK(!pending_seek_);

  pending_seek_ = std::move(status_cb);
  media_time_ = time;

  // Seeks and periodic updates are considered to be events. No two events may
  // be running at the same time. Seeks still need to happen however, so a seek
  // should be stored for later.
  if (has_pending_event_) {
    return;
  }

  SeekInternal();
}

void ManifestDemuxer::SeekInternal() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // SeekInternal can be delayed and potentially called after a pending event
  // finishes. Pending events should not restart playback which was stopped
  // prior to the seek being requested, so we can check that it's still 0.
  CHECK_EQ(current_playback_rate_, 0);

  has_pending_event_ = true;

  // Cancel any outstanding events, we don't want them interrupting us.
  cancelable_next_event_.Cancel();

  // ManifestDemuxer::Engine::Seek returns true if it cleared data from
  // ChunkDemuxer and needs a new call to `OnTimeUpdate`. If this is the
  // case, ChunkDemuxer won't finish it's seek process until new data is
  // appended as a result of the player event. However it's possible for that
  // data to come in chunks, so ChunkDemuxer might think it's ready to finish
  // it's seek when only some of the data has come in. As a result, we have to
  // wait for both `OnChunkDemuxerSeeked` AND `OnEngineSeekComplete` to finish.
  // Each of them will check |seek_waiting_on_engine_|, the first will clear the
  // flag, and the second will notice the cleared flag and finish the seek
  // process.
  seek_waiting_on_engine_ = impl_->Seek(media_time_);

  chunk_demuxer_->Seek(media_time_,
                       base::BindOnce(&ManifestDemuxer::OnChunkDemuxerSeeked,
                                      weak_factory_.GetWeakPtr()));

  if (seek_waiting_on_engine_) {
    TriggerEventWithTime(base::BindOnce(&ManifestDemuxer::OnEngineSeekComplete,
                                        weak_factory_.GetWeakPtr()),
                         media_time_);
  }
}

bool ManifestDemuxer::IsSeekable() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // The underlying wrapping ChunkDemuxer is seekable.
  return impl_->IsSeekable();
}

void ManifestDemuxer::Stop() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  impl_->Stop();
  chunk_demuxer_->Stop();
  impl_.reset();
  chunk_demuxer_.reset();
  cancelable_next_event_.Cancel();
}

base::TimeDelta ManifestDemuxer::GetStartTime() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Support time remapping for streams that start > 0.
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
  return chunk_demuxer_->GetTimelineOffset();
}

int64_t ManifestDemuxer::GetMemoryUsage() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Consider other potential significant memory usage
  // here of the player impl.
  int64_t demuxer_usage = chunk_demuxer_ ? chunk_demuxer_->GetMemoryUsage() : 0;
  int64_t impl_usage = impl_ ? impl_->GetMemoryUsage() : 0;
  return demuxer_usage + impl_usage;
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

void ManifestDemuxer::SetPlaybackRate(double rate) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  bool rate_increase = rate > current_playback_rate_;
  current_playback_rate_ = rate;
  if (!rate_increase || pending_seek_ || has_pending_event_) {
    return;
  }

  // If the playback rate increased and there isn't already something pending,
  // cancel the next event and set a new one.
  cancelable_next_event_.Cancel();
  TriggerEvent();
}

void ManifestDemuxer::OnError(PipelineStatus error) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  cancelable_next_event_.Cancel();

  if (pending_init_) {
    std::move(pending_init_).Run(std::move(error).AddHere());
    return;
  }

  if (pending_seek_) {
    std::move(pending_seek_).Run(std::move(error).AddHere());
    return;
  }

  host_->OnDemuxerError(std::move(error).AddHere());
  Stop();
}

ChunkDemuxer* ManifestDemuxer::GetChunkDemuxerForTesting() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  return chunk_demuxer_.get();
}

void ManifestDemuxer::OnChunkDemuxerOpened() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  demuxer_opened_ = true;
  MaybeCompleteInitialize();
}

void ManifestDemuxer::OnProgress() {}

void ManifestDemuxer::OnEngineInitialized(PipelineStatus status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!status.is_ok()) {
    OnError(std::move(status).AddHere());
    return;
  }
  engine_impl_ready_ = true;
  MaybeCompleteInitialize();
}

void ManifestDemuxer::MaybeCompleteInitialize() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!demuxer_opened_ || !engine_impl_ready_) {
    return;
  }

  TriggerEvent();
}

void ManifestDemuxer::TriggerEvent() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  auto queue_next_event = base::BindOnce(
      &ManifestDemuxer::OnEngineEventFinished, weak_factory_.GetWeakPtr());
  TriggerEventWithTime(std::move(queue_next_event), media_time_);
}

void ManifestDemuxer::TriggerEventWithTime(DelayCallback cb,
                                           base::TimeDelta current_time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  has_pending_event_ = true;
  impl_->OnTimeUpdate(current_time, current_playback_rate_,
                      base::BindPostTask(media_task_runner_, std::move(cb)));
}

void ManifestDemuxer::OnEngineEventFinished(base::TimeDelta delay) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // There should always be an outstanding player event when this method is
  // called. Player events get set in the Trigger methods, which happen as part
  // of the time-based player loop, and as part of the seek process.
  CHECK(has_pending_event_);
  has_pending_event_ = false;

  // If there is a pending seek, execute it now. Seeking always calls
  // |OnEngineEventFinished| when it is finished.
  if (pending_seek_) {
    SeekInternal();
    return;
  }

  if (delay != kNoTimestamp) {
    // Schedule an event to take place again after a delay.
    cancelable_next_event_.Reset(base::BindOnce(&ManifestDemuxer::TriggerEvent,
                                                weak_factory_.GetWeakPtr()));
    media_task_runner_->PostDelayedTask(
        FROM_HERE, cancelable_next_event_.callback(), delay);
  }
}

void ManifestDemuxer::OnChunkDemuxerInitialized(PipelineStatus init_status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!pending_init_) {
    OnError(std::move(init_status));
    return;
  }
  std::move(pending_init_).Run(std::move(init_status));
}

void ManifestDemuxer::OnChunkDemuxerSeeked(PipelineStatus seek_status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!pending_seek_) {
    OnError(std::move(seek_status));
    return;
  }

  if (!seek_status.is_ok()) {
    // If the seek is an error, then don't bother waiting for the
    // OnEngineSeekComplete call, just unset the flag and exit now.
    seek_waiting_on_engine_ = false;
    std::move(pending_seek_).Run(std::move(seek_status));
    return;
  }

  if (seek_waiting_on_engine_) {
    // If this flag was set, then there is a simultaneous wait for both
    // OnChunkDemuxerSeeked and OnEngineSeekComplete, and we were called first.
    // We should set the flag back to false, so that OnEngineSeekComplete can
    // finish the seeking process.
    seek_waiting_on_engine_ = false;
    return;
  }

  // Complete the seek with an ok-status. This function already handles non-ok
  // status results above.
  CompletePendingSeek();
}

void ManifestDemuxer::OnEngineSeekComplete(base::TimeDelta delay_time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!pending_seek_) {
    // OnChunkDemuxerSeeked returned earlier with an error, so we don't have
    // anything to do. Make sure that the seek waiting flag was cleaned up.
    CHECK_EQ(seek_waiting_on_engine_, false);
    return;
  }

  if (seek_waiting_on_engine_) {
    // If the flag is still set, we were called before OnChunkDemuxerSeeked.
    // Set the flag back to false, so that when OnChunkDemuxerSeeked is called,
    // it will finish up and execute the pending_seek_ callback.
    seek_waiting_on_engine_ = false;
    return;
  }

  // Complete the seek with an ok-status. If the chunk demuxer had failed to
  // seek, it would have already posted the `pending_seek_` call with its
  // failure status.
  CompletePendingSeek();
}

void ManifestDemuxer::CompletePendingSeek() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  CHECK(pending_seek_);
  std::move(pending_seek_).Run(OkStatus());

  // Schedule a new event ASAP to populate data.
  OnEngineEventFinished(base::Seconds(0));
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
