// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_demuxer.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
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
#include "media/formats/common/opus_constants.h"

namespace media {

namespace {

// Number of bytes read from the start of the stream to determine the
// container, like `FFmpegGlue`.
constexpr size_t kContainerSniffSize = 8192;

// Reads from the current position of `source` until `buffer` is full or the
// stream ends. Returns the number of bytes read, or nullopt on a read error.
std::optional<size_t> ReadStreamStart(SymphoniaDataSourceBridge& source,
                                      base::span<uint8_t> buffer) {
  size_t total = 0;
  while (total < buffer.size()) {
    base::span<uint8_t> remaining = buffer.subspan(total);
    const int64_t bytes_read =
        source.Read(rust::Slice<uint8_t>(remaining.data(), remaining.size()));
    if (bytes_read < 0) {
      return std::nullopt;
    }
    if (bytes_read == 0) {
      break;
    }
    total += static_cast<size_t>(bytes_read);
  }
  return total;
}

// Returns the size of the ID3v2 tag at the start of `data`, including its
// header and footer, or 0 if `data` does not start with one.
size_t GetId3v2TagSize(base::span<const uint8_t> data) {
  // The header is "ID3", two version bytes, a flags byte, and the size of the
  // rest of the tag (excluding the header and footer) as four "syncsafe"
  // bytes with seven bits each.
  constexpr size_t kHeaderSize = 10;
  constexpr uint8_t kFooterPresentFlag = 0x10;
  if (data.size() < kHeaderSize || data[0] != 'I' || data[1] != 'D' ||
      data[2] != '3') {
    return 0;
  }
  size_t size = 0;
  for (const uint8_t byte : data.subspan(6u, 4u)) {
    if (byte & 0x80) {
      return 0;  // Not syncsafe, so not an ID3v2 tag.
    }
    size = (size << 7) | byte;
  }
  const bool has_footer = (data[5] & kFooterPresentFlag) != 0;
  return kHeaderSize + size + (has_footer ? kHeaderSize : 0);
}

// Returns the container of the stream read through `source`, whose position
// must be 0, and leaves the position at 0. Returns nullopt on a read or seek
// error.
std::optional<container_names::MediaContainerName> SniffContainer(
    SymphoniaDataSourceBridge& source) {
  std::array<uint8_t, kContainerSniffSize> buffer;
  std::optional<size_t> size = ReadStreamStart(source, buffer);
  if (!size) {
    return std::nullopt;
  }
  base::span<const uint8_t> data = base::span(buffer).first(*size);

  // `DetermineContainer()` reports any stream that starts with an ID3v2 tag as
  // MP3. Like FFmpeg's probe, look at what follows the tag instead.
  const size_t tag_size = GetId3v2TagSize(data);
  if (tag_size > 0) {
    if (tag_size < data.size()) {
      data = data.subspan(tag_size);
    } else if (source.Seek(tag_size)) {
      size = ReadStreamStart(source, buffer);
      if (!size) {
        return std::nullopt;
      }
      data = base::span(buffer).first(*size);
    } else {
      // The tag runs past the end of the stream.
      data = {};
    }
  }

  if (!source.Seek(0)) {
    return std::nullopt;
  }
  if (data.size() < container_names::kMinimumContainerSize) {
    return container_names::MediaContainerName::kContainerUnknown;
  }
  return container_names::DetermineContainer(data);
}

void RecordInitOutcome(SymphoniaDemuxer::InitOutcome outcome) {
  base::UmaHistogramEnumeration("Media.Audio.Symphonia.DemuxerInitResult",
                                outcome);
}

bool IsPcmCodec(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kPCM:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
    case AudioCodec::kPCM_ALAW:
    case AudioCodec::kPCM_MULAW:
      return true;
    default:
      return false;
  }
}

uint32_t SanitizeSymphoniaChannelMask(AudioCodec codec,
                                      uint32_t channel_mask,
                                      uint32_t channels) {
  // Symphonia's WAV, AIFF, and MP4 LPCM readers synthesize a `(1 << n) - 1`
  // prefix mask (`Position::from_count(n)`) when a PCM stream has no speaker
  // mask or `dwChannelMask == 0`. For 4 channels that yields `0xF`
  // (`CHANNEL_LAYOUT_3_1`) instead of `CHANNEL_LAYOUT_QUAD`. Clear synthesized
  // prefix masks on PCM tracks so `FromChannelMask()` falls back to `Guess()`,
  // while keeping explicit `WAVEFORMATEXTENSIBLE` masks like `0x33` or `0x60F`.
  if (IsPcmCodec(codec) && channels > 0 && channels < 32 &&
      channel_mask == ((uint32_t{1} << channels) - 1)) {
    return 0;
  }
  return channel_mask;
}

// TODO(crbug.com/550619039): Switch `codec_delay` to `uint32_t` once
// `AudioDecoderConfig::Initialize()` takes an unsigned delay.
int GetCodecDelayFrames(AudioCodec codec,
                        uint32_t delay_frames,
                        base::span<const uint8_t> extra_data) {
  if (codec != AudioCodec::kOpus) {
    // Only Opus uses pipeline decoder delay in Chromium. For other codecs
    // (such as AAC and MP3), delay/padding is either discarded via packet
    // discard padding or handled by the decoder.
    return 0;
  }
  if (delay_frames > 0) {
    return static_cast<int>(delay_frames);
  }
  // TODO(crbug.com/583935554): `symphonia-format-mkv` leaves `Track::delay`
  // unset when `CodecDelay` is omitted or only present in the `OpusHead`
  // identification header. Fall back to the 16-bit little-endian pre-skip
  // field in `OpusHead`.
  if (extra_data.size() >= OPUS_EXTRADATA_SKIP_SAMPLES_OFFSET + 2) {
    return base::U16FromLittleEndian(
        extra_data.subspan<OPUS_EXTRADATA_SKIP_SAMPLES_OFFSET, 2>());
  }
  return 0;
}

base::TimeDelta GetSeekPreroll(AudioCodec codec) {
  // RFC 7845 Section 5.1 specifies an 80 ms seek preroll for Opus.
  if (codec == AudioCodec::kOpus) {
    return base::Milliseconds(80);
  }
  return base::TimeDelta();
}

// Builds the `AudioDecoderConfig` for an audio `track`, or returns
// `std::nullopt` if its codec is unsupported or the config is invalid.
std::optional<AudioDecoderConfig> CreateAudioDecoderConfig(
    const SymphoniaTrackInfo& track) {
  const AudioCodec codec = SymphoniaCodecToAudioCodec(track.codec);
  if (codec == AudioCodec::kUnknown) {
    return std::nullopt;
  }

  std::vector<uint8_t> extra_data(track.extra_data.begin(),
                                  track.extra_data.end());
  const int codec_delay =
      GetCodecDelayFrames(codec, track.delay_frames, extra_data);
  const base::TimeDelta seek_preroll = GetSeekPreroll(codec);

  AudioDecoderConfig config;
  config.Initialize(
      codec, SymphoniaCodecToSampleFormat(track.codec, track.bits_per_sample),
      ChannelLayoutConfig::FromChannelMask(
          SanitizeSymphoniaChannelMask(codec, track.channel_mask,
                                       track.channels),
          track.channels),
      track.sample_rate, std::move(extra_data), EncryptionScheme::kUnencrypted,
      seek_preroll, codec_delay);
  if (!config.IsValidConfig()) {
    return std::nullopt;
  }
  return config;
}

void AcknowledgeResetRequiredOnBlockingSequence(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    SymphoniaDemuxerBridge* demuxer) {
  CHECK(blocking_task_runner->RunsTasksInCurrentSequence());
  demuxer->acknowledge_reset_required();
}

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
    uint64_t time_us,
    int64_t track_id) {
  CHECK(blocking_task_runner->RunsTasksInCurrentSequence());
  // A negative track ID lets Symphonia pick the default track to seek on.
  return demuxer->seek_to(*source, time_us, track_id);
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
    std::unique_ptr<MediaLog> media_log,
    bool allow_fallback)
    : media_task_runner_(task_runner),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::USER_BLOCKING})),
      media_tracks_updated_cb_(std::move(media_tracks_updated_cb)),
      media_log_(std::move(media_log)),
      allow_fallback_(allow_fallback),
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
    SymphoniaDataSourceBridge* source_bridge,
    bool allow_fallback) {
  CHECK(blocking_task_runner->RunsTasksInCurrentSequence());

  container_names::MediaContainerName container =
      container_names::MediaContainerName::kContainerUnknown;
  if (allow_fallback) {
    // Determine the container from the start of the stream first, and only
    // probe with Symphonia if Symphonia demuxing is enabled for it.
    // Otherwise, report it so that the owner can fall back to another
    // demuxer.
    const std::optional<container_names::MediaContainerName> sniffed =
        SniffContainer(*source_bridge);
    if (!sniffed) {
      return base::unexpected(InitFailure{
          .status = PipelineStatus(DEMUXER_ERROR_COULD_NOT_OPEN,
                                   "Failed to read the start of the stream")});
    }
    container = *sniffed;
    if (!IsSymphoniaDemuxingEnabledForContainer(container)) {
      return base::unexpected(InitFailure{
          .status = PipelineStatus(DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED,
                                   "Unsupported container for Symphonia"),
          .sniffed_container = container});
    }
  }

  // Probe without extension or MIME type hints; Symphonia sniffs the format
  // from the stream contents. Any failure from here on is an error, since
  // Symphonia is enabled for the container or there is no fallback.
  auto init_result = init_symphonia_demuxer(
      *source_bridge, /*hint_extension=*/"", /*hint_mime=*/"");
  if (init_result.status != SymphoniaDemuxerInitStatus::Ok) {
    return base::unexpected(InitFailure{
        .status = PipelineStatus(DEMUXER_ERROR_COULD_NOT_OPEN,
                                 "Symphonia initialization failed: " +
                                     std::string(init_result.error_str)),
        .sniffed_container = container});
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
                     base::Unretained(source_bridge_.get()), allow_fallback_),
      base::BindOnce(&SymphoniaDemuxer::OnInitializeDone,
                     weak_factory_.GetWeakPtr()));
}

