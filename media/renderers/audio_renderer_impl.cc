// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/audio_renderer_impl.h"

#include <math.h>
#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_converter.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_mixing_matrix.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_client.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/renderer_client.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/audio_clock.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

AudioRendererImpl::AudioRendererImpl(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    AudioRendererSink* sink,
    const CreateAudioDecodersCB& create_audio_decoders_cb,
    MediaLog* media_log,
    MediaPlayerLoggingID media_player_id,
    SpeechRecognitionClient* speech_recognition_client)
    : task_runner_(task_runner),
      expecting_config_changes_(false),
      sink_(sink),
      media_log_(media_log),
      player_id_(media_player_id),
      client_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      last_audio_memory_usage_(0),
      last_decoded_sample_rate_(0),
      last_decoded_channel_layout_(CHANNEL_LAYOUT_NONE),
      is_encrypted_(false),
      last_decoded_channels_(0),
      volume_(1.0f),  // Default unmuted.
      playback_rate_(0.0),
      state_(kUninitialized),
      create_audio_decoders_cb_(create_audio_decoders_cb),
      buffering_state_(BUFFERING_HAVE_NOTHING),
      rendering_(false),
      sink_playing_(false),
      pending_read_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      is_suspending_(false),
#if BUILDFLAG(IS_ANDROID)
      is_passthrough_(false) {
#else
      is_passthrough_(false),
      speech_recognition_client_(speech_recognition_client) {
#endif
  DCHECK(create_audio_decoders_cb_);
  // PowerObserver's must be added and removed from the same thread, but we
  // won't remove the observer until we're destructed on |task_runner_| so we
  // must post it here if we're on the wrong thread.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    base::PowerMonitor::GetInstance()->GetInstance()->AddPowerSuspendObserver(
        this);
  } else {
    // Safe to post this without a WeakPtr because this class must be destructed
    // on the same thread and construction has not completed yet.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&base::PowerMonitor::AddPowerSuspendObserver),
            base::Unretained(base::PowerMonitor::GetInstance()), this));
  }

  // Do not add anything below this line since the above actions are only safe
  // as the last lines of the constructor.
}

AudioRendererImpl::~AudioRendererImpl() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::PowerMonitor::GetInstance()->GetInstance()->RemovePowerSuspendObserver(
      this);

  // If Render() is in progress, this call will wait for Render() to finish.
  // After this call, the |sink_| will not call back into |this| anymore.
  sink_->Stop();
  if (null_sink_)
    null_sink_->Stop();

  if (init_cb_)
    FinishInitialization(PIPELINE_ERROR_ABORT);
}

void AudioRendererImpl::StartTicking() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);

  DCHECK(!rendering_);
  rendering_ = true;

  // Wait for an eventual call to SetPlaybackRate() to start rendering.
  if (playback_rate_ == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StartRendering_Locked();
}

void AudioRendererImpl::StartRendering_Locked() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, kPlaying);
  DCHECK(!sink_playing_);
  DCHECK_NE(playback_rate_, 0.0);
  lock_.AssertAcquired();

  sink_playing_ = true;
  was_unmuted_ = was_unmuted_ || volume_ != 0;
  base::AutoUnlock auto_unlock(lock_);
  if (volume_ || !null_sink_)
    sink_->Play();
  else
    null_sink_->Play();
}

void AudioRendererImpl::StopTicking() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);

  DCHECK(rendering_);
  rendering_ = false;

  // Rendering should have already been stopped with a zero playback rate.
  if (playback_rate_ == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StopRendering_Locked();
}

void AudioRendererImpl::StopRendering_Locked() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, kPlaying);
  DCHECK(sink_playing_);
  lock_.AssertAcquired();

  sink_playing_ = false;

  base::AutoUnlock auto_unlock(lock_);
  if (volume_ || !null_sink_)
    sink_->Pause();
  else
    null_sink_->Pause();

  stop_rendering_time_ = last_render_time_;
}

void AudioRendererImpl::SetMediaTime(base::TimeDelta time) {
  DVLOG(1) << __func__ << "(" << time << ")";
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);
  DCHECK(!rendering_);
  DCHECK_EQ(state_, kFlushed);

  start_timestamp_ = time;
  ended_timestamp_ = kInfiniteDuration;
  last_render_time_ = stop_rendering_time_ = base::TimeTicks();
  first_packet_timestamp_ = kNoTimestamp;
  audio_clock_ =
      std::make_unique<AudioClock>(time, audio_parameters_.sample_rate());
}

base::TimeDelta AudioRendererImpl::CurrentMediaTime() {
  base::AutoLock auto_lock(lock_);

  // Return the current time based on the known extents of the rendered audio
  // data plus an estimate based on the last time those values were calculated.
  base::TimeDelta current_media_time = audio_clock_->front_timestamp();
  if (!last_render_time_.is_null()) {
    current_media_time +=
        (tick_clock_->NowTicks() - last_render_time_) * playback_rate_;
    if (current_media_time > audio_clock_->back_timestamp())
      current_media_time = audio_clock_->back_timestamp();
  }

  return current_media_time;
}

bool AudioRendererImpl::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  base::AutoLock auto_lock(lock_);
  DCHECK(wall_clock_times->empty());

  // When playback is paused (rate is zero), assume a rate of 1.0.
  const double playback_rate = playback_rate_ ? playback_rate_ : 1.0;
  const bool is_time_moving = sink_playing_ && playback_rate_ &&
                              !last_render_time_.is_null() &&
                              stop_rendering_time_.is_null() && !is_suspending_;

  // Pre-compute the time until playback of the audio buffer extents, since
  // these values are frequently used below.
  const base::TimeDelta time_until_front =
      audio_clock_->TimeUntilPlayback(audio_clock_->front_timestamp());
  const base::TimeDelta time_until_back =
      audio_clock_->TimeUntilPlayback(audio_clock_->back_timestamp());

  if (media_timestamps.empty()) {
    // Return the current media time as a wall clock time while accounting for
    // frames which may be in the process of play out.
    wall_clock_times->push_back(std::min(
        std::max(tick_clock_->NowTicks(), last_render_time_ + time_until_front),
        last_render_time_ + time_until_back));
    return is_time_moving;
  }

  wall_clock_times->reserve(media_timestamps.size());
  for (const auto& media_timestamp : media_timestamps) {
    // When time was or is moving and the requested media timestamp is within
    // range of played out audio, we can provide an exact conversion.
    if (!last_render_time_.is_null() &&
        media_timestamp >= audio_clock_->front_timestamp() &&
        media_timestamp <= audio_clock_->back_timestamp()) {
      wall_clock_times->push_back(
          last_render_time_ + audio_clock_->TimeUntilPlayback(media_timestamp));
      continue;
    }

    base::TimeDelta base_timestamp, time_until_playback;
    if (media_timestamp < audio_clock_->front_timestamp()) {
      base_timestamp = audio_clock_->front_timestamp();
      time_until_playback = time_until_front;
    } else {
      base_timestamp = audio_clock_->back_timestamp();
      time_until_playback = time_until_back;
    }

    // In practice, most calls will be estimates given the relatively small
    // window in which clients can get the actual time.
    wall_clock_times->push_back(last_render_time_ + time_until_playback +
                                (media_timestamp - base_timestamp) /
                                    playback_rate);
  }

  return is_time_moving;
}

