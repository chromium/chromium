// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/android/opensles_input.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/audio_bus.h"

#define LOG_ON_FAILURE_AND_RETURN(op, ...)      \
  do {                                          \
    SLresult err = (op);                        \
    if (err != SL_RESULT_SUCCESS) {             \
      DLOG(ERROR) << #op << " failed: " << err; \
      return __VA_ARGS__;                       \
    }                                           \
  } while (0)

namespace media {

OpenSLESInputStream::OpenSLESInputStream(AudioManagerAndroid* audio_manager,
                                         const AudioParameters& params)
    : peak_detector_(base::BindRepeating(&AudioManager::TraceAmplitudePeak,
                                         base::Unretained(audio_manager),
                                         /*trace_start=*/true)),
      audio_manager_(audio_manager),
      callback_(nullptr),
      recorder_(nullptr),
      simple_buffer_queue_(nullptr),
      active_buffer_index_(0),
      buffer_size_bytes_(0),
      started_(false),
      audio_bus_(media::AudioBus::Create(params)),
      no_effects_(params.effects() == AudioParameters::NO_EFFECTS) {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DVLOG(1) << "Audio effects enabled: " << !no_effects_;

  const SampleFormat kSampleFormat = kSampleFormatS16;

  format_.formatType = SL_DATAFORMAT_PCM;
  format_.numChannels = static_cast<SLuint32>(params.channels());
  // Provides sampling rate in milliHertz to OpenSLES.
  format_.samplesPerSec = static_cast<SLuint32>(params.sample_rate() * 1000);
  format_.bitsPerSample = format_.containerSize =
      SampleFormatToBitsPerChannel(kSampleFormat);
  format_.endianness = SL_BYTEORDER_LITTLEENDIAN;
  format_.channelMask = ChannelCountToSLESChannelMask(params.channels());

  buffer_size_bytes_ = params.GetBytesPerBuffer(kSampleFormat);
  hardware_delay_ = base::Seconds(params.frames_per_buffer() /
                                  static_cast<double>(params.sample_rate()));

  memset(&audio_data_, 0, sizeof(audio_data_));
}

OpenSLESInputStream::~OpenSLESInputStream() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!recorder_object_.Get());
  DCHECK(!engine_object_.Get());
  DCHECK(!recorder_);
  DCHECK(!simple_buffer_queue_);
  DCHECK(!audio_data_[0]);
}

AudioInputStream::OpenOutcome OpenSLESInputStream::Open() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (engine_object_.Get())
    return AudioInputStream::OpenOutcome::kFailed;

  if (!CreateRecorder())
    return AudioInputStream::OpenOutcome::kFailed;

  SetupAudioBuffer();
  return AudioInputStream::OpenOutcome::kSuccess;
}

void OpenSLESInputStream::Start(AudioInputCallback* callback) {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  DCHECK(recorder_);
  DCHECK(simple_buffer_queue_);
  if (started_)
    return;

  base::AutoLock lock(lock_);
  DCHECK(!callback_ || callback_ == callback);
  callback_ = callback;
  active_buffer_index_ = 0;

  // Enqueues kMaxNumOfBuffersInQueue zero buffers to get the ball rolling.
  // TODO(henrika): add support for Start/Stop/Start sequences when we are
  // able to clear the buffer queue. There is currently a bug in the OpenSLES
  // implementation which forces us to always call Stop() and Close() before
  // calling Start() again.
  SLresult err = SL_RESULT_UNKNOWN_ERROR;
  for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
    err = (*simple_buffer_queue_)->Enqueue(
        simple_buffer_queue_, audio_data_[i], buffer_size_bytes_);
    if (SL_RESULT_SUCCESS != err) {
      HandleError(err);
      started_ = false;
      return;
    }
  }

  // Start the recording by setting the state to SL_RECORDSTATE_RECORDING.
  // When the object is in the SL_RECORDSTATE_RECORDING state, adding buffers
  // will implicitly start the filling process.
  err = (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_RECORDING);
  if (SL_RESULT_SUCCESS != err) {
    HandleError(err);
    started_ = false;
    return;
  }

  started_ = true;
}

void OpenSLESInputStream::Stop() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!started_)
    return;

  base::AutoLock lock(lock_);

  // Stop recording by setting the record state to SL_RECORDSTATE_STOPPED.
  LOG_ON_FAILURE_AND_RETURN(
      (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_STOPPED));

  // Clear the buffer queue to get rid of old data when resuming recording.
  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->Clear(simple_buffer_queue_));

  started_ = false;
  callback_ = nullptr;
}

void OpenSLESInputStream::Close() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Stop the stream if it is still recording.
  Stop();
  {
    // TODO(henrika): Do we need to hold the lock here?
    base::AutoLock lock(lock_);

    // Destroy the buffer queue recorder object and invalidate all associated
    // interfaces.
    recorder_object_.Reset();
    simple_buffer_queue_ = nullptr;
    recorder_ = nullptr;

    // Destroy the engine object. We don't store any associated interface for
    // this object.
    engine_object_.Reset();
    ReleaseAudioBuffer();
  }

  audio_manager_->ReleaseInputStream(this);
}

