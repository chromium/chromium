// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_audio_renderer.h"

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/renderer_client.h"
#include "media/cdm/fuchsia/fuchsia_cdm_context.h"
#include "media/fuchsia/common/decrypting_sysmem_buffer_stream.h"
#include "media/fuchsia/common/passthrough_sysmem_buffer_stream.h"

namespace {

// nullopt is returned in case the codec is not supported. nullptr is returned
// for uncompressed PCM streams.
std::optional<std::unique_ptr<fuchsia::media::Compression>>
GetFuchsiaCompressionFromDecoderConfig(media::AudioDecoderConfig config) {
  auto compression = std::make_unique<fuchsia::media::Compression>();
  switch (config.codec()) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case media::AudioCodec::kAAC:
      compression->type = fuchsia::media::AUDIO_ENCODING_AAC;
      break;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    case media::AudioCodec::kMP3:
      compression->type = fuchsia::media::AUDIO_ENCODING_MP3;
      break;
    case media::AudioCodec::kVorbis:
      compression->type = fuchsia::media::AUDIO_ENCODING_VORBIS;
      break;
    case media::AudioCodec::kOpus:
      compression->type = fuchsia::media::AUDIO_ENCODING_OPUS;
      break;
    case media::AudioCodec::kFLAC:
      compression->type = fuchsia::media::AUDIO_ENCODING_FLAC;
      break;
    case media::AudioCodec::kPCM:
      compression.reset();
      break;

    default:
      return std::nullopt;
  }

  if (!config.extra_data().empty()) {
    compression->parameters = config.extra_data();
  }

  return std::move(compression);
}

std::optional<fuchsia::media::AudioSampleFormat>
GetFuchsiaSampleFormatFromSampleFormat(media::SampleFormat sample_format) {
  switch (sample_format) {
    case media::kSampleFormatU8:
      return fuchsia::media::AudioSampleFormat::UNSIGNED_8;
    case media::kSampleFormatS16:
      return fuchsia::media::AudioSampleFormat::SIGNED_16;
    case media::kSampleFormatS24:
      return fuchsia::media::AudioSampleFormat::SIGNED_24_IN_32;
    case media::kSampleFormatF32:
      return fuchsia::media::AudioSampleFormat::FLOAT;

    default:
      return std::nullopt;
  }
}

// Helper that converts a PCM stream in kStreamFormatS24 to the layout
// expected by AudioConsumer (i.e. SIGNED_24_IN_32).
scoped_refptr<media::DecoderBuffer> PreparePcm24Buffer(
    scoped_refptr<media::DecoderBuffer> buffer) {
  static_assert(ARCH_CPU_LITTLE_ENDIAN,
                "Only little-endian CPUs are supported.");

  size_t samples = buffer->size() / 3;
  scoped_refptr<media::DecoderBuffer> result =
      base::MakeRefCounted<media::DecoderBuffer>(samples * 4);
  for (size_t i = 0; i < samples - 1; ++i) {
    reinterpret_cast<uint32_t*>(result->writable_data())[i] =
        *reinterpret_cast<const uint32_t*>(buffer->data() + i * 3) & 0x00ffffff;
  }
  size_t last_sample = samples - 1;
  reinterpret_cast<uint32_t*>(result->writable_data())[last_sample] =
      buffer->data()[last_sample * 3] |
      (buffer->data()[last_sample * 3 + 1] << 8) |
      (buffer->data()[last_sample * 3 + 2] << 16);

  result->set_timestamp(buffer->timestamp());
  result->set_duration(buffer->duration());

  if (buffer->decrypt_config())
    result->set_decrypt_config(buffer->decrypt_config()->Clone());

  return result;
}

}  // namespace

// Size of a single audio buffer: 100kB. It's enough to cover 100ms of PCM at
// 48kHz, 2 channels, 16 bps.
constexpr size_t kBufferSize = 100 * 1024;

// Total number of buffers. 16 is the maximum allowed by AudioConsumer.
constexpr size_t kNumBuffers = 16;

WebEngineAudioRenderer::WebEngineAudioRenderer(
    media::MediaLog* media_log,
    fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer_handle)
    : audio_consumer_handle_(std::move(audio_consumer_handle)) {
  DETACH_FROM_THREAD(thread_checker_);
}

