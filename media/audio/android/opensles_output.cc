// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/android/opensles_output.h"

#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/media_switches.h"

#define LOG_ON_FAILURE_AND_RETURN(op, ...)      \
  do {                                          \
    SLresult err = (op);                        \
    if (err != SL_RESULT_SUCCESS) {             \
      DLOG(ERROR) << #op << " failed: " << err; \
      return __VA_ARGS__;                       \
    }                                           \
  } while (0)

namespace media {

OpenSLESOutputStream::OpenSLESOutputStream(AudioManagerAndroid* manager,
                                           const AudioParameters& params,
                                           SLint32 stream_type)
    : audio_manager_(manager),
      stream_type_(stream_type),
      callback_(nullptr),
      player_(nullptr),
      simple_buffer_queue_(nullptr),
      audio_data_(),
      active_buffer_index_(0),
      started_(false),
      muted_(false),
      volume_(1.0),
      samples_per_second_(params.sample_rate()),
      sample_format_(kSampleFormatF32),
      bytes_per_frame_(params.GetBytesPerFrame(sample_format_)),
      buffer_size_bytes_(params.GetBytesPerBuffer(sample_format_)),
      performance_mode_(SL_ANDROID_PERFORMANCE_NONE),
      delay_calculator_(samples_per_second_) {
  DVLOG(2) << "OpenSLESOutputStream::OpenSLESOutputStream("
           << "stream_type=" << stream_type << ")";

  if (params.latency_tag() == AudioLatency::Type::kPlayback) {
    performance_mode_ = SL_ANDROID_PERFORMANCE_POWER_SAVING;
  } else if (params.latency_tag() == AudioLatency::Type::kRtc) {
    performance_mode_ = SL_ANDROID_PERFORMANCE_LATENCY_EFFECTS;
  }

  audio_bus_ = AudioBus::Create(params);

  float_format_.formatType = SL_ANDROID_DATAFORMAT_PCM_EX;
  float_format_.numChannels = static_cast<SLuint32>(params.channels());
  // Despite the name, this field is actually the sampling rate in millihertz.
  float_format_.sampleRate = static_cast<SLuint32>(samples_per_second_ * 1000);
  float_format_.bitsPerSample = float_format_.containerSize =
      SampleFormatToBitsPerChannel(sample_format_);
  float_format_.endianness = SL_BYTEORDER_LITTLEENDIAN;
  float_format_.channelMask = ChannelCountToSLESChannelMask(params.channels());
  float_format_.representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
}

OpenSLESOutputStream::~OpenSLESOutputStream() {
  DVLOG(2) << "OpenSLESOutputStream::~OpenSLESOutputStream()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!engine_object_.Get());
  DCHECK(!player_object_.Get());
  DCHECK(!output_mixer_.Get());
  DCHECK(!player_);
  DCHECK(!simple_buffer_queue_);
  DCHECK(!audio_data_[0]);
}

bool OpenSLESOutputStream::Open() {
  DVLOG(2) << "OpenSLESOutputStream::Open()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (engine_object_.Get())
    return false;

  if (!CreatePlayer())
    return false;

  SetupAudioBuffer();
  active_buffer_index_ = 0;

  return true;
}

void OpenSLESOutputStream::Start(AudioSourceCallback* callback) {
  DVLOG(2) << "OpenSLESOutputStream::Start()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  DCHECK(player_);
  DCHECK(simple_buffer_queue_);
  if (started_)
    return;

  base::AutoLock lock(lock_);
  DCHECK(!callback_);
  callback_ = callback;

  CacheHardwareLatencyIfNeeded();

  // Fill audio data with silence to avoid start-up glitches. Don't use
  // FillBufferQueueNoLock() since it can trigger recursive entry if an error
  // occurs while writing into the stream. See http://crbug.com/624877.
  memset(audio_data_[active_buffer_index_], 0, buffer_size_bytes_);
  LOG_ON_FAILURE_AND_RETURN((*simple_buffer_queue_)
                                ->Enqueue(simple_buffer_queue_,
                                          audio_data_[active_buffer_index_],
                                          buffer_size_bytes_));
  active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;

  // Start streaming data by setting the play state to SL_PLAYSTATE_PLAYING.
  // For a player object, when the object is in the SL_PLAYSTATE_PLAYING
  // state, adding buffers will implicitly start playback.
  LOG_ON_FAILURE_AND_RETURN(
      (*player_)->SetPlayState(player_, SL_PLAYSTATE_PLAYING));

  // On older version of Android, the position may not be reset even though we
  // call Clear() during Stop(), in this case the best we can do is assume that
  // we're continuing on from this previous position.
  uint32_t position_in_ms = 0;
  LOG_ON_FAILURE_AND_RETURN((*player_)->GetPosition(player_, &position_in_ms));
  delay_calculator_.SetBaseTimestamp(base::Milliseconds(position_in_ms));
  delay_calculator_.AddFrames(audio_bus_->frames());

  started_ = true;
}

void OpenSLESOutputStream::Stop() {
  DVLOG(2) << "OpenSLESOutputStream::Stop()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!started_)
    return;

  base::AutoLock lock(lock_);

  // Stop playing by setting the play state to SL_PLAYSTATE_STOPPED.
  LOG_ON_FAILURE_AND_RETURN(
      (*player_)->SetPlayState(player_, SL_PLAYSTATE_STOPPED));

  // Clear the buffer queue so that the old data won't be played when
  // resuming playing.
  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->Clear(simple_buffer_queue_));

#ifndef NDEBUG
  // Verify that the buffer queue is in fact cleared as it should.
  SLAndroidSimpleBufferQueueState buffer_queue_state;
  LOG_ON_FAILURE_AND_RETURN((*simple_buffer_queue_)->GetState(
      simple_buffer_queue_, &buffer_queue_state));
  DCHECK_EQ(0u, buffer_queue_state.count);
  DCHECK_EQ(0u, buffer_queue_state.index);
#endif