TimeSource* AudioRendererImpl::GetTimeSource() {
  return this;
}

void AudioRendererImpl::Flush(base::OnceClosure callback) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "AudioRendererImpl::Flush",
                                    TRACE_ID_LOCAL(this));

  // Flush |sink_| now.  |sink_| must only be accessed on |task_runner_| and not
  // be called under |lock_|.
  DCHECK(!sink_playing_);
  if (volume_ || !null_sink_)
    sink_->Flush();
  else
    null_sink_->Flush();

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  DCHECK(!flush_cb_);

  flush_cb_ = std::move(callback);
  ChangeState_Locked(kFlushing);

  if (pending_read_)
    return;

  ChangeState_Locked(kFlushed);
  DoFlush_Locked();
}

void AudioRendererImpl::DoFlush_Locked() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  DCHECK(!pending_read_);
  DCHECK_EQ(state_, kFlushed);

  ended_timestamp_ = kInfiniteDuration;
  audio_decoder_stream_->Reset(base::BindOnce(
      &AudioRendererImpl::ResetDecoderDone, weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::ResetDecoderDone() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock auto_lock(lock_);

    DCHECK_EQ(state_, kFlushed);
    DCHECK(flush_cb_);

    received_end_of_stream_ = false;
    rendered_end_of_stream_ = false;

    // Flush() may have been called while underflowed/not fully buffered.
    if (buffering_state_ != BUFFERING_HAVE_NOTHING)
      SetBufferingState_Locked(BUFFERING_HAVE_NOTHING);

    if (buffer_converter_)
      buffer_converter_->Reset();
    algorithm_->FlushBuffers();
  }
  FinishFlush();
}

void AudioRendererImpl::StartPlaying() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);
  DCHECK(!sink_playing_);
  DCHECK_EQ(state_, kFlushed);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);
  DCHECK(!pending_read_) << "Pending read must complete before seeking";

  ChangeState_Locked(kPlaying);
  AttemptRead_Locked();
}

void AudioRendererImpl::Initialize(DemuxerStream* stream,
                                   CdmContext* cdm_context,
                                   RendererClient* client,
                                   PipelineStatusCallback init_cb) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(client);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::AUDIO);
  DCHECK(init_cb);
  DCHECK(state_ == kUninitialized || state_ == kFlushed);
  DCHECK(sink_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "AudioRendererImpl::Initialize",
                                    TRACE_ID_LOCAL(this));

  // If we are re-initializing playback (e.g. switching media tracks), stop the
  // sink first.
  if (state_ == kFlushed) {
    num_absurd_delay_warnings_ = 0;
    sink_->Stop();
    if (null_sink_)
      null_sink_->Stop();
  }

  state_ = kInitializing;
  demuxer_stream_ = stream;
  client_ = client;

  // Always post |init_cb_| because |this| could be destroyed if initialization
  // failed.
  init_cb_ = base::BindPostTaskToCurrentDefault(std::move(init_cb));

  // Retrieve hardware device parameters asynchronously so we don't block the
  // media thread on synchronous IPC.
  sink_->GetOutputDeviceInfoAsync(
      base::BindOnce(&AudioRendererImpl::OnDeviceInfoReceived,
                     weak_factory_.GetWeakPtr(), demuxer_stream_, cdm_context));

#if !BUILDFLAG(IS_ANDROID)
  if (speech_recognition_client_) {
    speech_recognition_client_->SetOnReadyCallback(
        base::BindPostTaskToCurrentDefault(
            base::BindOnce(&AudioRendererImpl::EnableSpeechRecognition,
                           weak_factory_.GetWeakPtr())));
  }
#endif
}