WebEngineAudioRenderer::~WebEngineAudioRenderer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void WebEngineAudioRenderer::Initialize(media::DemuxerStream* stream,
                                        media::CdmContext* cdm_context,
                                        media::RendererClient* client,
                                        media::PipelineStatusCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!demuxer_stream_);

  DCHECK(!init_cb_);
  init_cb_ = std::move(init_cb);

  cdm_context_ = cdm_context;
  demuxer_stream_ = stream;
  client_ = client;

  audio_consumer_.Bind(std::move(audio_consumer_handle_));
  audio_consumer_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "AudioConsumer disconnected.";
    OnError(media::AUDIO_RENDERER_ERROR);
  });

  UpdateVolume();

  audio_consumer_.events().OnEndOfStream = [this]() { OnEndOfStream(); };
  RequestAudioConsumerStatus();

  InitializeStream();

  // Call `init_cb_`, unless it's been called by OnError().
  if (init_cb_) {
    std::move(init_cb_).Run(media::PIPELINE_OK);
  }
}

void WebEngineAudioRenderer::InitializeStream() {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  // AAC streams require bitstream conversion. Without it the demuxer may
  // produce decoded stream without ADTS headers which are required for AAC
  // streams in AudioConsumer.
  // TODO(crbug.com/40145747): Reconsider this logic.
  if (demuxer_stream_->audio_decoder_config().codec() ==
      media::AudioCodec::kAAC) {
    demuxer_stream_->EnableBitstreamConverter();
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  if (demuxer_stream_->audio_decoder_config().is_encrypted()) {
    if (!cdm_context_) {
      DLOG(ERROR) << "No cdm context for encrypted stream.";
      OnError(media::AUDIO_RENDERER_ERROR);
      return;
    }

    media::FuchsiaCdmContext* fuchsia_cdm =
        cdm_context_->GetFuchsiaCdmContext();
    if (fuchsia_cdm) {
      sysmem_buffer_stream_ = fuchsia_cdm->CreateStreamDecryptor(false);
    } else {
      sysmem_buffer_stream_ =
          std::make_unique<media::DecryptingSysmemBufferStream>(
              &sysmem_allocator_, cdm_context_, media::Decryptor::kAudio);
    }

  } else {
    sysmem_buffer_stream_ =
        std::make_unique<media::PassthroughSysmemBufferStream>(
            &sysmem_allocator_);
  }

  sysmem_buffer_stream_->Initialize(this, kBufferSize, kNumBuffers);
}

void WebEngineAudioRenderer::UpdateVolume() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(audio_consumer_);
  if (!volume_control_) {
    audio_consumer_->BindVolumeControl(volume_control_.NewRequest());
    volume_control_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "VolumeControl disconnected.";
    });
  }
  volume_control_->SetVolume(volume_);
}

void WebEngineAudioRenderer::OnBuffersAcquired(
    std::vector<media::VmoBuffer> buffers,
    const fuchsia::sysmem2::SingleBufferSettings&) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  input_buffers_ = std::move(buffers);
  InitializeStreamSink();

  while (!delayed_packets_.empty()) {
    auto packet = std::move(delayed_packets_.front());
    delayed_packets_.pop_front();
    SendInputPacket(std::move(packet));
  }

  if (has_delayed_end_of_stream_) {
    has_delayed_end_of_stream_ = false;
    OnSysmemBufferStreamEndOfStream();
  }
}