double OpenSLESInputStream::GetMaxVolume() {
  return 0.0;
}

void OpenSLESInputStream::SetVolume(double volume) {
}

double OpenSLESInputStream::GetVolume() {
  return 0.0;
}

bool OpenSLESInputStream::SetAutomaticGainControl(bool enabled) {
  return false;
}

bool OpenSLESInputStream::GetAutomaticGainControl() {
  return false;
}

bool OpenSLESInputStream::IsMuted() {
  return false;
}

void OpenSLESInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

bool OpenSLESInputStream::CreateRecorder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!engine_object_.Get());
  DCHECK(!recorder_object_.Get());
  DCHECK(!recorder_);
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

  // Audio source configuration.
  SLDataLocator_IODevice mic_locator = {SL_DATALOCATOR_IODEVICE,
                                        SL_IODEVICE_AUDIOINPUT,
                                        SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};
  SLDataSource audio_source = {&mic_locator, nullptr};

  // Audio sink configuration.
  SLDataLocator_AndroidSimpleBufferQueue buffer_queue = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
      static_cast<SLuint32>(kMaxNumOfBuffersInQueue)};
  SLDataSink audio_sink = {&buffer_queue, &format_};

  // Create an audio recorder.
  const SLInterfaceID interface_id[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                        SL_IID_ANDROIDCONFIGURATION};
  const SLboolean interface_required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

  // Create AudioRecorder and specify SL_IID_ANDROIDCONFIGURATION.
  LOG_ON_FAILURE_AND_RETURN(
      (*engine)->CreateAudioRecorder(
          engine, recorder_object_.Receive(), &audio_source, &audio_sink,
          std::size(interface_id), interface_id, interface_required),
      false);

  SLAndroidConfigurationItf recorder_config;
  LOG_ON_FAILURE_AND_RETURN(
      recorder_object_->GetInterface(recorder_object_.Get(),
                                     SL_IID_ANDROIDCONFIGURATION,
                                     &recorder_config),
      false);

  // Uses the main microphone tuned for audio communications if effects are
  // enabled and disables all audio processing if effects are disabled.
  SLint32 stream_type = no_effects_
                            ? SL_ANDROID_RECORDING_PRESET_CAMCORDER
                            : SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION;
  LOG_ON_FAILURE_AND_RETURN(
      (*recorder_config)->SetConfiguration(recorder_config,
                                           SL_ANDROID_KEY_RECORDING_PRESET,
                                           &stream_type,
                                           sizeof(SLint32)),
      false);

  // Realize the recorder object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      recorder_object_->Realize(recorder_object_.Get(), SL_BOOLEAN_FALSE),
      false);

  // Get an implicit recorder interface.
  LOG_ON_FAILURE_AND_RETURN(
      recorder_object_->GetInterface(
          recorder_object_.Get(), SL_IID_RECORD, &recorder_),
      false);

  // Get the simple buffer queue interface.
  LOG_ON_FAILURE_AND_RETURN(
      recorder_object_->GetInterface(recorder_object_.Get(),
                                     SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                     &simple_buffer_queue_),
      false);

  // Register the input callback for the simple buffer queue.
  // This callback will be called when receiving new data from the device.
  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->RegisterCallback(
          simple_buffer_queue_, SimpleBufferQueueCallback, this),
      false);

  return true;
}

void OpenSLESInputStream::SimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf buffer_queue,
    void* instance) {
  OpenSLESInputStream* stream =
      reinterpret_cast<OpenSLESInputStream*>(instance);
  stream->ReadBufferQueue();
}

void OpenSLESInputStream::ReadBufferQueue() {
  base::AutoLock lock(lock_);
  if (!started_)
    return;

  TRACE_EVENT0("audio", "OpenSLESOutputStream::ReadBufferQueue");

  // Convert from interleaved format to deinterleaved audio bus format.
  audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(
      reinterpret_cast<int16_t*>(audio_data_[active_buffer_index_]),
      audio_bus_->frames());

  peak_detector_.FindPeak(audio_bus_.get());

  // TODO(henrika): Investigate if it is possible to get an accurate
  // delay estimation.
  callback_->OnData(audio_bus_.get(), base::TimeTicks::Now() - hardware_delay_,
                    0.0, {});

  // Done with this buffer. Send it to device for recording.
  SLresult err =
      (*simple_buffer_queue_)->Enqueue(simple_buffer_queue_,
                                       audio_data_[active_buffer_index_],
                                       buffer_size_bytes_);
  if (SL_RESULT_SUCCESS != err)
    HandleError(err);

  active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;
}

void OpenSLESInputStream::SetupAudioBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!audio_data_[0]);
  for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
    audio_data_[i] = new uint8_t[buffer_size_bytes_];
  }
}

void OpenSLESInputStream::ReleaseAudioBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_data_[0]) {
    for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
      delete[] audio_data_[i];
      audio_data_[i] = nullptr;
    }
  }
}

void OpenSLESInputStream::HandleError(SLresult error) {
  DLOG(ERROR) << "OpenSLES Input error " << error;
  if (callback_)
    callback_->OnError();
}

}  // namespace media