void AudioRendererImpl::OnDeviceInfoReceived(
    DemuxerStream* stream,
    CdmContext* cdm_context,
    OutputDeviceInfo output_device_info) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(client_);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::AUDIO);
  DCHECK(init_cb_);
  DCHECK_EQ(state_, kInitializing);

  // Fall-back to a fake audio sink if the audio device can't be setup; this
  // allows video playback in cases where there is no audio hardware.
  //
  // TODO(dalecurtis): We could disable the audio track here too.
  UMA_HISTOGRAM_ENUMERATION("Media.AudioRendererImpl.SinkStatus",
                            output_device_info.device_status(),
                            OUTPUT_DEVICE_STATUS_MAX + 1);
  if (output_device_info.device_status() != OUTPUT_DEVICE_STATUS_OK) {
    MEDIA_LOG(ERROR, media_log_)
        << "Output device error, falling back to null sink. device_status="
        << output_device_info.device_status();
    sink_ = new NullAudioSink(task_runner_);
    output_device_info = sink_->GetOutputDeviceInfo();
  } else if (base::FeatureList::IsEnabled(kSuspendMutedAudio)) {
    // If playback is muted, we use a fake sink for output until it unmutes.
    null_sink_ = new NullAudioSink(task_runner_);
  }

  current_decoder_config_ = stream->audio_decoder_config();
  DCHECK(current_decoder_config_.IsValidConfig());

  const AudioParameters& hw_params = output_device_info.output_params();
  ChannelLayout hw_channel_layout =
      hw_params.IsValid() ? hw_params.channel_layout() : CHANNEL_LAYOUT_NONE;

  DVLOG(1) << __func__ << ": " << hw_params.AsHumanReadableString();

  AudioCodec codec = stream->audio_decoder_config().codec();
  if (auto* mc = GetMediaClient()) {
    const auto format = ConvertAudioCodecToBitstreamFormat(codec);
    is_passthrough_ = mc->IsSupportedBitstreamAudioCodec(codec) &&
                      hw_params.IsFormatSupportedByHardware(format);
  } else {
    is_passthrough_ = false;
  }
  expecting_config_changes_ = stream->SupportsConfigChanges();
  // AC3/EAC3 windows decoder supports input channel count in the range 1 (mono)
  // to 8 (7.1 channel configuration), but output channel config are stereo, 5.1
  // and 7.1. There will be channel config changes, so here force
  // 'expecting_config_changes_' to true to use 'hw_channel_layout'.
  // Refer to
  // https://learn.microsoft.com/en-us/windows/win32/medfound/dolby-audio-decoder
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO) && BUILDFLAG(IS_WIN)
  if (current_decoder_config_.codec() == AudioCodec::kAC3 ||
      current_decoder_config_.codec() == AudioCodec::kEAC3) {
    expecting_config_changes_ = true;
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO) && BUILDFLAG(IS_WIN)

  bool use_stream_params = !expecting_config_changes_ || !hw_params.IsValid() ||
                           hw_params.format() == AudioParameters::AUDIO_FAKE ||
                           !sink_->IsOptimizedForHardwareParameters();

  if (stream->audio_decoder_config().channel_layout() ==
          CHANNEL_LAYOUT_DISCRETE &&
      sink_->IsOptimizedForHardwareParameters()) {
    use_stream_params = false;
  }

  // Target ~20ms for our buffer size (which is optimal for power efficiency and
  // responsiveness to play/pause events), but if the hardware needs something
  // even larger (say for Bluetooth devices) prefer that.
  //
  // Even if |use_stream_params| is true we should choose a value here based on
  // hardware parameters since it affects the initial buffer size used by
  // AudioRendererAlgorithm. Too small and we will underflow if the hardware
  // asks for a buffer larger than the initial algorithm capacity.
  const int preferred_buffer_size =
      std::max(2 * stream->audio_decoder_config().samples_per_second() / 100,
               hw_params.IsValid() ? hw_params.frames_per_buffer() : 0);

  SampleFormat target_output_sample_format = kUnknownSampleFormat;
  if (is_passthrough_) {
    ChannelLayout channel_layout =
        stream->audio_decoder_config().channel_layout();
    int channels = stream->audio_decoder_config().channels();
    int bytes_per_frame = stream->audio_decoder_config().bytes_per_frame();
    AudioParameters::Format format = AudioParameters::AUDIO_FAKE;
    // For DTS and Dolby formats, set target_output_sample_format to the
    // respective bit-stream format so that passthrough decoder will be selected
    // by MediaCodecAudioRenderer if this is running on Android.
    if (codec == AudioCodec::kAC3) {
      format = AudioParameters::AUDIO_BITSTREAM_AC3;
      target_output_sample_format = kSampleFormatAc3;
    } else if (codec == AudioCodec::kEAC3) {
      format = AudioParameters::AUDIO_BITSTREAM_EAC3;
      target_output_sample_format = kSampleFormatEac3;
    } else if (codec == AudioCodec::kDTS) {
      format = AudioParameters::AUDIO_BITSTREAM_DTS;
      target_output_sample_format = kSampleFormatDts;
      if (hw_params.RequireEncapsulation()) {
        bytes_per_frame = 1;
        channel_layout = CHANNEL_LAYOUT_MONO;
        channels = 1;
      }
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    // If we want the precise PCM frame count here, we have to somehow peek the
    // audio bitstream and parse the header ahead of time. Instead, we ensure
    // audio bus being large enough to accommodate
    // kMaxFramesPerCompressedAudioBuffer frames. The real data size and frame
    // count for bitstream formats will be carried in additional fields of
    // AudioBus.
    const int buffer_size =
        AudioParameters::kMaxFramesPerCompressedAudioBuffer * bytes_per_frame;

    audio_parameters_.Reset(format, {channel_layout, channels},
                            stream->audio_decoder_config().samples_per_second(),
                            buffer_size);
    buffer_converter_.reset();
  } else if (use_stream_params) {
    audio_parameters_.Reset(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                            {stream->audio_decoder_config().channel_layout(),
                             stream->audio_decoder_config().channels()},
                            stream->audio_decoder_config().samples_per_second(),
                            preferred_buffer_size);
    buffer_converter_.reset();
  } else {
    // To allow for seamless sample rate adaptations (i.e. changes from say
    // 16kHz to 48kHz), always resample to the hardware rate.
    int sample_rate = hw_params.sample_rate();

    // If supported by the OS and the initial sample rate is not too low, let
    // the OS level resampler handle resampling for power efficiency.
    if (AudioLatency::IsResamplingPassthroughSupported(
            AudioLatency::Type::kPlayback) &&
        stream->audio_decoder_config().samples_per_second() >= 44100) {
      sample_rate = stream->audio_decoder_config().samples_per_second();
    }

    int stream_channel_count = stream->audio_decoder_config().channels();

    bool try_supported_channel_layouts = false;
#if BUILDFLAG(IS_WIN)
    try_supported_channel_layouts =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTrySupportedChannelLayouts);
#endif

    // We don't know how to up-mix for DISCRETE layouts (fancy multichannel
    // hardware with non-standard speaker arrangement). Instead, pretend the
    // hardware layout is stereo and let the OS take care of further up-mixing
    // to the discrete layout (http://crbug.com/266674). Additionally, pretend
    // hardware is stereo whenever kTrySupportedChannelLayouts is set. This flag
    // is for savvy users who want stereo content to output in all surround
    // speakers. Using the actual layout (likely 5.1 or higher) will mean our
    // mixer will attempt to up-mix stereo source streams to just the left/right
    // speaker of the 5.1 setup, nulling out the other channels
    // (http://crbug.com/177872).
    hw_channel_layout = hw_params.channel_layout() == CHANNEL_LAYOUT_DISCRETE ||
                                try_supported_channel_layouts
                            ? CHANNEL_LAYOUT_STEREO
                            : hw_params.channel_layout();
    int hw_channel_count = ChannelLayoutToChannelCount(hw_channel_layout);

    // The layout we pass to |audio_parameters_| will be used for the lifetime
    // of this audio renderer, regardless of changes to hardware and/or stream
    // properties. Below we choose the max of stream layout vs. hardware layout
    // to leave room for changes to the hardware and/or stream (i.e. avoid
    // premature down-mixing - http://crbug.com/379288).
    // If stream_channels < hw_channels:
    //   Taking max means we up-mix to hardware layout. If stream later changes
    //   to have more channels, we aren't locked into down-mixing to the
    //   initial stream layout.
    // If stream_channels > hw_channels:
    //   We choose to output stream's layout, meaning mixing is a no-op for the
    //   renderer. Browser-side will down-mix to the hardware config. If the
    //   hardware later changes to equal stream channels, browser-side will stop
    //   down-mixing and use the data from all stream channels.

    ChannelLayout stream_channel_layout =
        stream->audio_decoder_config().channel_layout();
    bool use_stream_channel_layout = hw_channel_count <= stream_channel_count;

    ChannelLayoutConfig renderer_channel_layout_config =
        use_stream_channel_layout
            ? ChannelLayoutConfig(stream_channel_layout, stream_channel_count)
            : ChannelLayoutConfig(hw_channel_layout, hw_channel_count);

    audio_parameters_.Reset(hw_params.format(), renderer_channel_layout_config,
                            sample_rate,
                            AudioLatency::GetHighLatencyBufferSize(
                                sample_rate, preferred_buffer_size));
  }

  audio_parameters_.set_effects(audio_parameters_.effects() |
                                AudioParameters::MULTIZONE);

  audio_parameters_.set_latency_tag(AudioLatency::Type::kPlayback);
  if (!audio_parameters_.IsBitstreamFormat()) {
    // Requesting audio offload if it is supported on output.
    media::AudioParameters::HardwareCapabilities hardware_caps(0, 0, 0, true);
    audio_parameters_.set_hardware_capabilities(hardware_caps);
  }

  audio_decoder_stream_ = std::make_unique<AudioDecoderStream>(
      std::make_unique<AudioDecoderStream::StreamTraits>(
          media_log_, hw_channel_layout, target_output_sample_format),
      task_runner_, create_audio_decoders_cb_, media_log_);

  audio_decoder_stream_->set_config_change_observer(base::BindRepeating(
      &AudioRendererImpl::OnConfigChange, weak_factory_.GetWeakPtr()));

  DVLOG(1) << __func__ << ": is_passthrough_=" << is_passthrough_
           << " codec=" << codec
           << " stream->audio_decoder_config().sample_format="
           << stream->audio_decoder_config().sample_format();

  if (!client_->IsVideoStreamAvailable()) {
    // When video is not available, audio prefetch can be enabled.  See
    // crbug/988535.
    audio_parameters_.set_effects(audio_parameters_.effects() |
                                  AudioParameters::AUDIO_PREFETCH);
  }

  last_decoded_channel_layout_ =
      stream->audio_decoder_config().channel_layout();

  is_encrypted_ = stream->audio_decoder_config().is_encrypted();

  last_decoded_channels_ = stream->audio_decoder_config().channels();

  {
    // Set the |audio_clock_| under lock in case this is a reinitialize and some
    // external caller to GetWallClockTimes() exists.
    base::AutoLock lock(lock_);
    audio_clock_ = std::make_unique<AudioClock>(
        base::TimeDelta(), audio_parameters_.sample_rate());
  }

  audio_decoder_stream_->Initialize(
      stream,
      base::BindOnce(&AudioRendererImpl::OnAudioDecoderStreamInitialized,
                     weak_factory_.GetWeakPtr()),
      cdm_context,
      base::BindRepeating(&AudioRendererImpl::OnStatisticsUpdate,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&AudioRendererImpl::OnWaiting,
                          weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::OnAudioDecoderStreamInitialized(bool success) {
  DVLOG(1) << __func__ << ": " << success;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(lock_);

  if (!success) {
    state_ = kUninitialized;
    FinishInitialization(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  if (!audio_parameters_.IsValid()) {
    DVLOG(1) << __func__ << ": Invalid audio parameters: "
             << audio_parameters_.AsHumanReadableString();
    ChangeState_Locked(kUninitialized);

    // TODO(flim): If the channel layout is discrete but channel count is 0, a
    // possible cause is that the input stream has > 8 channels but there is no
    // Web Audio renderer attached and no channel mixing matrices defined for
    // hardware renderers. Adding one for previewing content could be useful.
    FinishInitialization(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  if (expecting_config_changes_ && !audio_parameters_.IsBitstreamFormat()) {
    buffer_converter_ =
        std::make_unique<AudioBufferConverter>(audio_parameters_);
  }

  // We're all good! Continue initializing the rest of the audio renderer
  // based on the decoder format.
  auto* media_client = GetMediaClient();
  auto params =
      (media_client ? media_client->GetAudioRendererAlgorithmParameters(
                          audio_parameters_)
                    : std::nullopt);
  if (params && !client_->IsVideoStreamAvailable()) {
    algorithm_ =
        std::make_unique<AudioRendererAlgorithm>(media_log_, params.value());
  } else {
    algorithm_ = std::make_unique<AudioRendererAlgorithm>(media_log_);
  }
  algorithm_->Initialize(audio_parameters_, is_encrypted_);
  if (latency_hint_)
    algorithm_->SetLatencyHint(latency_hint_);

  algorithm_->SetPreservesPitch(preserves_pitch_);
  ConfigureChannelMask();

  ChangeState_Locked(kFlushed);

  {
    base::AutoUnlock auto_unlock(lock_);
    sink_->Initialize(audio_parameters_, this);
    if (null_sink_) {
      null_sink_->Initialize(audio_parameters_, this);
      null_sink_->Start();  // Does nothing but reduce state bookkeeping.
      real_sink_needs_start_ = true;
    } else {
      // Even when kSuspendMutedAudio is enabled, we can hit this path if we are
      // exclusively using NullAudioSink due to OnDeviceInfoReceived() failure.
      sink_->Start();
      sink_->Pause();  // Sinks play on start.
    }
    SetVolume(volume_);
  }

  DCHECK(!sink_playing_);
  FinishInitialization(PIPELINE_OK);
}

void AudioRendererImpl::FinishInitialization(PipelineStatus status) {
  DCHECK(init_cb_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("media", "AudioRendererImpl::Initialize",
                                  TRACE_ID_LOCAL(this), "status",
                                  PipelineStatusToString(status));
  std::move(init_cb_).Run(status);
}

void AudioRendererImpl::FinishFlush() {
  DCHECK(flush_cb_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "AudioRendererImpl::Flush",
                                  TRACE_ID_LOCAL(this));
  // The |flush_cb_| must always post in order to avoid deadlocking, as some of
  // the functions which may be bound here are re-entrant into lock-acquiring
  // methods of AudioRendererImpl, and FinishFlush may be called while holding
  // the lock. See crbug.com/c/1163459 for a detailed explanation of this.
  task_runner_->PostTask(FROM_HERE, std::move(flush_cb_));
}

void AudioRendererImpl::OnPlaybackError(PipelineStatus error) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnError(error);
}

void AudioRendererImpl::OnPlaybackEnded() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnEnded();
}

void AudioRendererImpl::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnStatisticsUpdate(stats);
}

void AudioRendererImpl::OnBufferingStateChange(BufferingState buffering_state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // "Underflow" is only possible when playing. This avoids noise like blaming
  // the decoder for an "underflow" that is really just a seek.
  BufferingStateChangeReason reason = BUFFERING_CHANGE_REASON_UNKNOWN;
  if (state_ == kPlaying && buffering_state == BUFFERING_HAVE_NOTHING) {
    reason = audio_decoder_stream_->is_demuxer_read_pending()
                 ? DEMUXER_UNDERFLOW
                 : DECODER_UNDERFLOW;
  }

  media_log_->AddEvent<MediaLogEvent::kBufferingStateChanged>(
      SerializableBufferingState<SerializableBufferingStateType::kAudio>{
          buffering_state, reason});

  client_->OnBufferingStateChange(buffering_state, reason);
}

void AudioRendererImpl::OnWaiting(WaitingReason reason) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnWaiting(reason);
}

void AudioRendererImpl::SetVolume(float volume) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Only consider audio as unmuted if the volume is set to a non-zero value
  // when the state is kPlaying.
  if (state_ == kPlaying) {
    was_unmuted_ = was_unmuted_ || volume != 0;
  }

  if (state_ == kUninitialized || state_ == kInitializing) {
    volume_ = volume;
    return;
  }

  sink_->SetVolume(volume);
  if (!null_sink_) {
    // Either null sink suspension is not enabled or we're already on the null
    // sink due to failing to get device parameters.
    return;
  }

  null_sink_->SetVolume(volume);

  // Two cases to handle:
  //   1. Changing from muted to unmuted state.
  //   2. Unmuted startup case.
  if ((!volume_ && volume) || (volume && real_sink_needs_start_)) {
    // Suspend null audio sink (does nothing if unused).
    null_sink_->Pause();

    // Complete startup for the real sink if needed.
    if (real_sink_needs_start_) {
      sink_->Start();
      if (!sink_playing_)
        sink_->Pause();  // Sinks play on start.
      real_sink_needs_start_ = false;
    }

    // Start sink playback if needed.
    if (sink_playing_)
      sink_->Play();
  } else if (volume_ && !volume) {
    // Suspend the real sink (does nothing if unused).
    sink_->Pause();

    // Start fake sink playback if needed.
    if (sink_playing_)
      null_sink_->Play();
  }

  volume_ = volume;
}

void AudioRendererImpl::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  base::AutoLock auto_lock(lock_);

  latency_hint_ = latency_hint;

  if (algorithm_) {
    algorithm_->SetLatencyHint(latency_hint);

    // See if we need further reads to fill up to the new playback threshold.
    // This may be needed if rendering isn't active to schedule regular reads.
    AttemptRead_Locked();
  }
}

void AudioRendererImpl::SetPreservesPitch(bool preserves_pitch) {
  base::AutoLock auto_lock(lock_);

  preserves_pitch_ = preserves_pitch;

  if (algorithm_)
    algorithm_->SetPreservesPitch(preserves_pitch);
}

void AudioRendererImpl::SetWasPlayedWithUserActivationAndHighMediaEngagement(
    bool was_played_with_user_activation_and_high_media_engagement) {
  base::AutoLock auto_lock(lock_);
  was_played_with_user_activation_and_high_media_engagement_ =
      was_played_with_user_activation_and_high_media_engagement;
}

void AudioRendererImpl::OnSuspend() {
  base::AutoLock auto_lock(lock_);
  is_suspending_ = true;
}

void AudioRendererImpl::OnResume() {
  base::AutoLock auto_lock(lock_);
  is_suspending_ = false;
}

void AudioRendererImpl::SetPlayDelayCBForTesting(PlayDelayCBForTesting cb) {
  DCHECK_EQ(state_, kUninitialized);
  play_delay_cb_for_testing_ = std::move(cb);
}

void AudioRendererImpl::DecodedAudioReady(
    AudioDecoderStream::ReadResult result) {
  DVLOG(2) << __func__ << "(" << static_cast<int>(result.code()) << ")";
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized);

  CHECK(pending_read_);
  pending_read_ = false;

  if (!result.has_value()) {
    auto status = PIPELINE_ERROR_DECODE;
    if (result.code() == DecoderStatus::Codes::kAborted)
      status = PIPELINE_OK;
    else if (result.code() == DecoderStatus::Codes::kDisconnected)
      status = PIPELINE_ERROR_DISCONNECTED;

    HandleAbortedReadOrDecodeError(status);
    return;
  }

  scoped_refptr<AudioBuffer> buffer = std::move(result).value();
  DCHECK(buffer);

  if (state_ == kFlushing) {
    ChangeState_Locked(kFlushed);
    DoFlush_Locked();
    return;
  }

  bool need_another_buffer = true;

  // FFmpeg allows "channel pair element" and "single channel element" type
  // AAC streams to masquerade as mono and stereo respectively. Allow these
  // specific exceptions to avoid playback errors.
  bool allow_config_changes = expecting_config_changes_;
  if (!expecting_config_changes_ && !buffer->end_of_stream() &&
      current_decoder_config_.codec() == AudioCodec::kAAC &&
      buffer->sample_rate() == audio_parameters_.sample_rate()) {
    const bool is_mono_to_stereo =
        buffer->channel_layout() == CHANNEL_LAYOUT_MONO &&
        audio_parameters_.channel_layout() == CHANNEL_LAYOUT_STEREO;
    const bool is_stereo_to_mono =
        buffer->channel_layout() == CHANNEL_LAYOUT_STEREO &&
        audio_parameters_.channel_layout() == CHANNEL_LAYOUT_MONO;
    if (is_mono_to_stereo || is_stereo_to_mono) {
      if (!buffer_converter_ && !audio_parameters_.IsBitstreamFormat()) {
        buffer_converter_ =
            std::make_unique<AudioBufferConverter>(audio_parameters_);
      }
      allow_config_changes = true;
    }
  }

  if (allow_config_changes) {
    if (!buffer->end_of_stream()) {
      if (last_decoded_sample_rate_ &&
          buffer->sample_rate() != last_decoded_sample_rate_) {
        DVLOG(1) << __func__ << " Updating audio sample_rate."
                 << " ts:" << buffer->timestamp().InMicroseconds()
                 << " old:" << last_decoded_sample_rate_
                 << " new:" << buffer->sample_rate();
        // Send a bogus config to reset timestamp state.
        OnConfigChange(AudioDecoderConfig());
      }
      last_decoded_sample_rate_ = buffer->sample_rate();

      if (last_decoded_channel_layout_ != buffer->channel_layout()) {
        if (buffer->channel_layout() == CHANNEL_LAYOUT_DISCRETE) {
          MEDIA_LOG(ERROR, media_log_)
              << "Unsupported midstream configuration change! Discrete channel"
              << " layout not allowed by sink.";
          HandleAbortedReadOrDecodeError(PIPELINE_ERROR_DECODE);
          return;
        } else {
          last_decoded_channel_layout_ = buffer->channel_layout();
          last_decoded_channels_ = buffer->channel_count();
          ConfigureChannelMask();
        }
      }
    }

    if (audio_parameters_.IsBitstreamFormat()) {
      // Avoid using `buffer_converter_` for bitstreams, as resampling the
      // bitstream data doesn't make sense.
      CHECK(!buffer_converter_);
      need_another_buffer = HandleDecodedBuffer_Locked(std::move(buffer));
    } else {
      DCHECK(buffer_converter_);
      buffer_converter_->AddInput(std::move(buffer));

      while (buffer_converter_->HasNextBuffer()) {
        need_another_buffer =
            HandleDecodedBuffer_Locked(buffer_converter_->GetNextBuffer());
      }
    }
  } else {
    // TODO(chcunningham, tguilbert): Figure out if we want to support implicit
    // config changes during src=. Doing so requires resampling each individual
    // stream which is inefficient when there are many tags in a page.
    //
    // Check if the buffer we received matches the expected configuration.
    // Note: We explicitly do not check channel layout here to avoid breaking
    // weird behavior with multichannel wav files: http://crbug.com/600538.
    if (!buffer->end_of_stream() &&
        (buffer->sample_rate() != audio_parameters_.sample_rate() ||
         buffer->channel_count() != audio_parameters_.channels())) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unsupported midstream configuration change!"
          << " Sample Rate: " << buffer->sample_rate() << " vs "
          << audio_parameters_.sample_rate()
          << ", Channels: " << buffer->channel_count() << " vs "
          << audio_parameters_.channels();
      HandleAbortedReadOrDecodeError(PIPELINE_ERROR_DECODE);
      return;
    }

    need_another_buffer = HandleDecodedBuffer_Locked(std::move(buffer));
  }

  if (!need_another_buffer && !CanRead_Locked())
    return;

  AttemptRead_Locked();
}