void SymphoniaDemuxer::OnInitializeDone(InitResultOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `Stop()` invalidates `weak_factory_`, so this never runs after it.
  CHECK(!is_stopped_);

  // Initialization failed: Symphonia demuxing is not enabled for the
  // container, or Symphonia could not read the container's headers.
  if (!result.has_value()) {
    InitFailure failure = std::move(result).error();
    if (failure.status == DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED) {
      FailInitializationForFallback(
          InitOutcome::kFallbackSniffedContainerDisabled,
          failure.sniffed_container, failure.status.message());
      return;
    }
    RecordInitOutcome(InitOutcome::kError);
    std::move(pending_init_cb_).Run(std::move(failure.status));
    return;
  }

  // The start of the stream can look like a different container than the one
  // Symphonia found, for example with two ID3 tags before FLAC data. Check the
  // container that Symphonia found too. The unused demuxer bridge is freed
  // with `result`.
  const container_names::MediaContainerName probed_container =
      SymphoniaFormatNameToContainerName(result->container_name);
  if (allow_fallback_ &&
      !IsSymphoniaDemuxingEnabledForContainer(probed_container)) {
    FailInitializationForFallback(
        InitOutcome::kFallbackProbedContainerDisabled, probed_container,
        "Symphonia demuxing is not enabled for " + result->container_name);
    return;
  }

  // TODO(crbug.com/583934381): Remove once video tracks are supported. Until
  // then, fall back so that files with video keep playing it.
  if (allow_fallback_ &&
      std::ranges::any_of(result->tracks, [](const SymphoniaTrackInfo& track) {
        return track.track_type == SymphoniaTrackType::Video;
      })) {
    FailInitializationForFallback(InitOutcome::kFallbackVideoTrack,
                                  probed_container,
                                  "Video tracks are not supported");
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
  container_name_.store(probed_container, std::memory_order_relaxed);
  host_->SetDuration(duration_);

  // Create a stream for each supported track.
  auto media_tracks = std::make_unique<MediaTracks>();
  for (const auto& track : result->tracks) {
    switch (track.track_type) {
      case SymphoniaTrackType::Audio:
        AddAudioStream(track, *media_tracks);
        break;
      default:
        // Subtitle and unknown tracks are not supported; the bridge reports
        // them but no `DemuxerStream` is created.
        MEDIA_LOG(WARNING, media_log_)
            << GetDisplayName() << ": ignoring unsupported non-audio track "
            << track.track_id;
        break;
    }
  }

  if (streams_.empty()) {
    RecordInitOutcome(InitOutcome::kError);
    std::move(pending_init_cb_)
        .Run(PipelineStatus(DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                            "No supported audio tracks found"));
    return;
  }

  // Like `FFmpegDemuxer` for media without a timeline offset, report liveness
  // as recorded only when the duration is known.
  const bool duration_known = duration_ != kInfiniteDuration;
  for (const auto& stream : streams_) {
    stream->set_liveness(duration_known ? StreamLiveness::kRecorded
                                        : StreamLiveness::kUnknown);
  }

  // Help the data source size its buffers. Symphonia does not report container
  // or stream bitrates, so estimate one from the file size, as `FFmpegDemuxer`
  // does when FFmpeg reports none.
  int64_t file_size = 0;
  if (duration_known && duration_.is_positive() &&
      data_source_->GetSize(&file_size) && file_size > 0) {
    // Multiply by `ToHz()` (1 / seconds) in `double` before scaling by 8, so
    // `file_size * 8` cannot overflow `int64_t` on huge files.
    const int bitrate = base::ClampRound(file_size * duration_.ToHz() * 8);
    if (bitrate > 0) {
      data_source_->SetBitrate(bitrate);
    }
  }

  // Chained Ogg streams can change config mid-stream in single-track
  // audio-only media; see `OnStreamResetRequired()`.
  if (probed_container == container_names::MediaContainerName::kContainerOgg &&
      streams_.size() == 1 && result->tracks.size() == 1) {
    streams_[0]->set_supports_config_changes(true);
  }

  // Publish the tracks, complete initialization, and start prefetching
  // packets.
  RecordInitOutcome(InitOutcome::kSuccess);
  media_tracks_updated_cb_.Run(std::move(media_tracks));
  std::move(pending_init_cb_).Run(PIPELINE_OK);
  ReadFrameIfNeeded();
}

void SymphoniaDemuxer::AddAudioStream(const SymphoniaTrackInfo& track,
                                      MediaTracks& media_tracks) {
  // Like `FFmpegDemuxer`, skip tracks that reuse a track ID. A malformed
  // container can declare the same ID twice, and `MediaTracks` requires
  // unique stream IDs.
  if (std::ranges::find(streams_, track.track_id,
                        &SymphoniaDemuxerStream::track_id) != streams_.end()) {
    return;
  }

  std::optional<AudioDecoderConfig> maybe_config =
      CreateAudioDecoderConfig(track);
  if (!maybe_config) {
    return;
  }
  const AudioDecoderConfig& config = *maybe_config;

  // Unretained is safe: `this` owns the stream, which runs the callback
  // synchronously and never outlives `this`.
  // Like `FFmpegDemuxer`, number tracks sequentially from 1, rather than
  // exposing container-specific IDs such as Ogg serial numbers.
  const auto media_track_id =
      static_cast<StreamParser::TrackId>(media_tracks.tracks().size() + 1);
  auto stream = std::make_unique<SymphoniaDemuxerStream>(
      track.track_id, MediaTrack::IntegralId(media_track_id), config, duration_,
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
      config, enabled, media_track_id, MediaTrack::Kind("main"),
      MediaTrack::Label(""), MediaTrack::Language(std::string(track.language)));
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
  if (pending_track_change_cb_) {
    std::move(pending_track_change_cb_).Run();
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
  last_buffered_timestamp_ = kNoTimestamp;
  pending_seek_cb_ = std::move(status_cb);
  int64_t seek_track_id = -1;
  for (const auto& stream : streams_) {
    stream->FlushBuffers();
    if (seek_track_id < 0 && stream->is_enabled()) {
      seek_track_id = static_cast<int64_t>(stream->track_id());
    }
  }

  base::TimeDelta seek_time = std::max(base::TimeDelta(), time);
  // Like `FFmpegDemuxer`, seek earlier by `seek_preroll()` for Opus so the
  // decoder receives enough preroll data to reconstruct accurate output at
  // `time`.
  for (const auto& stream : streams_) {
    if (stream->is_enabled() &&
        stream->audio_decoder_config().codec() == AudioCodec::kOpus) {
      seek_time =
          std::max(base::TimeDelta(),
                   seek_time - stream->audio_decoder_config().seek_preroll());
      break;
    }
  }
  const uint64_t seek_time_us =
      static_cast<uint64_t>(seek_time.InMicroseconds());

  // Unretained is safe: the bridges are only destroyed by a task that `Stop()`
  // posts to the same sequenced `blocking_task_runner_`, so they outlive this
  // task.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SeekOnBlockingSequence, blocking_task_runner_,
                     base::Unretained(&**demuxer_bridge_),
                     base::Unretained(source_bridge_.get()), seek_time_us,
                     seek_track_id),
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

  if (pending_track_change_cb_) {
    std::move(pending_track_change_cb_).Run();
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
  // `PipelineController` waits for each track change to finish before it
  // starts another one.
  CHECK(!pending_track_change_cb_);
  // Symphonia has no notion of selected tracks: it always demuxes packets for
  // every track, and packets for disabled streams are dropped on enqueue.
  SymphoniaDemuxerStream* enabled_stream = nullptr;
  bool newly_enabled = false;
  for (const auto& stream : streams_) {
    if (stream->type() != track_type) {
      continue;
    }
    const bool enable = track_id == stream->media_track_id();
    if (enable) {
      enabled_stream = stream.get();
      newly_enabled = !stream->is_enabled();
    }
    stream->SetEnabled(enable);
  }

  // A disabled stream dropped its packets, so a newly enabled stream has none
  // around `curr_time`. Like `FFmpegDemuxer`, seek back to `curr_time` and
  // flush the newly enabled stream. Other streams receive packets they may
  // already have, which the pipeline tolerates. A pending seek flushes every
  // stream and moves the read position anyway, so it covers this case.
  if (!newly_enabled || is_stopped_ || !demuxer_bridge_ || pending_seek_cb_) {
    std::move(change_completed_cb).Run(enabled_stream);
    ReadFrameIfNeeded();
    return;
  }

  is_eos_ = false;
  // Flush before seeking (clearing any prior `end_of_stream_` or `aborted_`
  // state); packet reads are paused while `pending_track_change_cb_` is set, so
  // nothing can enqueue until the seek finishes.
  enabled_stream->FlushBuffers();
  pending_track_change_cb_ =
      base::BindOnce(std::move(change_completed_cb), enabled_stream);
  const uint64_t seek_time_us =
      static_cast<uint64_t>(std::max(int64_t{0}, curr_time.InMicroseconds()));

  // Unretained is safe: the bridges are only destroyed by a task that `Stop()`
  // posts to the same sequenced `blocking_task_runner_`, so they outlive this
  // task.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SeekOnBlockingSequence, blocking_task_runner_,
                     base::Unretained(&**demuxer_bridge_),
                     base::Unretained(source_bridge_.get()), seek_time_us,
                     static_cast<int64_t>(enabled_stream->track_id())),
      base::BindOnce(&SymphoniaDemuxer::OnTrackChangeSeekDone,
                     weak_factory_.GetWeakPtr()));
}

