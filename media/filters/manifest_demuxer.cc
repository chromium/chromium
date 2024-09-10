// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/manifest_demuxer.h"

#include <optional>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
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
  impl_->Stop();
  impl_.reset();
  streams_.clear();
  chunk_demuxer_.reset();
}

ManifestDemuxer::ManifestDemuxer(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    base::RepeatingCallback<void(base::TimeDelta)> request_seek,
    std::unique_ptr<ManifestDemuxer::Engine> impl,
    MediaLog* media_log)
    : request_seek_(std::move(request_seek)),
      media_log_(media_log->Clone()),
      media_task_runner_(std::move(media_task_runner)),
      impl_(std::move(impl)) {
        media_log_->AddMessage(MediaLogMessageLevel::kINFO,
          "Demuxing stream using ManifestDemuxer");
      }

std::vector<DemuxerStream*> ManifestDemuxer::GetAllStreams() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // For each stream that ChunkDemuxer returns, we need to wrap it so that we
  // can grab the timestamp. Chunk demuxer's streams live forever, so ours
  // might as well also live forever, even if that leaks a small amount of
  // memory.
  // TODO(crbug.com/40057824): Rearchitect the demuxer stream ownership model to
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
  impl_->AbortPendingReads(base::DoNothing());
}

void ManifestDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {
  if (!media_task_runner_->RunsTasksInCurrentSequence()) {
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ManifestDemuxer::StartWaitingForSeek,
                                  weak_factory_.GetWeakPtr(), seek_time));
    return;
  }
  media_time_ = seek_time;
  chunk_demuxer_->StartWaitingForSeek(seek_time);
  impl_->StartWaitingForSeek();
}

void ManifestDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
  if (!media_task_runner_->RunsTasksInCurrentSequence()) {
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ManifestDemuxer::CancelPendingSeek,
                                  weak_factory_.GetWeakPtr(), seek_time));
    return;
  }

  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Since cancellation happens from the main thread, it's possible for a
  // function order to look like:
  // Main Thread                           | Media Thread
  // --------------------------------------|-----------------------------------
  // CancelPendingSeek                     |
  //         |                             |   CompletePendingSeek
  //          `----------------------------|-> CancelPendingSeek
  // so `pending_seek_` might already be called. If `pending_seek_` is still
  // pending, then canceling the chunk demuxer pending seek should execute
  // its callback immediately with a success status, and we'd just then be left
  // waiting for the engine to finish.
  // TODO(crbug.com/40057824): Make the engine cancelable as well.
  if (pending_seek_) {
    AbortPendingReads();
    chunk_demuxer_->CancelPendingSeek(seek_time);
  }
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

  has_pending_event_ = true;

  // Cancel any outstanding events, we don't want them interrupting us.
  cancelable_next_event_.Cancel();

  impl_->Seek(media_time_, base::BindOnce(&ManifestDemuxer::OnEngineSeeked,
                                          weak_factory_.GetWeakPtr()));
}

bool ManifestDemuxer::IsSeekable() const {
  return impl_->IsSeekable();
}

void ManifestDemuxer::Stop() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  cancelable_next_event_.Cancel();
  impl_->Stop();
  chunk_demuxer_->Stop();
  host_ = nullptr;
}

base::TimeDelta ManifestDemuxer::GetStartTime() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug.com/40057824): Support time remapping for streams that start >
  // 0.
  return base::TimeDelta();
}

base::Time ManifestDemuxer::GetTimelineOffset() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug.com/40057824): Implement this with the value of the
  // EXT-X-PROGRAM-DATETIME tag.
  // TODO(crbug.com/40057824): Moderate that tag with respect to any underlying
  // streams' nonzero timeline offsets that the wrapped ChunkDemuxer may have?
  // And should wrapped ChunkDemuxer's enforcement that any specified (non-null)
  // offset across multiple ChunkDemuxer::OnSourceInitDone() match be relaxed if
  // its wrapped by an HLS demuxer which might ignore those offsets?
  return chunk_demuxer_->GetTimelineOffset();
}

int64_t ManifestDemuxer::GetMemoryUsage() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug.com/40057824): Consider other potential significant memory usage
  // here of the player impl.
  int64_t demuxer_usage = chunk_demuxer_ ? chunk_demuxer_->GetMemoryUsage() : 0;
  int64_t impl_usage = impl_ ? impl_->GetMemoryUsage() : 0;
  return demuxer_usage + impl_usage;
}

std::optional<container_names::MediaContainerName>
ManifestDemuxer::GetContainerForMetrics() const {
  // TODO(crbug.com/40057824): Consider how this is used. HLS can involve
  // multiple stream types (mp2t, mp4, etc). Refactor to report something
  // useful.
  return std::nullopt;
}

void ManifestDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  chunk_demuxer_->OnEnabledAudioTracksChanged(
      MapTrackIds(track_ids), curr_time,
      base::BindOnce(&ManifestDemuxer::MapDemuxerStreams,
                     weak_factory_.GetWeakPtr(),
                     std::move(change_completed_cb)));
}

void ManifestDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  chunk_demuxer_->OnSelectedVideoTrackChanged(
      MapTrackIds(track_ids), curr_time,
      base::BindOnce(&ManifestDemuxer::MapDemuxerStreams,
                     weak_factory_.GetWeakPtr(),
                     std::move(change_completed_cb)));
}

void ManifestDemuxer::SetPlaybackRate(double rate) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  bool rate_increase = rate > current_playback_rate_;
  current_playback_rate_ = rate;
  if (has_pending_event_ || pending_seek_) {
    return;
  }

  if (rate_increase || (rate == 0 && !IsSeekable())) {
    // If the playback rate increased, or it was a pause of live content,
    // cancel the next event and set a new one.
    cancelable_next_event_.Cancel();
    TriggerEvent();
  }
}

bool ManifestDemuxer::AddRole(std::string_view role,
                              RelaxedParserSupportedType mime) {
  CHECK(chunk_demuxer_);
  if (ChunkDemuxer::kOk !=
      chunk_demuxer_->AddAutoDetectedCodecsId(std::string(role), mime)) {
    return false;
  }
  chunk_demuxer_->SetParseWarningCallback(
      std::string(role),
      base::BindRepeating(&ManifestDemuxer::OnChunkDemuxerParseWarning,
                          weak_factory_.GetWeakPtr(), std::string(role)));
  chunk_demuxer_->SetTracksWatcher(
      std::string(role),
      base::BindRepeating(&ManifestDemuxer::OnChunkDemuxerTracksChanged,
                          weak_factory_.GetWeakPtr(), std::string(role)));
  return true;
}

void ManifestDemuxer::RemoveRole(std::string_view role) {
  chunk_demuxer_->RemoveId(std::string(role));
}

void ManifestDemuxer::SetSequenceMode(std::string_view role,
                                      bool sequence_mode) {
  CHECK(chunk_demuxer_);
  return chunk_demuxer_->SetSequenceMode(std::string(role), sequence_mode);
}

void ManifestDemuxer::SetDuration(double duration) {
  CHECK(chunk_demuxer_);
  return chunk_demuxer_->SetDuration(duration);
}

Ranges<base::TimeDelta> ManifestDemuxer::GetBufferedRanges(
    std::string_view role) {
  CHECK(chunk_demuxer_);
  return chunk_demuxer_->GetBufferedRanges(std::string(role));
}

void ManifestDemuxer::Remove(std::string_view role,
                             base::TimeDelta start,
                             base::TimeDelta end) {
  chunk_demuxer_->Remove(std::string(role), start, end);
}

void ManifestDemuxer::RemoveAndReset(std::string_view role,
                                     base::TimeDelta start,
                                     base::TimeDelta end,
                                     base::TimeDelta* offset) {
  CHECK(chunk_demuxer_);
  Remove(role, start, end);
  chunk_demuxer_->ResetParserState(std::string(role), start, end, offset);
  chunk_demuxer_->AbortPendingReads();
}

void ManifestDemuxer::SetGroupStartIfParsingAndSequenceMode(
    std::string_view role,
    base::TimeDelta start) {
  CHECK(chunk_demuxer_);
  if (!chunk_demuxer_->IsParsingMediaSegment(std::string(role))) {
    chunk_demuxer_->SetGroupStartTimestampIfInSequenceMode(std::string(role),
                                                           start);
  }
}

void ManifestDemuxer::EvictCodedFrames(std::string_view role,
                                       base::TimeDelta time,
                                       size_t data_size) {
  CHECK(chunk_demuxer_);
  if (!chunk_demuxer_->EvictCodedFrames(std::string(role), time, data_size)) {
    MEDIA_LOG(ERROR, media_log_) << "EvictCodedFrames(" << role << ") failed.";
  }
}

bool ManifestDemuxer::AppendAndParseData(std::string_view role,
                                         base::TimeDelta end,
                                         base::TimeDelta* offset,
                                         base::span<const uint8_t> data) {
  CHECK(chunk_demuxer_);
  if (!chunk_demuxer_->AppendToParseBuffer(std::string(role), data)) {
    return false;
  }
  while (true) {
    switch (chunk_demuxer_->RunSegmentParserLoop(
        std::string(role), base::TimeDelta(), end, offset)) {
      case StreamParser::ParseStatus::kSuccess:
        return true;
      case StreamParser::ParseStatus::kSuccessHasMoreData:
        break;  // Keep parsing.
      default:
        return false;
    }
  }
}

void ManifestDemuxer::ResetParserState(std::string_view role,
                                       base::TimeDelta end,
                                       base::TimeDelta* offset) {
  CHECK(chunk_demuxer_);
  return chunk_demuxer_->ResetParserState(std::string(role), base::TimeDelta(),
                                          end, offset);
}

void ManifestDemuxer::OnError(PipelineStatus error) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  cancelable_next_event_.Cancel();
  weak_factory_.InvalidateWeakPtrs();

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

void ManifestDemuxer::RequestSeek(base::TimeDelta time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  request_seek_.Run(time);
}

void ManifestDemuxer::SetGroupStartTimestamp(std::string_view role,
                                             base::TimeDelta time) {
  chunk_demuxer_->SetGroupStartTimestampIfInSequenceMode(std::string(role),
                                                         time);
}