bool AudioRendererImpl::HandleDecodedBuffer_Locked(
    scoped_refptr<AudioBuffer> buffer) {
  lock_.AssertAcquired();
  bool should_render_end_of_stream = false;
  if (buffer->end_of_stream()) {
    received_end_of_stream_ = true;
    algorithm_->MarkEndOfStream();

    // We received no audio to play before EOS, so enter the ended state.
    if (first_packet_timestamp_ == kNoTimestamp)
      should_render_end_of_stream = true;
  } else {
    if (buffer->IsBitstreamFormat() && state_ == kPlaying) {
      if (IsBeforeStartTime(*buffer))
        return true;

      // Adjust the start time since we are unable to trim a compressed audio
      // buffer.
      if (buffer->timestamp() < start_timestamp_ &&
          (buffer->timestamp() + buffer->duration()) > start_timestamp_) {
        start_timestamp_ = buffer->timestamp();
        audio_clock_ = std::make_unique<AudioClock>(
            buffer->timestamp(), audio_parameters_.sample_rate());
      }
    } else if (state_ == kPlaying) {
      if (IsBeforeStartTime(*buffer))
        return true;

      // Trim off any additional time before the start timestamp.
      const base::TimeDelta trim_time = start_timestamp_ - buffer->timestamp();
      if (trim_time.is_positive()) {
        const int frames_to_trim = AudioTimestampHelper::TimeToFrames(
            trim_time, buffer->sample_rate());
        DVLOG(1) << __func__ << ": Trimming first audio buffer by "
                 << frames_to_trim << " frames so it starts at "
                 << start_timestamp_;

        buffer->TrimStart(frames_to_trim);
        buffer->set_timestamp(start_timestamp_);
      }
      // If the entire buffer was trimmed, request a new one.
      if (!buffer->frame_count())
        return true;
    }

    // Store the timestamp of the first packet so we know when to start actual
    // audio playback.
    if (first_packet_timestamp_ == kNoTimestamp)
      first_packet_timestamp_ = buffer->timestamp();

#if !BUILDFLAG(IS_ANDROID)
    // Do not transcribe muted streams initiated by autoplay if the stream was
    // never unmuted.
    if (transcribe_audio_callback_ &&
        (was_played_with_user_activation_and_high_media_engagement_ ||
         was_unmuted_)) {
      transcribe_audio_callback_.Run(buffer);
    }
#endif

    if (state_ != kUninitialized)
      algorithm_->EnqueueBuffer(std::move(buffer));
  }

  const size_t memory_usage = algorithm_->GetMemoryUsage();
  PipelineStatistics stats;
  stats.audio_memory_usage = memory_usage - last_audio_memory_usage_;
  last_audio_memory_usage_ = memory_usage;
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AudioRendererImpl::OnStatisticsUpdate,
                                        weak_factory_.GetWeakPtr(), stats));

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushing:
      NOTREACHED();

    case kFlushed:
      DCHECK(!pending_read_);
      return false;

    case kPlaying:
      if (received_end_of_stream_ || algorithm_->IsQueueAdequateForPlayback()) {
        if (buffering_state_ == BUFFERING_HAVE_NOTHING)
          SetBufferingState_Locked(BUFFERING_HAVE_ENOUGH);
        // This must be done after SetBufferingState_Locked() to ensure the
        // proper state transitions for higher levels.
        if (should_render_end_of_stream) {
          task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AudioRendererImpl::OnPlaybackEnded,
                                        weak_factory_.GetWeakPtr()));
        }
        return false;
      }
      return true;
  }
  return false;
}