void SymphoniaDemuxer::OnTrackChangeSeekDone(SymphoniaSeekResult seek_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_stopped_);
  CHECK(pending_track_change_cb_);

  // Re-enabling a track at or past the last packet is not an error: mark every
  // stream at end of stream, matching `OnSeekDone()`.
  if (seek_result.status == SymphoniaSeekStatus::OutOfRange) {
    is_eos_ = true;
    for (const auto& stream : streams_) {
      stream->SetEndOfStream();
    }
    std::move(pending_track_change_cb_).Run();
    return;
  }

  // TODO(crbug.com/550619039): Report seek failures for track changes too.
  // `FFmpegDemuxer` ignores them as well.
  if (seek_result.status != SymphoniaSeekStatus::Ok) {
    MEDIA_LOG(ERROR, media_log_)
        << GetDisplayName()
        << ": track change seek failed: " << seek_result.error_str;
  }

  std::move(pending_track_change_cb_).Run();
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
      pending_track_change_cb_ || !demuxer_bridge_ ||
      !StreamsHaveAvailableCapacity()) {
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
  if (pending_seek_cb_ || pending_track_change_cb_) {
    return;
  }

  if (packet_result.status == SymphoniaReadPacketStatus::EndOfStream) {
    OnEndOfStream();
    return;
  }

  if (packet_result.status == SymphoniaReadPacketStatus::ResetRequired) {
    OnStreamResetRequired(packet_result.tracks);
    return;
  }

  // Like `FFmpegDemuxer`, treat any other read error as the end of the stream,
  // so that truncated or partly corrupt media plays up to the point of
  // failure. DataSource read failures are reported separately by
  // `source_bridge_`.
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

  // A chained stream's track may have a new ID; route it to the stream it
  // continues.
  uint32_t track_id = packet_result.packet.track_id;
  if (chained_track_id_ == track_id) {
    CHECK_EQ(streams_.size(), 1u);
    track_id = streams_[0]->track_id();
  }

  // Drop packets for tracks without an enabled stream (e.g. video,
  // unsupported audio codecs, or disabled tracks) before inspecting timestamps
  // or updating `max_packet_end_time_`.
  auto it =
      std::ranges::find(streams_, track_id, &SymphoniaDemuxerStream::track_id);
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
  if (duration != kNoTimestamp && duration.is_positive()) {
    const base::TimeDelta packet_start = std::max(base::TimeDelta(), timestamp);
    // Connect adjacent packets in the same read run so microsecond rounding
    // (e.g. at 44.1 kHz) does not leave 1-us holes in `buffered_ranges_`.
    const base::TimeDelta range_start =
        (last_buffered_timestamp_ != kNoTimestamp &&
         last_buffered_timestamp_ <= packet_start)
            ? last_buffered_timestamp_
            : packet_start;
    const base::TimeDelta range_end = timestamp + duration;
    if (range_end > range_start) {
      buffered_ranges_.Add(range_start, range_end);
      last_buffered_timestamp_ = range_end;
      host_->OnBufferedTimeRangesChanged(buffered_ranges_);
    }
  }
  if (has_chained_stream_ && duration_ != kInfiniteDuration &&
      max_packet_end_time_ > duration_) {
    duration_ = max_packet_end_time_;
    host_->SetDuration(duration_);
  }

  // Like `FFmpegDemuxer`, raise a known duration if the stream turns out to be
  // longer. An unknown duration is set at end of stream instead.
  const base::TimeDelta estimate = (*it)->estimated_duration();
  if (duration_ != kInfiniteDuration && estimate != kNoTimestamp &&
      estimate > duration_) {
    duration_ = estimate;
    host_->SetDuration(duration_);
  }

  ReadFrameIfNeeded();
}