  callback_ = nullptr;
  started_ = false;
}

void OpenSLESOutputStream::Close() {
  DVLOG(2) << "OpenSLESOutputStream::Close()";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Stop the stream if it is still playing.
  Stop();
  {
    // Destroy the buffer queue player object and invalidate all associated
    // interfaces.
    player_object_.Reset();
    simple_buffer_queue_ = nullptr;
    player_ = nullptr;

    // Destroy the mixer object. We don't store any associated interface for
    // this object.
    output_mixer_.Reset();

    // Destroy the engine object. We don't store any associated interface for
    // this object.
    engine_object_.Reset();
    ReleaseAudioBuffer();
  }

  audio_manager_->ReleaseOutputStream(this);
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void OpenSLESOutputStream::Flush() {}

void OpenSLESOutputStream::SetVolume(double volume) {
  DVLOG(2) << "OpenSLESOutputStream::SetVolume(" << volume << ")";
  DCHECK(thread_checker_.CalledOnValidThread());

  double volume_override = 0;
  if (audio_manager_->HasOutputVolumeOverride(&volume_override)) {
    volume = volume_override;
  }

  float volume_float = static_cast<float>(volume);
  if (volume_float < 0.0f || volume_float > 1.0f) {
    return;
  }
  volume_ = volume_float;
}

void OpenSLESOutputStream::GetVolume(double* volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *volume = static_cast<double>(volume_);
}

void OpenSLESOutputStream::SetMute(bool muted) {
  DVLOG(2) << "OpenSLESOutputStream::SetMute(" << muted << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  muted_ = muted;
}

bool OpenSLESOutputStream::CreatePlayer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!engine_object_.Get());
  DCHECK(!player_object_.Get());
  DCHECK(!output_mixer_.Get());
  DCHECK(!player_);
  DCHECK(!simple_buffer_queue_);

  // Initializes the engine object with specific option. After working with the
  // object, we need to free the object and its resources.
  SLEngineOption option[] = {
      {SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE)}};
  LOG_ON_FAILURE_AND_RETURN(
      slCreateEngine(engine_object_.Receive(), 1, option, 0, nullptr, nullptr),
      false);

  // Realize the SL engine object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE), false);

  // Get the SL engine interface which is implicit.
  SLEngineItf engine;
  LOG_ON_FAILURE_AND_RETURN(engine_object_->GetInterface(
                                engine_object_.Get(), SL_IID_ENGINE, &engine),
                            false);

  // Create output mixer object to be used by the player.
  LOG_ON_FAILURE_AND_RETURN(
      (*engine)->CreateOutputMix(engine, output_mixer_.Receive(), 0, nullptr,
                                 nullptr),
      false);

  // Realizing the output mix object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      output_mixer_->Realize(output_mixer_.Get(), SL_BOOLEAN_FALSE), false);

  // Audio source configuration.
  SLDataLocator_AndroidSimpleBufferQueue simple_buffer_queue = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
      static_cast<SLuint32>(kMaxNumOfBuffersInQueue)};
  SLDataSource audio_source;
  audio_source = {&simple_buffer_queue, &float_format_};

  // Audio sink configuration.
  SLDataLocator_OutputMix locator_output_mix = {SL_DATALOCATOR_OUTPUTMIX,
                                                output_mixer_.Get()};
  SLDataSink audio_sink = {&locator_output_mix, nullptr};

  // Create an audio player.
  const SLInterfaceID interface_id[] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME,
                                        SL_IID_ANDROIDCONFIGURATION};
  const SLboolean interface_required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
                                          SL_BOOLEAN_TRUE};
  LOG_ON_FAILURE_AND_RETURN(
      (*engine)->CreateAudioPlayer(
          engine, player_object_.Receive(), &audio_source, &audio_sink,
          std::size(interface_id), interface_id, interface_required),
      false);

  // Create AudioPlayer and specify SL_IID_ANDROIDCONFIGURATION.
  SLAndroidConfigurationItf player_config;
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->GetInterface(
          player_object_.Get(), SL_IID_ANDROIDCONFIGURATION, &player_config),
      false);

  // Set configuration using the stream type provided at construction.
  LOG_ON_FAILURE_AND_RETURN(
      (*player_config)
          ->SetConfiguration(player_config, SL_ANDROID_KEY_STREAM_TYPE,
                             &stream_type_, sizeof(SLint32)),
      false);

  // Set configuration using the stream type provided at construction.
  if (performance_mode_ > SL_ANDROID_PERFORMANCE_NONE) {
    LOG_ON_FAILURE_AND_RETURN(
        (*player_config)
            ->SetConfiguration(player_config, SL_ANDROID_KEY_PERFORMANCE_MODE,
                               &performance_mode_, sizeof(SLuint32)),
        false);
  }

  // Realize the player object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->Realize(player_object_.Get(), SL_BOOLEAN_FALSE), false);

  // Get an implicit player interface.
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->GetInterface(player_object_.Get(), SL_IID_PLAY, &player_),
      false);

  // Get the simple buffer queue interface.
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->GetInterface(
          player_object_.Get(), SL_IID_BUFFERQUEUE, &simple_buffer_queue_),
      false);

  // Register the input callback for the simple buffer queue.
  // This callback will be called when the soundcard needs data.
  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->RegisterCallback(
          simple_buffer_queue_, SimpleBufferQueueCallback, this),
      false);

  return true;
}

