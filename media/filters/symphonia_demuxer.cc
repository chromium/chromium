// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_demuxer.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/container_names.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_tracks.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/symphonia_data_source_bridge.h"
#include "media/filters/symphonia_demuxer_bridge.rs.h"
#include "media/filters/symphonia_demuxer_stream.h"
#include "media/filters/symphonia_demuxer_util.h"

namespace media {

namespace {

SymphoniaReadPacketResult ReadPacketOnBlockingSequence(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    SymphoniaDemuxerBridge* demuxer,
    SymphoniaDataSourceBridge* source) {
  CHECK(blocking_task_runner->RunsTasksInCurrentSequence());
  return demuxer->read_packet(*source);
}

SymphoniaSeekResult SeekOnBlockingSequence(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    SymphoniaDemuxerBridge* demuxer,
    SymphoniaDataSourceBridge* source,
    uint64_t time_us) {
  CHECK(blocking_task_runner->RunsTasksInCurrentSequence());
  // A track ID of -1 lets Symphonia pick the default track to seek on.
  return demuxer->seek_to(*source, time_us, /*track_id=*/-1);
}

// Adapts an owned `rust::Vec<uint8_t>` packet payload into a
// `DecoderBuffer::ExternalMemory` without copying.
class RustVecExternalMemory final : public DecoderBuffer::ExternalMemory {
 public:
  explicit RustVecExternalMemory(rust::Vec<uint8_t> vec)
      : vec_(std::move(vec)) {}
  ~RustVecExternalMemory() override = default;

  const base::span<const uint8_t> Span() const override {
    return base::span(vec_);
  }

 private:
  rust::Vec<uint8_t> vec_;
};

}  // namespace

SymphoniaDemuxer::SymphoniaDemuxer(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    DataSource* data_source,
    const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
    MediaTracksUpdatedCB media_tracks_updated_cb,
    std::unique_ptr<MediaLog> media_log)
    : media_task_runner_(task_runner),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::USER_BLOCKING})),
      media_tracks_updated_cb_(std::move(media_tracks_updated_cb)),
      media_log_(std::move(media_log)),
      data_source_(data_source) {
  CHECK(data_source_);
  CHECK(media_log_);
  // Constructed on the main thread, but used on the media sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SymphoniaDemuxer::~SymphoniaDemuxer() {
  // The owner normally calls `Stop()` on the media sequence and then destroys
  // the demuxer on another thread, so only stop here if that did not happen.
  if (!is_stopped_) {
    Stop();
  }
}

std::string SymphoniaDemuxer::GetDisplayName() const {
  return "SymphoniaDemuxer";
}

DemuxerType SymphoniaDemuxer::GetDemuxerType() const {
  return DemuxerType::kSymphoniaDemuxer;
}

// static
SymphoniaDemuxer::InitResultOrError
SymphoniaDemuxer::InitializeOnBlockingSequence(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    SymphoniaDataSourceBridge* source_bridge) {
  CHECK(blocking_task_runner->RunsTasksInCurrentSequence());

  // Probe without extension or MIME type hints; Symphonia sniffs the format
  // from the stream contents.
  auto init_result = init_symphonia_demuxer(
      *source_bridge, /*hint_extension=*/"", /*hint_mime=*/"");
  if (init_result.status != SymphoniaDemuxerInitStatus::Ok) {
    return base::unexpected(std::string(init_result.error_str));
  }

  return InitResult{
      .duration_us = init_result.duration_us,
      .container_name = std::string(init_result.container_name),
      .tracks = std::move(init_result.tracks),
      .demuxer_bridge = std::move(init_result.demuxer),
  };
}