void AudioRendererImpl::AttemptRead() {
  base::AutoLock auto_lock(lock_);
  AttemptRead_Locked();
}

void AudioRendererImpl::AttemptRead_Locked() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  if (!CanRead_Locked())
    return;

  pending_read_ = true;

  // Don't hold the lock while calling Read(), if the demuxer is busy this will
  // block audio rendering for an extended period of time.
  // |audio_decoder_stream_| is only accessed on |task_runner_| so this is safe.
  base::AutoUnlock auto_unlock(lock_);
  audio_decoder_stream_->Read(base::BindOnce(
      &AudioRendererImpl::DecodedAudioReady, weak_factory_.GetWeakPtr()));
}

bool AudioRendererImpl::CanRead_Locked() {
  lock_.AssertAcquired();

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushing:
    case kFlushed:
      return false;

    case kPlaying:
      break;
  }

  return !pending_read_ && !received_end_of_stream_ &&
         !algorithm_->IsQueueFull();
}

void AudioRendererImpl::SetPlaybackRate(double playback_rate) {
  DVLOG(1) << __func__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GE(playback_rate, 0);
  DCHECK(sink_);

  base::AutoLock auto_lock(lock_);

  if (is_passthrough_ && playback_rate != 0 && playback_rate != 1) {
    MEDIA_LOG(INFO, media_log_) << "Playback rate changes are not supported "
                                   "when output compressed bitstream."
                                << " Playback Rate: " << playback_rate;
    return;
  }

  // We have two cases here:
  // Play: current_playback_rate == 0 && playback_rate != 0
  // Pause: current_playback_rate != 0 && playback_rate == 0
  double current_playback_rate = playback_rate_;
  playback_rate_ = playback_rate;

  if (!rendering_)
    return;

  if (current_playback_rate == 0 && playback_rate != 0) {
    StartRendering_Locked();
    return;
  }

  if (current_playback_rate != 0 && playback_rate == 0) {
    StopRendering_Locked();
    return;
  }
}