void ManifestDemuxer::SetEndOfStream() {
  chunk_demuxer_->MarkEndOfStream(PIPELINE_OK);
}

void ManifestDemuxer::UnsetEndOfStream() {
  chunk_demuxer_->UnmarkEndOfStream();
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

void ManifestDemuxer::OnEngineSeeked(SeekResponse seek_status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  CHECK(pending_seek_);
  if (!seek_status.has_value()) {
    std::move(pending_seek_).Run(std::move(seek_status).error().AddHere());
    return;
  }

  chunk_demuxer_->Seek(media_time_,
                       base::BindOnce(&ManifestDemuxer::OnChunkDemuxerSeeked,
                                      weak_factory_.GetWeakPtr()));

  if (std::move(seek_status).value() == SeekState::kNeedsData) {
    // Buffers need to be refilled, or ChunkDemuxer::Seek will never complete.
    can_complete_seek_ = false;
    TriggerEventWithTime(base::BindOnce(&ManifestDemuxer::OnSeekBuffered,
                                        weak_factory_.GetWeakPtr()),
                         media_time_);
  }
}

void ManifestDemuxer::OnSeekBuffered(base::TimeDelta delay_time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!pending_seek_) {
    // ChunkDemuxer::Seek replied with an error, and has already reset the flag.
    CHECK(can_complete_seek_);
    return;
  }

  if (!can_complete_seek_) {
    // ChunkDemuxer::Seek has not yet replied. Set the flag to true and exit.
    can_complete_seek_ = true;
    return;
  }

  // Finish seeking and schedule a new event ASAP to continue.
  std::move(pending_seek_).Run(OkStatus());
  OnEngineEventFinished(base::Seconds(0));
}

void ManifestDemuxer::OnChunkDemuxerSeeked(PipelineStatus seek_status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  CHECK(pending_seek_);
  if (!seek_status.is_ok()) {
    can_complete_seek_ = true;
    std::move(pending_seek_).Run(std::move(seek_status));
    return;
  }

  if (!can_complete_seek_) {
    // The engine should reply shortly after finishing the event which
    // repopulated ChunkDemuxer's buffers. Reset the flag to allow that reply
    // to finish the seek process.
    can_complete_seek_ = true;
    return;
  }

  // Finish seeking and schedule a new event ASAP to continue.
  std::move(pending_seek_).Run(std::move(seek_status));
  OnEngineEventFinished(base::Seconds(0));
}

void ManifestDemuxer::OnChunkDemuxerParseWarning(
    std::string role,
    SourceBufferParseWarning warning) {
  MEDIA_LOG(WARNING, media_log_)
      << "ParseWarning (" << role << "): " << static_cast<int>(warning);
}

void ManifestDemuxer::OnChunkDemuxerTracksChanged(
    std::string role,
    std::unique_ptr<MediaTracks> tracks) {
  for (const auto& track : tracks->tracks()) {
    if (track->enabled()) {
      if (track->type() == MediaTrack::Type::kVideo) {
        internal_video_track_id_ = track->track_id();
      } else if (track->type() == MediaTrack::Type::kAudio) {
        internal_audio_track_id_ = track->track_id();
      }
    }
  }
}

void ManifestDemuxer::OnEncryptedMediaData(EmeInitDataType type,
                                           const std::vector<uint8_t>& data) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  OnError(PIPELINE_ERROR_INVALID_STATE);
}

void ManifestDemuxer::OnDemuxerStreamRead(
    DemuxerStream::ReadCB wrapped_read_cb,
    DemuxerStream::Status status,
    DemuxerStream::DecoderBufferVector buffers) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (status == DemuxerStream::Status::kOk) {
    // The entire vector must be checked as timestamps are often out of order.
    for (const auto& buffer : buffers) {
      if (!buffer->end_of_stream() && buffer->timestamp() > media_time_) {
        media_time_ = buffer->timestamp();
      }
    }
  }

  std::move(wrapped_read_cb).Run(status, std::move(buffers));
}

void ManifestDemuxer::MapDemuxerStreams(
    TrackChangeCB cb,
    const std::vector<DemuxerStream*>& streams) {
  std::vector<DemuxerStream*> mapped_streams;
  for (const auto* const stream : streams) {
    mapped_streams.push_back(streams_.at(stream).get());
  }
  std::move(cb).Run(mapped_streams);
}

std::vector<MediaTrack::Id> ManifestDemuxer::MapTrackIds(
    const std::vector<MediaTrack::Id>& track_ids) {
  std::vector<MediaTrack::Id> chunk_demuxer_ids;
  for (const auto& track_id : track_ids) {
    // TODO(crbug/40057824): replace track binding when we expose multiple
    // tracks for renditions and variants.
    if (track_id.value() == "audio" && internal_audio_track_id_.has_value()) {
      chunk_demuxer_ids.push_back(*internal_audio_track_id_);
    }
    if (track_id.value() == "video" && internal_video_track_id_.has_value()) {
      chunk_demuxer_ids.push_back(*internal_video_track_id_);
    }
  }
  return chunk_demuxer_ids;
}

}  // namespace media