void SymphoniaDemuxer::Initialize(DemuxerHost* host,
                                  PipelineStatusCallback status_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(host);
  CHECK(!pending_init_cb_);
  CHECK(!source_bridge_);
  host_ = host;
  pending_init_cb_ = std::move(status_cb);
  persistent_weak_this_ = persistent_weak_factory_.GetWeakPtr();

  // The source bridge is created here, rather than on the blocking sequence,
  // so that `Stop()` can reset it even while probing is still in progress.
  // DataSource read errors are reported from the blocking sequence;
  // `SymphoniaDataSourceBridge` posts them back to the media sequence where it
  // was created. Use `persistent_weak_factory_` so errors are still reported
  // after `AbortPendingReads()`.
  source_bridge_ = SymphoniaDataSourceBridge::Create(
      data_source_.get(), blocking_task_runner_,
      base::BindRepeating(&SymphoniaDemuxer::OnDemuxerError,
                          persistent_weak_factory_.GetWeakPtr(),
                          PIPELINE_ERROR_READ));

  // Unretained is safe: resetting `source_bridge_` in `Stop()` schedules its
  // destruction on the same sequenced `blocking_task_runner_`, so it outlives
  // this task.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SymphoniaDemuxer::InitializeOnBlockingSequence,
                     blocking_task_runner_,
                     base::Unretained(source_bridge_.get())),
      base::BindOnce(&SymphoniaDemuxer::OnInitializeDone,
                     weak_factory_.GetWeakPtr()));
}

void SymphoniaDemuxer::OnInitializeDone(InitResultOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `Stop()` invalidates `weak_factory_`, so this never runs after it.
  CHECK(!is_stopped_);

  // Probing failed: Symphonia did not recognize the container or could not
  // read its headers.
  if (!result.has_value()) {
    std::move(pending_init_cb_)
        .Run(PipelineStatus(
            DEMUXER_ERROR_COULD_NOT_OPEN,
            "Symphonia initialization failed: " + std::move(result).error()));
    return;
  }

  // Take ownership of the demuxer bridge. From here on it is only touched by
  // tasks posted to `blocking_task_runner_`.
  demuxer_bridge_ = std::move(result->demuxer_bridge);
  // Symphonia reports 0 when neither the container nor any track knows its
  // duration, for example an MP3 without a Xing or VBRI header. Like
  // `FFmpegDemuxer`, report an unknown duration until end of stream.
  duration_ = result->duration_us > 0 ? base::Microseconds(result->duration_us)
                                      : kInfiniteDuration;
  container_name_.store(
      SymphoniaFormatNameToContainerName(result->container_name),
      std::memory_order_relaxed);
  host_->SetDuration(duration_);

  // Create a stream for each supported track.
  auto media_tracks = std::make_unique<MediaTracks>();
  for (const auto& track : result->tracks) {
    switch (track.track_type) {
      case SymphoniaTrackType::Audio:
        AddAudioStream(track, *media_tracks);
        break;
      default:
        // TODO(crbug.com/583934381): Video tracks are not yet supported; the
        // bridge reports them but no `DemuxerStream` is created.
        MEDIA_LOG(WARNING, media_log_)
            << GetDisplayName() << ": ignoring unsupported non-audio track "
            << track.track_id;
        break;
    }
  }

  if (streams_.empty()) {
    std::move(pending_init_cb_)
        .Run(PipelineStatus(DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                            "No supported audio tracks found"));
    return;
  }

  // Publish the tracks, complete initialization, and start prefetching
  // packets.
  media_tracks_updated_cb_.Run(std::move(media_tracks));
  std::move(pending_init_cb_).Run(PIPELINE_OK);
  ReadFrameIfNeeded();
}

void SymphoniaDemuxer::AddAudioStream(const SymphoniaTrackInfo& track,
                                      MediaTracks& media_tracks) {
  const AudioCodec codec = SymphoniaCodecToAudioCodec(track.codec);
  if (codec == AudioCodec::kUnknown) {
    return;
  }

  // Like `FFmpegDemuxer`, skip tracks that reuse a track ID. A malformed
  // container can declare the same ID twice, and `MediaTracks` requires
  // unique stream IDs.
  if (std::ranges::find(streams_, track.track_id,
                        &SymphoniaDemuxerStream::track_id) != streams_.end()) {
    return;
  }

  std::vector<uint8_t> extra_data(track.extra_data.begin(),
                                  track.extra_data.end());
  AudioDecoderConfig config(codec, SymphoniaCodecToSampleFormat(track.codec),
                            ChannelLayoutConfig::Guess(track.channels),
                            track.sample_rate, std::move(extra_data),
                            EncryptionScheme::kUnencrypted);
  if (!config.IsValidConfig()) {
    return;
  }

  // Unretained is safe: `this` owns the stream, which runs the callback
  // synchronously and never outlives `this`.
  auto stream = std::make_unique<SymphoniaDemuxerStream>(
      track.track_id, config, duration_,
      base::BindRepeating(&SymphoniaDemuxer::ReadFrameIfNeeded,
                          base::Unretained(this)));

  // Like `FFmpegDemuxer`, start with only the first supported audio track
  // enabled. Symphonia demuxes every track, so enabling more would buffer
  // packets for tracks the renderer never reads.
  const bool enabled = streams_.empty();
  stream->SetEnabled(enabled);
  // Symphonia reports a language for some containers, such as MKV and MP4, but
  // never a track name.
  // TODO(crbug.com/583935468): Set the label once Symphonia exposes track
  // names.
  MediaTrack* media_track = media_tracks.AddAudioTrack(
      config, enabled, static_cast<StreamParser::TrackId>(track.track_id),
      MediaTrack::Kind("main"), MediaTrack::Label(""),
      MediaTrack::Language(std::string(track.language)));
  media_track->set_id(stream->media_track_id());
  streams_.push_back(std::move(stream));
}