bool AudioRendererImpl::IsBeforeStartTime(const AudioBuffer& buffer) {
  DCHECK_EQ(state_, kPlaying);
  return !buffer.end_of_stream() &&
         (buffer.timestamp() + buffer.duration()) < start_timestamp_;
}

int AudioRendererImpl::Render(base::TimeDelta delay,
                              base::TimeTicks delay_timestamp,
                              const AudioGlitchInfo& glitch_info,
                              AudioBus* audio_bus) {
  TRACE_EVENT("media", "AudioRendererImpl::Render", "id", player_id_,
              "playout_delay (ms)", delay.InMillisecondsF(),
              "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF());

  int frames_requested = audio_bus->frames();
  DVLOG(4) << __func__ << " delay:" << delay << " glitch_info:["
           << glitch_info.ToString() << "]"
           << " frames_requested:" << frames_requested;

  // Since this information is coming from the OS or potentially a fake stream,
  // it may end up with spurious values.
  if (delay.is_negative()) {
    delay = base::TimeDelta();
  }

  if (delay > base::Seconds(1)) {
    LIMITED_MEDIA_LOG(WARNING, media_log_, num_absurd_delay_warnings_, 1)
        << "Large rendering delay (" << delay.InSecondsF()
        << "s) detected; video may stall or be otherwise out of sync with "
           "audio.";
  }

  int frames_written = 0;
  {
    base::AutoLock auto_lock(lock_);
    last_render_time_ = tick_clock_->NowTicks();

    int64_t frames_delayed = AudioTimestampHelper::TimeToFrames(
        delay, audio_parameters_.sample_rate());

    if (!stop_rendering_time_.is_null()) {
      audio_clock_->CompensateForSuspendedWrites(
          last_render_time_ - stop_rendering_time_, frames_delayed);
      stop_rendering_time_ = base::TimeTicks();
    }

    // When WSOLA is used for playback rate changes, its effect is non-linear,
    // so we need to adjust the playback rate given to AudioClock to avoid a/v
    // sync issues over time.
    double effective_playback_rate = playback_rate_;

    // Ensure Stop() hasn't destroyed our |algorithm_| on the pipeline thread.
    if (!algorithm_) {
      audio_clock_->WroteAudio(0, frames_requested, frames_delayed,
                               playback_rate_);
      return 0;
    }

    if (playback_rate_ == 0 || is_suspending_) {
      audio_clock_->WroteAudio(0, frames_requested, frames_delayed,
                               playback_rate_);
      return 0;
    }

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      audio_clock_->WroteAudio(0, frames_requested, frames_delayed,
                               playback_rate_);
      return 0;
    }

    if (is_passthrough_ && algorithm_->BufferedFrames() > 0) {
      DCHECK_EQ(playback_rate_, 1.0);

      // TODO(tsunghung): For compressed bitstream formats, play zeroed buffer
      // won't generate delay. It could be discarded immediately. Need another
      // way to generate audio delay.
      const base::TimeDelta play_delay =
          first_packet_timestamp_ - audio_clock_->back_timestamp();
      if (play_delay.is_positive()) {
        MEDIA_LOG(ERROR, media_log_)
            << "Cannot add delay for compressed audio bitstream format."
            << " Requested delay: " << play_delay;
      }

      frames_written += algorithm_->FillBuffer(audio_bus, 0, frames_requested,
                                               playback_rate_);

      // See Initialize(), the |audio_bus| should be bigger than we need in
      // bitstream cases. Fix |frames_requested| to avoid incorrect time
      // calculation of |audio_clock_| below.
      frames_requested = frames_written;
    } else if (algorithm_->BufferedFrames() > 0) {
      // Delay playback by writing silence if we haven't reached the first
      // timestamp yet; this can occur if the video starts before the audio.
      CHECK_NE(first_packet_timestamp_, kNoTimestamp);
      CHECK_GE(first_packet_timestamp_, base::TimeDelta());
      const base::TimeDelta play_delay =
          first_packet_timestamp_ - audio_clock_->back_timestamp();
      if (play_delay.is_positive()) {
        DCHECK_EQ(frames_written, 0);

        if (!play_delay_cb_for_testing_.is_null())
          play_delay_cb_for_testing_.Run(play_delay);

        // Don't multiply |play_delay| out since it can be a huge value on
        // poorly encoded media and multiplying by the sample rate could cause
        // the value to overflow.
        if (play_delay.InSecondsF() > static_cast<double>(frames_requested) /
                                          audio_parameters_.sample_rate()) {
          frames_written = frames_requested;
        } else {
          frames_written =
              play_delay.InSecondsF() * audio_parameters_.sample_rate();
        }

        audio_bus->ZeroFramesPartial(0, frames_written);
      }

      // If there's any space left, actually render the audio; this is where the
      // aural magic happens.
      if (frames_written < frames_requested) {
        DVLOG(4) << __func__ << ": drift="
                 << CalculateClockAndAlgorithmDrift().InMicroseconds() << "us";

        const auto frames_filled = algorithm_->FillBuffer(
            audio_bus, frames_written, frames_requested - frames_written,
            playback_rate_);
        frames_written += frames_filled;
        effective_playback_rate = algorithm_->effective_playback_rate();

        DVLOG(4) << __func__ << ": frames_filled=" << frames_filled
                 << ", playback_rate_=" << playback_rate_
                 << ", effective_playback_rate=" << effective_playback_rate;
      }
    }

    // We use the following conditions to determine end of playback:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We received an end of stream buffer
    //   3) We haven't already signalled that we've ended
    //   4) We've played all known audio data sent to hardware
    //
    // We use the following conditions to determine underflow:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We have NOT received an end of stream buffer
    //   3) We are in the kPlaying state
    //
    // Otherwise the buffer has data we can send to the device.
    //
    // Per the TimeSource API the media time should always increase even after
    // we've rendered all known audio data. Doing so simplifies scenarios where
    // we have other sources of media data that need to be scheduled after audio
    // data has ended.
    //
    // That being said, we don't want to advance time when underflowed as we
    // know more decoded frames will eventually arrive. If we did, we would
    // throw things out of sync when said decoded frames arrive.
    int frames_after_end_of_stream = 0;
    if (frames_written == 0) {
      if (received_end_of_stream_) {
        if (ended_timestamp_ == kInfiniteDuration)
          ended_timestamp_ = audio_clock_->back_timestamp();
        frames_after_end_of_stream = frames_requested;
      } else if (state_ == kPlaying &&
                 buffering_state_ != BUFFERING_HAVE_NOTHING) {
        // Don't increase queue capacity if the queue latency is explicitly
        // specified.
        if (!latency_hint_)
          algorithm_->IncreasePlaybackThreshold();

        SetBufferingState_Locked(BUFFERING_HAVE_NOTHING);
      }
    } else if (frames_written < frames_requested && !received_end_of_stream_ &&
               state_ == kPlaying &&
               buffering_state_ != BUFFERING_HAVE_NOTHING) {
      // If we only partially filled the request and should have more data, go
      // ahead and increase queue capacity to try and meet the next request.
      // Trigger underflow to give us a chance to refill up to the new cap.
      // When a latency hint is present, don't override the user's preference
      // with a queue increase, but still signal HAVE_NOTHING for them to take
      // action if they choose.

      if (!latency_hint_)
        algorithm_->IncreasePlaybackThreshold();

      SetBufferingState_Locked(BUFFERING_HAVE_NOTHING);
    }

    // Note: effective_playback_rate() is used here because WSOLA is a
    // non-linear operation. E.g., for a `playback_rate_` of 2.0 WSOLA may end
    // up with effective rates between 1 and 3 and a/v sync drift of +/- 20ms.
    // This effect is normally cyclical, so it doesn't build over time... except
    // during repeated playback changes. https://crbug.com/40190553
    //
    // Teaching AudioClock about non-linear time would be difficult, but luckily
    // we can approximate it well enough by just calculating an effective rate
    // as frames consumed / frames produced for each FillBuffer() call.
    audio_clock_->WroteAudio(frames_written + frames_after_end_of_stream,
                             frames_requested, frames_delayed,
                             effective_playback_rate);

    if (CanRead_Locked()) {
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&AudioRendererImpl::AttemptRead,
                                            weak_factory_.GetWeakPtr()));
    }

    if (audio_clock_->front_timestamp() >= ended_timestamp_ &&
        !rendered_end_of_stream_) {
      rendered_end_of_stream_ = true;
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&AudioRendererImpl::OnPlaybackEnded,
                                            weak_factory_.GetWeakPtr()));
    }
  }

  DCHECK_LE(frames_written, frames_requested);
  return frames_written;
}