void SymphoniaDemuxer::OnEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_eos_ = true;
  // If the container did not report a duration, or a chained stream ran past
  // the first link's probed duration, use the end of the latest packet.
  const bool should_update_duration =
      max_packet_end_time_.is_positive() &&
      (duration_ == kInfiniteDuration ||
       (has_chained_stream_ && max_packet_end_time_ > duration_));
  if (should_update_duration) {
    duration_ = max_packet_end_time_;
    host_->SetDuration(duration_);
  }
  // Close any sub-packet rounding gap between the last demuxed packet and
  // `duration_`, without marking ranges skipped by a seek as buffered.
  if (last_buffered_timestamp_ != kNoTimestamp &&
      duration_ != kInfiniteDuration && duration_ > last_buffered_timestamp_) {
    buffered_ranges_.Add(last_buffered_timestamp_, duration_);
    last_buffered_timestamp_ = duration_;
    host_->OnBufferedTimeRangesChanged(buffered_ranges_);
  }
  for (const auto& stream : streams_) {
    stream->SetEndOfStream();
  }
}

void SymphoniaDemuxer::OnStreamResetRequired(
    const rust::Vec<SymphoniaTrackInfo>& tracks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(demuxer_bridge_);
  // Clear `pending_reset_required` before scheduling the next packet read.
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AcknowledgeResetRequiredOnBlockingSequence,
                                blocking_task_runner_,
                                base::Unretained(&**demuxer_bridge_)));

  // Like `FFmpegDemuxer`, only support a new stream (e.g. chained Ogg) in
  // single-track audio-only media, where it is unambiguous which stream the
  // new track continues.
  const SymphoniaTrackInfo* new_track = nullptr;
  if (streams_.size() == 1 && streams_[0]->SupportsConfigChanges() &&
      tracks.size() == 1 && tracks[0].track_type == SymphoniaTrackType::Audio) {
    new_track = &tracks[0];
  }
  std::optional<AudioDecoderConfig> config =
      new_track ? CreateAudioDecoderConfig(*new_track) : std::nullopt;
  if (!config) {
    MEDIA_LOG(ERROR, media_log_)
        << GetDisplayName() << ": unsupported mid-stream track change";
    is_eos_ = true;
    OnDemuxerError(PIPELINE_ERROR_DECODE);
    return;
  }

  has_chained_stream_ = true;
  SymphoniaDemuxerStream& stream = *streams_[0];
  chained_track_id_ = new_track->track_id != stream.track_id()
                          ? std::optional<uint32_t>(new_track->track_id)
                          : std::nullopt;
  stream.ChangeConfig(*std::move(config));
  ReadFrameIfNeeded();
}

void SymphoniaDemuxer::OnDemuxerError(PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_stopped_) {
    host_->OnDemuxerError(status);
  }
}

void SymphoniaDemuxer::FailInitializationForFallback(
    InitOutcome outcome,
    container_names::MediaContainerName container,
    std::string_view reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(allow_fallback_);
  CHECK(pending_init_cb_);
  CHECK(source_bridge_);

  RecordInitOutcome(outcome);
  base::UmaHistogramEnumeration(
      "Media.Audio.Symphonia.DemuxerFallbackContainer", container);

  // Probing has finished, so no read is in flight on `blocking_task_runner_`.
  // Release `data_source_` without stopping it, so `Stop()` leaves it usable
  // for the demuxer that `DemuxerManager` falls back to.
  source_bridge_->ReleaseDataSource();
  source_bridge_.reset();
  data_source_ = nullptr;

  MEDIA_LOG(INFO, media_log_)
      << GetDisplayName() << ": falling back to another demuxer: " << reason;
  std::move(pending_init_cb_)
      .Run(PipelineStatus(DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED,
                          std::string(reason), "container",
                          std::to_underlying(container)));
}

}  // namespace media