void SymphoniaDemuxer::AbortPendingReads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopped_) {
    return;
  }
  // Invalidating `weak_factory_` below would also drop the init reply, so the
  // pipeline must not abort reads before initialization finishes.
  CHECK(!pending_init_cb_);

  for (const auto& stream : streams_) {
    stream->Abort();
  }

  // Drop replies from any in-flight read or seek, and unblock the blocking
  // sequence if it is waiting on the DataSource. Calling
  // `data_source_->Abort()` cancels only the in-flight read so subsequent seeks
  // and reads still succeed.
  weak_factory_.InvalidateWeakPtrs();
  if (data_source_) {
    data_source_->Abort();
  }
  is_read_pending_ = false;

  if (pending_seek_cb_) {
    std::move(pending_seek_cb_).Run(PIPELINE_OK);
  }
}

void SymphoniaDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {}

void SymphoniaDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
  if (media_task_runner_->RunsTasksInCurrentSequence()) {
    AbortPendingReads();
    return;
  }
  // Don't call `GetWeakPtr()` here since we are on the wrong sequence.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SymphoniaDemuxer::AbortPendingReads,
                                persistent_weak_this_));
}

void SymphoniaDemuxer::Seek(base::TimeDelta time,
                            PipelineStatusCallback status_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopped_) {
    std::move(status_cb).Run(PIPELINE_ERROR_ABORT);
    return;
  }
  // The pipeline only seeks after a successful `Initialize()`, one seek at a
  // time.
  CHECK(demuxer_bridge_);
  CHECK(!pending_seek_cb_);

  is_eos_ = false;
  pending_seek_cb_ = std::move(status_cb);
  for (const auto& stream : streams_) {
    stream->FlushBuffers();
  }

  const uint64_t seek_time_us =
      static_cast<uint64_t>(std::max(int64_t{0}, time.InMicroseconds()));

  // Unretained is safe: the bridges are only destroyed by a task that `Stop()`
  // posts to the same sequenced `blocking_task_runner_`, so they outlive this
  // task.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SeekOnBlockingSequence, blocking_task_runner_,
                     base::Unretained(&**demuxer_bridge_),
                     base::Unretained(source_bridge_.get()), seek_time_us),
      base::BindOnce(&SymphoniaDemuxer::OnSeekDone,
                     weak_factory_.GetWeakPtr()));
}

void SymphoniaDemuxer::OnSeekDone(SymphoniaSeekResult seek_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `Stop()` and `AbortPendingReads()` run `pending_seek_cb_` and invalidate
  // `weak_factory_`, so this only runs for a live seek.
  CHECK(!is_stopped_);

  // Seeking past the last packet is not an error: the media just ends there.
  if (seek_result.status == SymphoniaSeekStatus::OutOfRange) {
    is_eos_ = true;
    for (const auto& stream : streams_) {
      stream->SetEndOfStream();
    }
    std::move(pending_seek_cb_).Run(PIPELINE_OK);
    return;
  }

  if (seek_result.status != SymphoniaSeekStatus::Ok) {
    std::move(pending_seek_cb_)
        .Run(PipelineStatus(
            PIPELINE_ERROR_READ,
            "Symphonia seek failed: " + std::string(seek_result.error_str)));
    return;
  }

  std::move(pending_seek_cb_).Run(PIPELINE_OK);
  ReadFrameIfNeeded();
}

bool SymphoniaDemuxer::IsSeekable() const {
  return true;
}