void OpenSLESOutputStream::SimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf buffer_queue,
    void* instance) {
  OpenSLESOutputStream* stream =
      reinterpret_cast<OpenSLESOutputStream*>(instance);
  stream->FillBufferQueue();
}

void OpenSLESOutputStream::FillBufferQueue() {
  base::AutoLock lock(lock_);
  if (!started_)
    return;

  TRACE_EVENT0("audio", "OpenSLESOutputStream::FillBufferQueue");

  // Verify that we are in a playing state.
  SLuint32 state;
  SLresult err = (*player_)->GetPlayState(player_, &state);
  if (SL_RESULT_SUCCESS != err) {
    HandleError(err);
    return;
  }
  if (state != SL_PLAYSTATE_PLAYING) {
    DLOG(WARNING) << "Received callback in non-playing state";
    return;
  }

  // Fill up one buffer in the queue by asking the registered source for
  // data using the OnMoreData() callback.
  FillBufferQueueNoLock();
}

void OpenSLESOutputStream::FillBufferQueueNoLock() {
  // Ensure that the calling thread has acquired the lock since it is not
  // done in this method.
  lock_.AssertAcquired();

  // Calculate the position relative to the number of frames written.
  uint32_t position_in_ms = 0;
  SLresult err = (*player_)->GetPosition(player_, &position_in_ms);

  // Given the position of the playback head, compute the approximate number of
  // frames that have been queued to the buffer but not yet played out.
  // Note that the value returned by GetFramesToTarget() is negative because
  // more frames have been added to |delay_calculator_| than have been played
  // out and thus the target timestamp is earlier than the current timestamp of
  // |delay_calculator_|.
  const int delay_frames =
      err == SL_RESULT_SUCCESS
          ? -delay_calculator_.GetFramesToTarget(
                AdjustPositionForHardwareLatency(position_in_ms))
          : 0;
  DCHECK_GE(delay_frames, 0);

  // Note: *DO NOT* use format_.samplesPerSecond in any calculations, it is not
  // actually the sample rate! See constructor comments. :|
  const base::TimeDelta delay =
      AudioTimestampHelper::FramesToTime(delay_frames, samples_per_second_);

  // Read data from the registered client source.
  const int frames_filled = callback_->OnMoreData(delay, base::TimeTicks::Now(),
                                                  {}, audio_bus_.get());
  if (frames_filled <= 0) {
    // Audio source is shutting down, or halted on error.
    return;
  }

  // Note: If the internal representation ever changes from 16-bit PCM to
  // raw float, the data must be clipped and sanitized since it may come
  // from an untrusted source such as NaCl.
  audio_bus_->Scale(muted_ ? 0.0f : volume_);
  // We skip clipping since that occurs at the shared memory boundary.
  audio_bus_->ToInterleaved<Float32SampleTypeTraitsNoClip>(
      frames_filled,
      reinterpret_cast<float*>(audio_data_[active_buffer_index_]));

  delay_calculator_.AddFrames(frames_filled);
  const int num_filled_bytes = frames_filled * bytes_per_frame_;
  DCHECK_LE(static_cast<size_t>(num_filled_bytes), buffer_size_bytes_);

  // Enqueue the buffer for playback.
  err = (*simple_buffer_queue_)
            ->Enqueue(simple_buffer_queue_, audio_data_[active_buffer_index_],
                      num_filled_bytes);
  if (SL_RESULT_SUCCESS != err)
    HandleError(err);

  active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;
}