void WebEngineAudioRenderer::InitializeStreamSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!stream_sink_);

  // Clone |buffers| to pass to StreamSink.
  std::vector<zx::vmo> vmos_for_stream_sink;
  vmos_for_stream_sink.reserve(input_buffers_.size());
  for (media::VmoBuffer& buffer : input_buffers_) {
    vmos_for_stream_sink.push_back(buffer.Duplicate(/*writable=*/false));
  }

  auto config = demuxer_stream_->audio_decoder_config();
  auto compression = GetFuchsiaCompressionFromDecoderConfig(config);
  if (!compression) {
    LOG(ERROR) << "Unsupported audio codec: " << GetCodecName(config.codec());
    OnError(media::AUDIO_RENDERER_ERROR);
    return;
  }

  fuchsia::media::AudioStreamType stream_type;
  stream_type.channels = config.channels();
  stream_type.frames_per_second = config.samples_per_second();

  // Set sample_format for uncompressed streams.
  if (!compression.value()) {
    std::optional<fuchsia::media::AudioSampleFormat> sample_format =
        GetFuchsiaSampleFormatFromSampleFormat(config.sample_format());
    if (!sample_format) {
      LOG(ERROR) << "Unsupported sample format: "
                 << SampleFormatToString(config.sample_format());
      OnError(media::AUDIO_RENDERER_ERROR);
      return;
    }
    stream_type.sample_format = sample_format.value();
  } else {
    // For compressed formats sample format is determined by the decoder, but
    // this field is still required in AudioStreamType.
    stream_type.sample_format = fuchsia::media::AudioSampleFormat::SIGNED_16;
  }

  audio_consumer_->CreateStreamSink(
      std::move(vmos_for_stream_sink), std::move(stream_type),
      std::move(compression).value(), stream_sink_.NewRequest());

  if (GetPlaybackState() == PlaybackState::kStartPending)
    StartAudioConsumer();

  ScheduleBufferTimers();
}

void WebEngineAudioRenderer::UpdatePlaybackRate() {
  float target_rate =
      (GetPlaybackState() == PlaybackState::kPaused) ? 0.0 : playback_rate_;

  audio_consumer_->SetRate(target_rate);

  // AudioConsumer will update media timeline asynchronously. That update is
  // processed in OnAudioConsumerStatusChanged(). This might cause the clock to
  // go back. It's not desirable, e.g. because VideoRenderer could drop some
  // video frames that should be shown when the stream is resumed. To avoid this
  // issue, update the timeline synchronously. OnAudioConsumerStatusChanged()
  // will still process the update from AudioConsumer to save the position when
  // the stream was actually paused, but that update would not move the clock
  // backward.
  if (target_rate != 0.0) {
    return;
  }

  base::AutoLock lock(timeline_lock_);
  media_pos_ = CurrentMediaTimeLocked();
  reference_time_ = base::TimeTicks::Now();
  media_delta_ = 0;
}

media::TimeSource* WebEngineAudioRenderer::GetTimeSource() {
  return this;
}

void WebEngineAudioRenderer::Flush(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  FlushInternal();
  renderer_started_ = false;

  std::move(callback).Run();
}

void WebEngineAudioRenderer::StartPlaying() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  renderer_started_ = true;
  ScheduleBufferTimers();
}

void WebEngineAudioRenderer::SetVolume(float volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  volume_ = volume;
  if (audio_consumer_)
    UpdateVolume();
}

void WebEngineAudioRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  // TODO(crbug.com/40150050): Implement at some later date after we've vetted
  // the API shape and usefulness outside of fuchsia.
  NOTIMPLEMENTED();
}

void WebEngineAudioRenderer::SetPreservesPitch(bool preserves_pitch) {
  // TODO(crbug.com/40868390): Implement this.
  NOTIMPLEMENTED();
}

void WebEngineAudioRenderer::
    SetWasPlayedWithUserActivationAndHighMediaEngagement(
        bool was_played_with_user_activation_and_high_media_engagement) {
  // WebEngine does not use this signal. This is currently only used by the Live
  // Caption feature.
  NOTIMPLEMENTED_LOG_ONCE();
}

void WebEngineAudioRenderer::StartTicking() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  switch (GetPlaybackState()) {
    case PlaybackState::kStopped: {
      base::AutoLock lock(timeline_lock_);
      SetPlaybackState(PlaybackState::kStartPending);
      break;
    }

    case PlaybackState::kStartPending:
    case PlaybackState::kStarting:
    case PlaybackState::kPlaying:
      NOTREACHED();

    case PlaybackState::kPaused: {
      // If the stream was paused then we can unpause it without restarting
      // AudioConsumer.
      {
        base::AutoLock lock(timeline_lock_);
        SetPlaybackState(PlaybackState::kPlaying);
      }
      UpdatePlaybackRate();
      return;
    }
  }

  // If StreamSink hasn't been created yet, then delay starting AudioConsumer
  // until StreamSink is created.
  if (!stream_sink_) {
    return;
  }

  StartAudioConsumer();
}