void AudioRendererImpl::OnRenderError() {
  MEDIA_LOG(ERROR, media_log_) << "audio render error";

  // Post to |task_runner_| as this is called on the audio callback thread.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioRendererImpl::OnPlaybackError,
                     weak_factory_.GetWeakPtr(), AUDIO_RENDERER_ERROR));
}

void AudioRendererImpl::HandleAbortedReadOrDecodeError(PipelineStatus status) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  switch (state_) {
    case kUninitialized:
    case kInitializing:
      NOTREACHED();
    case kFlushing:
      ChangeState_Locked(kFlushed);
      if (status == PIPELINE_OK) {
        DoFlush_Locked();
        return;
      }

      MEDIA_LOG(ERROR, media_log_)
          << "audio error during flushing, status: " << status;
      client_->OnError(status);
      FinishFlush();
      return;

    case kFlushed:
    case kPlaying:
      if (status != PIPELINE_OK) {
        MEDIA_LOG(ERROR, media_log_)
            << "audio error during playing, status: " << status;
        client_->OnError(status);
      }
      return;
  }
}

void AudioRendererImpl::ChangeState_Locked(State new_state) {
  DVLOG(1) << __func__ << " : " << state_ << " -> " << new_state;
  lock_.AssertAcquired();
  state_ = new_state;
}