void SymphoniaDemuxer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopped_) {
    return;
  }
  is_stopped_ = true;

  if (pending_init_cb_) {
    std::move(pending_init_cb_).Run(PIPELINE_ERROR_ABORT);
  }

  if (pending_seek_cb_) {
    std::move(pending_seek_cb_).Run(PIPELINE_ERROR_ABORT);
  }

  for (const auto& stream : streams_) {
    stream->Stop();
  }

  // Resetting `source_bridge_` stops `data_source_`, unblocks any pending read,
  // and schedules bridge destruction on `blocking_task_runner_`. If `Stop()` is
  // called before `Initialize()`, stop `data_source_` directly.
  if (source_bridge_) {
    source_bridge_.reset();
  } else if (data_source_) {
    data_source_->Stop();
  }
  data_source_ = nullptr;

  if (demuxer_bridge_) {
    blocking_task_runner_->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(
                       std::exchange(demuxer_bridge_, std::nullopt)));
  }

  weak_factory_.InvalidateWeakPtrsAndDoom();
  persistent_weak_factory_.InvalidateWeakPtrsAndDoom();
}

base::TimeDelta SymphoniaDemuxer::GetStartTime() const {
  return base::TimeDelta();
}

base::Time SymphoniaDemuxer::GetTimelineOffset() const {
  return base::Time();
}

int64_t SymphoniaDemuxer::GetMemoryUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t total = 0;
  for (const auto& stream : streams_) {
    total += stream->GetMemoryUsage();
  }
  return total;
}

std::optional<container_names::MediaContainerName>
SymphoniaDemuxer::GetContainerForMetrics() const {
  return container_name_.load(std::memory_order_relaxed);
}

void SymphoniaDemuxer::OnTracksChanged(DemuxerStream::Type track_type,
                                       std::optional<MediaTrack::Id> track_id,
                                       base::TimeDelta curr_time,
                                       TrackChangeCB change_completed_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Symphonia has no notion of selected tracks: it always demuxes packets for
  // every track, and packets for disabled streams are dropped on enqueue.
  // TODO(crbug.com/583935964): Seek a re-enabled stream to `curr_time`.
  SymphoniaDemuxerStream* enabled_stream = nullptr;
  for (const auto& stream : streams_) {
    if (stream->type() != track_type) {
      continue;
    }
    const bool enable = track_id == stream->media_track_id();
    stream->SetEnabled(enable);
    if (enable) {
      enabled_stream = stream.get();
    }
  }
  std::move(change_completed_cb).Run(enabled_stream);
  ReadFrameIfNeeded();
}

void SymphoniaDemuxer::SetPlaybackRate(double rate) {}

std::vector<raw_ptr<DemuxerStream>> SymphoniaDemuxer::GetAllStreams() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Enabled streams first, followed by disabled streams.
  std::vector<raw_ptr<DemuxerStream>> result;
  result.reserve(streams_.size());
  for (const auto& stream : streams_) {
    result.push_back(stream.get());
  }
  std::ranges::stable_partition(result, [](const raw_ptr<DemuxerStream>& s) {
    return static_cast<const SymphoniaDemuxerStream*>(s.get())->is_enabled();
  });
  return result;
}

void SymphoniaDemuxer::ReadFrameIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopped_ || is_eos_ || is_read_pending_ || pending_seek_cb_ ||
      !demuxer_bridge_ || !StreamsHaveAvailableCapacity()) {
    return;
  }

  is_read_pending_ = true;
  // Unretained is safe: the bridges are only destroyed by a task that `Stop()`
  // posts to the same sequenced `blocking_task_runner_`, so they outlive this
  // task.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadPacketOnBlockingSequence, blocking_task_runner_,
                     base::Unretained(&**demuxer_bridge_),
                     base::Unretained(source_bridge_.get())),
      base::BindOnce(&SymphoniaDemuxer::OnReadPacketDone,
                     weak_factory_.GetWeakPtr()));
}

bool SymphoniaDemuxer::StreamsHaveAvailableCapacity() const {
  // TODO(crbug.com/584369243): Also stop reading once `GetMemoryUsage()` goes
  // over `GetDemuxerMemoryLimit()`, like `FFmpegDemuxer`.
  return std::ranges::any_of(streams_, [](const auto& stream) {
    return stream->is_enabled() && stream->HasAvailableCapacity();
  });
}