void WebEngineAudioRenderer::StartAudioConsumer() {
  DCHECK(stream_sink_);
  DCHECK_EQ(GetPlaybackState(), PlaybackState::kStartPending);

  fuchsia::media::AudioConsumerStartFlags flags{};
  if (demuxer_stream_->liveness() == media::StreamLiveness::kLive) {
    flags = fuchsia::media::AudioConsumerStartFlags::LOW_LATENCY;
  }

  base::TimeDelta media_pos;
  {
    base::AutoLock lock(timeline_lock_);
    media_pos = media_pos_;
    SetPlaybackState(PlaybackState::kStarting);
  }

  audio_consumer_->Start(flags, fuchsia::media::NO_TIMESTAMP,
                         media_pos.ToZxDuration());
  UpdatePlaybackRate();
}

void WebEngineAudioRenderer::StopTicking() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(GetPlaybackState() != PlaybackState::kStopped);

  switch (GetPlaybackState()) {
    case PlaybackState::kStopped:
    case PlaybackState::kPaused:
      NOTREACHED();

    case PlaybackState::kStartPending: {
      base::AutoLock lock(timeline_lock_);
      SetPlaybackState(PlaybackState::kStopped);
      break;
    }

    case PlaybackState::kStarting:
    case PlaybackState::kPlaying: {
      {
        base::AutoLock lock(timeline_lock_);
        SetPlaybackState(PlaybackState::kPaused);
      }
      UpdatePlaybackRate();
      break;
    }
  }
}

void WebEngineAudioRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  playback_rate_ = playback_rate;
  UpdatePlaybackRate();
}

void WebEngineAudioRenderer::SetMediaTime(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool stop_audio_consumer = false;

  {
    base::AutoLock lock(timeline_lock_);

    if (GetPlaybackState() == PlaybackState::kPaused) {
      SetPlaybackState(PlaybackState::kStopped);
      stop_audio_consumer = true;
    }
    DCHECK(GetPlaybackState() == PlaybackState::kStopped);

    media_pos_ = time;

    // Reset reference timestamp. This is necessary to ensure that the correct
    // value is returned from GetWallClockTimes() until playback is resumed:
    // GetWallClockTimes() is required to return 0 wall clock between
    // SetMediaTime() and StartTicking().
    reference_time_ = base::TimeTicks();
  }

  if (stop_audio_consumer) {
    audio_consumer_->Stop();
  }

  FlushInternal();
  ScheduleBufferTimers();
}

base::TimeDelta WebEngineAudioRenderer::CurrentMediaTime() {
  base::AutoLock lock(timeline_lock_);
  if (!IsTimeMoving())
    return media_pos_;

  return CurrentMediaTimeLocked();
}

bool WebEngineAudioRenderer::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  wall_clock_times->reserve(media_timestamps.size());

  base::AutoLock lock(timeline_lock_);

  const bool is_time_moving = IsTimeMoving();

  if (media_timestamps.empty()) {
    wall_clock_times->push_back(is_time_moving ? base::TimeTicks::Now()
                                               : reference_time_);
    return is_time_moving;
  }

  base::TimeTicks wall_clock_base =
      is_time_moving ? reference_time_ : base::TimeTicks::Now();

  for (base::TimeDelta timestamp : media_timestamps) {
    auto relative_pos = timestamp - media_pos_;
    if (is_time_moving) {
      // See https://fuchsia.dev/reference/fidl/fuchsia.media#formulas .
      relative_pos = relative_pos * reference_delta_ / media_delta_;
    }
    wall_clock_times->push_back(wall_clock_base + relative_pos);
  }

  return is_time_moving;
}

WebEngineAudioRenderer::PlaybackState
WebEngineAudioRenderer::GetPlaybackState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return state_;
}

void WebEngineAudioRenderer::SetPlaybackState(PlaybackState state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  state_ = state;
}

void WebEngineAudioRenderer::OnError(media::PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  audio_consumer_.Unbind();
  stream_sink_.Unbind();
  sysmem_buffer_stream_.reset();
  read_timer_.Stop();
  out_of_buffer_timer_.Stop();
  renderer_started_ = false;

  if (is_demuxer_read_pending_) {
    drop_next_demuxer_read_result_ = true;
  }

  if (init_cb_) {
    std::move(init_cb_).Run(status);
  } else if (client_) {
    client_->OnError(status);
  }
}