void AudioRendererImpl::OnConfigChange(const AudioDecoderConfig& config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(expecting_config_changes_);

  // We don't use `buffer_converter_` for bitstream formats.
  CHECK(buffer_converter_ || audio_parameters_.IsBitstreamFormat());
  if (buffer_converter_) {
    buffer_converter_->ResetTimestampState();
  }

  // An invalid config may be supplied by callers who simply want to reset
  // internal state outside of detecting a new config from the demuxer stream.
  // RendererClient only cares to know about config changes that differ from
  // previous configs.
  if (config.IsValidConfig() && !current_decoder_config_.Matches(config)) {
    current_decoder_config_ = config;
    client_->OnAudioConfigChange(config);
  }
}

void AudioRendererImpl::SetBufferingState_Locked(
    BufferingState buffering_state) {
  DVLOG(1) << __func__ << " : " << buffering_state_ << " -> "
           << buffering_state;
  DCHECK_NE(buffering_state_, buffering_state);
  lock_.AssertAcquired();
  buffering_state_ = buffering_state;

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioRendererImpl::OnBufferingStateChange,
                                weak_factory_.GetWeakPtr(), buffering_state_));
}

void AudioRendererImpl::ConfigureChannelMask() {
  DCHECK(algorithm_);
  DCHECK(audio_parameters_.IsValid());
  DCHECK_NE(last_decoded_channel_layout_, CHANNEL_LAYOUT_NONE);
  DCHECK_NE(last_decoded_channel_layout_, CHANNEL_LAYOUT_UNSUPPORTED);

  // If we're actually downmixing the signal, no mask is necessary, but ensure
  // we clear any existing mask if present.
  if (last_decoded_channels_ >= audio_parameters_.channels()) {
    algorithm_->SetChannelMask(
        std::vector<bool>(audio_parameters_.channels(), true));
    return;
  }

  // Determine the matrix used to upmix the channels.
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix(last_decoded_channel_layout_, last_decoded_channels_,
                      audio_parameters_.channel_layout(),
                      audio_parameters_.channels())
      .CreateTransformationMatrix(&matrix);

  // All channels with a zero mix are muted and can be ignored.
  std::vector<bool> channel_mask(audio_parameters_.channels(), false);
  for (size_t ch = 0; ch < matrix.size(); ++ch) {
    channel_mask[ch] =
        base::ranges::any_of(matrix[ch], [](float mix) { return !!mix; });
  }
  algorithm_->SetChannelMask(std::move(channel_mask));
}

void AudioRendererImpl::EnableSpeechRecognition() {
#if !BUILDFLAG(IS_ANDROID)
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  transcribe_audio_callback_ = base::BindRepeating(
      &AudioRendererImpl::TranscribeAudio, weak_factory_.GetWeakPtr());
#endif
}

void AudioRendererImpl::TranscribeAudio(
    scoped_refptr<media::AudioBuffer> buffer) {
#if !BUILDFLAG(IS_ANDROID)
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (speech_recognition_client_)
    speech_recognition_client_->AddAudio(std::move(buffer));
#endif
}

base::TimeDelta AudioRendererImpl::CalculateClockAndAlgorithmDrift() const {
  return algorithm_->FrontTimestamp().value_or(audio_clock_->back_timestamp()) -
         audio_clock_->back_timestamp();
}

}  // namespace media