void OpenSLESOutputStream::SetupAudioBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!audio_data_[0]);
  for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i)
    audio_data_[i] = new uint8_t[buffer_size_bytes_];
}

void OpenSLESOutputStream::ReleaseAudioBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_data_[0]) {
    for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
      delete[] audio_data_[i];
      audio_data_[i] = nullptr;
    }
  }
}

void OpenSLESOutputStream::HandleError(SLresult error) {
  DLOG(ERROR) << "OpenSLES Output error " << error;
  // TODO(dalecurtis): Consider sending a translated |error|.
  if (callback_)
    callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
}

void OpenSLESOutputStream::CacheHardwareLatencyIfNeeded() {
  // If the feature is turned off, then leave it at its default (zero) value.
  // In general, GetOutputLatency is not reliable.
  if (!base::FeatureList::IsEnabled(kUseAudioLatencyFromHAL))
    return;

  hardware_latency_ = audio_manager_->GetOutputLatency();
}

base::TimeDelta OpenSLESOutputStream::AdjustPositionForHardwareLatency(
    uint32_t position_in_ms) {
  base::TimeDelta position = base::Milliseconds(position_in_ms);

  if (position <= hardware_latency_)
    return base::Milliseconds(0);

  return position - hardware_latency_;
}

}  // namespace media