void SymphoniaDemuxer::OnReadPacketDone(
    SymphoniaReadPacketResult packet_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_stopped_);
  is_read_pending_ = false;

  // A seek started while this read was in flight; drop the stale packet.
  if (pending_seek_cb_) {
    return;
  }

  if (packet_result.status == SymphoniaReadPacketStatus::EndOfStream) {
    OnEndOfStream();
    return;
  }

  // Like `FFmpegDemuxer`, treat any read error as the end of the stream, so
  // that truncated or partly corrupt media plays up to the point of failure.
  // DataSource read failures are reported separately by `source_bridge_`.
  // TODO(crbug.com/583935452): Support chained Ogg streams, which report
  // `ResetRequired` at each new logical stream.
  if (packet_result.status != SymphoniaReadPacketStatus::Ok) {
    MEDIA_LOG(DEBUG, media_log_)
        << GetDisplayName()
        << ": read_packet failed: " << packet_result.error_str;
    OnEndOfStream();
    return;
  }

  // Zero-length packets carry no audio and cannot be wrapped by
  // `DecoderBuffer::FromExternalMemory()`.
  if (packet_result.packet.data.empty()) {
    ReadFrameIfNeeded();
    return;
  }

  // Drop packets for tracks without an enabled stream (e.g. video,
  // unsupported audio codecs, or disabled tracks) before inspecting timestamps
  // or updating `max_packet_end_time_`.
  auto it = std::ranges::find(streams_, packet_result.packet.track_id,
                              &SymphoniaDemuxerStream::track_id);
  if (it == streams_.end() || !(*it)->is_enabled()) {
    ReadFrameIfNeeded();
    return;
  }

  // A malformed container can make the Rust side produce timestamps that do
  // not fit a `base::TimeDelta`. Like `FFmpegDemuxerStream::EnqueuePacket()`,
  // reject packets without a valid timestamp, and drop invalid durations.
  // TODO(crbug.com/583935070): Match the rest of `FFmpegDemuxer`'s packet
  // handling, such as very short durations and packets that go backwards.
  const base::TimeDelta timestamp =
      base::Microseconds(packet_result.packet.pts_us);
  if (timestamp == kNoTimestamp || timestamp == kInfiniteDuration) {
    OnDemuxerError(PipelineStatus(DEMUXER_ERROR_COULD_NOT_PARSE,
                                  "Symphonia packet has an invalid timestamp"));
    return;
  }
  base::TimeDelta duration =
      base::Microseconds(packet_result.packet.duration_us);
  if (duration.is_negative() || duration == kInfiniteDuration) {
    duration = kNoTimestamp;
  }

  auto buffer =
      DecoderBuffer::FromExternalMemory(std::make_unique<RustVecExternalMemory>(
          std::move(packet_result.packet.data)));
  buffer->set_timestamp(timestamp);
  buffer->set_duration(duration);
  buffer->set_is_key_frame(packet_result.packet.is_keyframe);
  const base::TimeDelta end_time =
      duration == kNoTimestamp ? timestamp : timestamp + duration;
  if (end_time != kInfiniteDuration) {
    max_packet_end_time_ = std::max(max_packet_end_time_, end_time);
  }

  // Symphonia only exposes gapless trimming; other side data (e.g. codec
  // specific side data) is not available through the bridge.
  if (packet_result.packet.trim_start_us > 0 ||
      packet_result.packet.trim_end_us > 0) {
    buffer->set_discard_padding(DecoderBuffer::DiscardPadding(
        base::Microseconds(packet_result.packet.trim_start_us),
        base::Microseconds(packet_result.packet.trim_end_us)));
  }

  (*it)->EnqueueBuffer(std::move(buffer));

  ReadFrameIfNeeded();
}

void SymphoniaDemuxer::OnEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_eos_ = true;
  // If the container did not report a duration, the end of the latest packet
  // is the best duration available.
  if (duration_ == kInfiniteDuration &&
      max_packet_end_time_ > base::TimeDelta()) {
    duration_ = max_packet_end_time_;
    host_->SetDuration(duration_);
  }
  for (const auto& stream : streams_) {
    stream->SetEndOfStream();
  }
}

void SymphoniaDemuxer::OnDemuxerError(PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_stopped_) {
    host_->OnDemuxerError(status);
  }
}

}  // namespace media