void WebEngineAudioRenderer::RequestAudioConsumerStatus() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_consumer_->WatchStatus(fit::bind_member(
      this, &WebEngineAudioRenderer::OnAudioConsumerStatusChanged));
}

void WebEngineAudioRenderer::OnAudioConsumerStatusChanged(
    fuchsia::media::AudioConsumerStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (status.has_error()) {
    LOG(ERROR) << "fuchsia::media::AudioConsumer reported an error";
    OnError(media::AUDIO_RENDERER_ERROR);
    return;
  }

  bool reschedule_timers = false;

  if (status.has_presentation_timeline()) {
    if (GetPlaybackState() != PlaybackState::kStopped) {
      base::AutoLock lock(timeline_lock_);
      if (GetPlaybackState() == PlaybackState::kStarting) {
        SetPlaybackState(PlaybackState::kPlaying);
      }
      reference_time_ = base::TimeTicks::FromZxTime(
          status.presentation_timeline().reference_time);
      media_pos_ = base::TimeDelta::FromZxDuration(
          status.presentation_timeline().subject_time);
      reference_delta_ = status.presentation_timeline().reference_delta;
      media_delta_ = status.presentation_timeline().subject_delta;

      reschedule_timers = true;
    }
  }

  if (status.has_min_lead_time()) {
    auto new_min_lead_time =
        base::TimeDelta::FromZxDuration(status.min_lead_time());
    DCHECK(!new_min_lead_time.is_zero());
    if (new_min_lead_time != min_lead_time_) {
      min_lead_time_ = new_min_lead_time;
      reschedule_timers = true;
    }
  }
  if (status.has_max_lead_time()) {
    auto new_max_lead_time =
        base::TimeDelta::FromZxDuration(status.max_lead_time());
    DCHECK(!new_max_lead_time.is_zero());
    if (new_max_lead_time != max_lead_time_) {
      max_lead_time_ = new_max_lead_time;
      reschedule_timers = true;
    }
  }

  if (reschedule_timers) {
    ScheduleBufferTimers();
  }

  RequestAudioConsumerStatus();
}

void WebEngineAudioRenderer::ScheduleBufferTimers() {
  std::vector<base::TimeDelta> media_timestamps;
  if (!last_packet_timestamp_.is_min()) {
    media_timestamps.push_back(last_packet_timestamp_);
  }
  std::vector<base::TimeTicks> wall_clock_times;
  bool is_time_moving = GetWallClockTimes(media_timestamps, &wall_clock_times);

  ScheduleReadDemuxerStream(is_time_moving, wall_clock_times[0]);
  ScheduleOutOfBufferTimer(is_time_moving, wall_clock_times[0]);
}

void WebEngineAudioRenderer::ScheduleReadDemuxerStream(
    bool is_time_moving,
    base::TimeTicks end_of_buffer_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  read_timer_.Stop();

  if (!renderer_started_ || !demuxer_stream_ || is_demuxer_read_pending_ ||
      is_at_end_of_stream_) {
    return;
  }

  base::TimeTicks next_read_time;

  // If playback is not active then there is no need to buffer more.
  if (!is_time_moving) {
    // Check if we have buffered more than |max_lead_time_|.
    if (end_of_buffer_time >= base::TimeTicks::Now() + max_lead_time_) {
      return;
    }
  }

  // Schedule the next read at the time when the buffer size will be below
  // `max_lead_time_` (may be in the past).
  next_read_time = end_of_buffer_time - max_lead_time_;

  read_timer_.Start(FROM_HERE, next_read_time,
                    base::BindOnce(&WebEngineAudioRenderer::ReadDemuxerStream,
                                   base::Unretained(this)));
}

void WebEngineAudioRenderer::ScheduleOutOfBufferTimer(
    bool is_time_moving,
    base::TimeTicks end_of_buffer_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  out_of_buffer_timer_.Stop();

  if (buffer_state_ == media::BUFFERING_HAVE_NOTHING || !is_time_moving ||
      is_at_end_of_stream_) {
    return;
  }

  // Time when the `stream_sink_` will run out of buffer.
  base::TimeTicks out_of_buffer_time = end_of_buffer_time - min_lead_time_;

  out_of_buffer_timer_.Start(
      FROM_HERE, out_of_buffer_time,
      base::BindOnce(&WebEngineAudioRenderer::SetBufferState,
                     base::Unretained(this), media::BUFFERING_HAVE_NOTHING),
      base::subtle::DelayPolicy::kFlexibleNoSooner);
}

void WebEngineAudioRenderer::ReadDemuxerStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(demuxer_stream_);
  DCHECK(!is_demuxer_read_pending_);

  is_demuxer_read_pending_ = true;
  demuxer_stream_->Read(
      1, base::BindOnce(&WebEngineAudioRenderer::OnDemuxerStreamReadDone,
                        weak_factory_.GetWeakPtr()));
}
void WebEngineAudioRenderer::OnDemuxerStreamReadDone(
    media::DemuxerStream::Status read_status,
    media::DemuxerStream::DecoderBufferVector buffers) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_demuxer_read_pending_);
  DCHECK_LE(buffers.size(), 1u)
      << "ReadDemuxerStream() only reads a single buffer.";

  is_demuxer_read_pending_ = false;

  if (drop_next_demuxer_read_result_) {
    drop_next_demuxer_read_result_ = false;
    ScheduleBufferTimers();
    return;
  }

  if (read_status != media::DemuxerStream::kOk) {
    if (read_status == media::DemuxerStream::kError) {
      OnError(media::PIPELINE_ERROR_READ);
    } else if (read_status == media::DemuxerStream::kConfigChanged) {
      stream_sink_.Unbind();

      // Re-initialize the stream for the new config.
      InitializeStream();

      // Continue reading the stream. Decryptor won't finish output buffer
      // initialization until it starts receiving data on the input.
      ScheduleBufferTimers();

      client_->OnAudioConfigChange(demuxer_stream_->audio_decoder_config());
    } else {
      DCHECK_EQ(read_status, media::DemuxerStream::kAborted);
    }
    return;
  }

  scoped_refptr<media::DecoderBuffer> buffer = std::move(buffers[0]);
  DCHECK(buffer);

  if (buffer->end_of_stream()) {
    is_at_end_of_stream_ = true;
  } else {
    if (buffer->size() > kBufferSize) {
      DLOG(ERROR) << "Demuxer returned buffer that is too big: "
                  << buffer->size();
      OnError(media::AUDIO_RENDERER_ERROR);
      return;
    }

    last_packet_timestamp_ = buffer->timestamp();
    if (buffer->duration() != media::kNoTimestamp)
      last_packet_timestamp_ += buffer->duration();
  }

  // Update layout for 24-bit PCM streams.
  if (!buffer->end_of_stream() &&
      demuxer_stream_->audio_decoder_config().codec() ==
          media::AudioCodec::kPCM &&
      demuxer_stream_->audio_decoder_config().sample_format() ==
          media::kSampleFormatS24) {
    buffer = PreparePcm24Buffer(std::move(buffer));
  }

  sysmem_buffer_stream_->EnqueueBuffer(std::move(buffer));

  ScheduleBufferTimers();
}

void WebEngineAudioRenderer::SendInputPacket(
    media::StreamProcessorHelper::IoPacket packet) {
  const auto packet_size = packet.size();
  fuchsia::media::StreamPacket stream_packet;
  stream_packet.payload_buffer_id = packet.buffer_index();
  stream_packet.pts = packet.timestamp().ToZxDuration();
  stream_packet.payload_offset = packet.offset();
  stream_packet.payload_size = packet_size;

  stream_sink_->SendPacket(
      std::move(stream_packet),
      [this, packet = std::make_unique<media::StreamProcessorHelper::IoPacket>(
                 std::move(packet))]() mutable {
        OnStreamSendDone(std::move(packet));
      });

  // AudioConsumer doesn't report exact time when the data is decoded, but it's
  // safe to report it as decoded right away since the packet is expected to be
  // decoded soon after AudioConsumer receives it.
  media::PipelineStatistics stats;
  stats.audio_bytes_decoded = packet_size;
  client_->OnStatisticsUpdate(stats);
}

void WebEngineAudioRenderer::OnStreamSendDone(
    std::unique_ptr<media::StreamProcessorHelper::IoPacket> packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Check if we need to update buffering state after sending more than
  // |min_lead_time_| to the AudioConsumer.
  if (buffer_state_ == media::BUFFERING_HAVE_NOTHING) {
    std::vector<base::TimeTicks> wall_clock_times;
    GetWallClockTimes({packet->timestamp()}, &wall_clock_times);
    base::TimeDelta relative_buffer_pos =
        wall_clock_times[0] - base::TimeTicks::Now();
    if (relative_buffer_pos >= min_lead_time_) {
      SetBufferState(media::BUFFERING_HAVE_ENOUGH);

      // Reschedule timers to ensure that the state is changed back to
      // `BUFFERING_HAVE_NOTHING` when necessary.
      ScheduleBufferTimers();
    }
  }
}

void WebEngineAudioRenderer::SetBufferState(
    media::BufferingState buffer_state) {
  if (buffer_state != buffer_state_) {
    buffer_state_ = buffer_state;
    client_->OnBufferingStateChange(buffer_state_,
                                    media::BUFFERING_CHANGE_REASON_UNKNOWN);
  }
}

void WebEngineAudioRenderer::FlushInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(GetPlaybackState() == PlaybackState::kStopped ||
         GetPlaybackState() == PlaybackState::kPaused || is_at_end_of_stream_);

  if (stream_sink_)
    stream_sink_->DiscardAllPacketsNoReply();

  SetBufferState(media::BUFFERING_HAVE_NOTHING);
  last_packet_timestamp_ = base::TimeDelta::Min();
  read_timer_.Stop();
  out_of_buffer_timer_.Stop();
  is_at_end_of_stream_ = false;

  if (is_demuxer_read_pending_) {
    drop_next_demuxer_read_result_ = true;
  }
}

void WebEngineAudioRenderer::OnEndOfStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->OnEnded();
}

bool WebEngineAudioRenderer::IsTimeMoving() {
  return state_ == PlaybackState::kPlaying && media_delta_ > 0;
}

base::TimeDelta WebEngineAudioRenderer::CurrentMediaTimeLocked() {
  // Calculate media position using formula specified by the TimelineFunction.
  // See https://fuchsia.dev/reference/fidl/fuchsia.media#formulas .
  return media_pos_ + (base::TimeTicks::Now() - reference_time_) *
                          media_delta_ / reference_delta_;
}

void WebEngineAudioRenderer::OnSysmemBufferStreamBufferCollectionToken(
    fuchsia::sysmem2::BufferCollectionTokenPtr token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Drop old buffers.
  input_buffers_.clear();
  stream_sink_.Unbind();

  // Acquire buffers for the new buffer collection.
  input_buffer_collection_ =
      sysmem_allocator_.BindSharedCollection(std::move(token));
  fuchsia::sysmem2::BufferCollectionConstraints buffer_constraints =
      media::VmoBuffer::GetRecommendedConstraints(kNumBuffers, kBufferSize,
                                                  /*writable=*/false);
  input_buffer_collection_->Initialize(std::move(buffer_constraints),
                                       "CrAudioRenderer");
  input_buffer_collection_->AcquireBuffers(base::BindOnce(
      &WebEngineAudioRenderer::OnBuffersAcquired, base::Unretained(this)));
}

void WebEngineAudioRenderer::OnSysmemBufferStreamOutputPacket(
    media::StreamProcessorHelper::IoPacket packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (stream_sink_) {
    SendInputPacket(std::move(packet));
  } else {
    // The packet will be sent after StreamSink is connected.
    delayed_packets_.push_back(std::move(packet));
  }
}

void WebEngineAudioRenderer::OnSysmemBufferStreamEndOfStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_at_end_of_stream_);

  // Stream sink is not bound yet, queue EOS request until then.
  if (!stream_sink_) {
    has_delayed_end_of_stream_ = true;
    return;
  }

  stream_sink_->EndOfStream();

  // No more data is going to be buffered. Update buffering state to ensure
  // RendererImpl starts playback in case it was waiting for buffering to
  // finish.
  SetBufferState(media::BUFFERING_HAVE_ENOUGH);
}

void WebEngineAudioRenderer::OnSysmemBufferStreamError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OnError(media::AUDIO_RENDERER_ERROR);
}

void WebEngineAudioRenderer::OnSysmemBufferStreamNoKey() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->OnWaiting(media::WaitingReason::kNoDecryptionKey);
}
