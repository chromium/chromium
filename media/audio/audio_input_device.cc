// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_device.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"

namespace media {

namespace {

// The number of shared memory buffer segments indicated to browser process
// in order to avoid data overwriting. This number can be any positive number,
// dependent how fast the renderer process can pick up captured data from
// shared memory.
const int kRequestedSharedMemoryCount = 10;

// The number of seconds with missing callbacks before we report a capture
// error. The value is based on that the Mac audio implementation can defer
// start for 5 seconds when resuming after standby, and has a startup success
// check 5 seconds after actually starting, where stats is logged. We must allow
// enough time for this. See AUAudioInputStream::CheckInputStartupSuccess().
const int kMissingCallbacksTimeBeforeErrorSeconds = 12;

// The interval for checking missing callbacks.
const int kCheckMissingCallbacksIntervalSeconds = 5;

// How often AudioInputDevice::AudioThreadCallback informs that it has gotten
// data from the source.
const int kGotDataCallbackIntervalSeconds = 1;

base::ThreadPriority ThreadPriorityFromPurpose(
    AudioInputDevice::Purpose purpose) {
  switch (purpose) {
    case AudioInputDevice::Purpose::kUserInput:
      return base::ThreadPriority::REALTIME_AUDIO;
    case AudioInputDevice::Purpose::kLoopback:
      return base::ThreadPriority::NORMAL;
  }
}

}  // namespace

// Takes care of invoking the capture callback on the audio thread.
// An instance of this class is created for each capture stream in
// OnLowLatencyCreated().
class AudioInputDevice::AudioThreadCallback
    : public AudioDeviceThread::Callback {
 public:
  AudioThreadCallback(const AudioParameters& audio_parameters,
                      base::ReadOnlySharedMemoryRegion shared_memory_region,
                      uint32_t total_segments,
                      bool enable_uma,
                      CaptureCallback* capture_callback,
                      base::RepeatingClosure got_data_callback);
  ~AudioThreadCallback() override;

  void MapSharedMemory() override;

  // Called whenever we receive notifications about pending data.
  void Process(uint32_t pending_data) override;

 private:
  const bool enable_uma_;
  base::ReadOnlySharedMemoryRegion shared_memory_region_;
  base::ReadOnlySharedMemoryMapping shared_memory_mapping_;
  const base::TimeTicks start_time_;
  bool no_callbacks_received_;
  size_t current_segment_id_;
  uint32_t last_buffer_id_;
  std::vector<std::unique_ptr<const media::AudioBus>> audio_buses_;
  CaptureCallback* capture_callback_;

  // Used for informing AudioInputDevice that we have gotten data, i.e. the
  // stream is alive. |got_data_callback_| is run every
  // |got_data_callback_interval_in_frames_| frames, calculated from
  // kGotDataCallbackIntervalSeconds.
  const int got_data_callback_interval_in_frames_;
  int frames_since_last_got_data_callback_;
  base::RepeatingClosure got_data_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioThreadCallback);
};

AudioInputDevice::AudioInputDevice(std::unique_ptr<AudioInputIPC> ipc,
                                   Purpose purpose)
    : thread_priority_(ThreadPriorityFromPurpose(purpose)),
      enable_uma_(purpose == AudioInputDevice::Purpose::kUserInput),
      callback_(nullptr),
      ipc_(std::move(ipc)),
      state_(IDLE),
      agc_is_enabled_(false) {
  CHECK(ipc_);

  // The correctness of the code depends on the relative values assigned in the
  // State enum.
  static_assert(IPC_CLOSED < IDLE, "invalid enum value assignment 0");
  static_assert(IDLE < CREATING_STREAM, "invalid enum value assignment 1");
  static_assert(CREATING_STREAM < RECORDING, "invalid enum value assignment 2");
}

void AudioInputDevice::Initialize(const AudioParameters& params,
                                  CaptureCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(params.IsValid());
  DCHECK(!callback_);
  audio_parameters_ = params;
  callback_ = callback;
}

void AudioInputDevice::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_) << "Initialize hasn't been called";
  TRACE_EVENT0("audio", "AudioInputDevice::Start");

  // Make sure we don't call Start() more than once.
  if (state_ != IDLE)
    return;

  state_ = CREATING_STREAM;
  ipc_->CreateStream(this, audio_parameters_, agc_is_enabled_,
                     kRequestedSharedMemoryCount);
}

void AudioInputDevice::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "AudioInputDevice::Stop");

  if (enable_uma_) {
    UMA_HISTOGRAM_BOOLEAN(
        "Media.Audio.Capture.DetectedMissingCallbacks",
        alive_checker_ ? alive_checker_->DetectedDead() : false);

    UMA_HISTOGRAM_ENUMERATION("Media.Audio.Capture.StreamCallbackError2",
                              had_error_);
  }
  had_error_ = kNoError;

  // Close the stream, if we haven't already.
  if (state_ >= CREATING_STREAM) {
    ipc_->CloseStream();
    state_ = IDLE;
    agc_is_enabled_ = false;
  }

  // We can run into an issue where Stop is called right after
  // OnStreamCreated is called in cases where Start/Stop are called before we
  // get the OnStreamCreated callback.  To handle that corner case, we call
  // audio_thread_.reset(). In most cases, the thread will already be stopped.
  //
  // |alive_checker_| must outlive |audio_callback_|.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
  audio_thread_.reset();
  audio_callback_.reset();
  alive_checker_.reset();
}

void AudioInputDevice::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("audio", "AudioInputDevice::SetVolume", "volume", volume);

  if (volume < 0 || volume > 1.0) {
    DLOG(ERROR) << "Invalid volume value specified";
    return;
  }

  if (state_ >= CREATING_STREAM)
    ipc_->SetVolume(volume);
}

void AudioInputDevice::SetAutomaticGainControl(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("audio", "AudioInputDevice::SetAutomaticGainControl", "enabled",
               enabled);

  if (state_ >= CREATING_STREAM) {
    DLOG(WARNING) << "The AGC state can not be modified after starting.";
    return;
  }

  // We simply store the new AGC setting here. This value will be used when
  // a new stream is initialized and by GetAutomaticGainControl().
  agc_is_enabled_ = enabled;
}

void AudioInputDevice::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("audio", "AudioInputDevice::SetOutputDeviceForAec",
               "output_device_id", output_device_id);

  output_device_id_for_aec_ = output_device_id;
  if (state_ > CREATING_STREAM)
    ipc_->SetOutputDeviceForAec(output_device_id);
}

void AudioInputDevice::OnStreamCreated(
    base::ReadOnlySharedMemoryRegion shared_memory_region,
    base::SyncSocket::Handle socket_handle,
    bool initially_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "AudioInputDevice::OnStreamCreated");
  DCHECK(shared_memory_region.IsValid());
#if defined(OS_WIN)
  DCHECK(socket_handle);
#else
  DCHECK_GE(socket_handle, 0);
#endif
  DCHECK_GT(shared_memory_region.GetSize(), 0u);

  if (state_ != CREATING_STREAM)
    return;

  DCHECK(!audio_callback_);
  DCHECK(!audio_thread_);

  if (initially_muted)
    callback_->OnCaptureMuted(true);

  if (auto* controls = ipc_->GetProcessorControls())
    callback_->OnCaptureProcessorCreated(controls);

  if (output_device_id_for_aec_)
    ipc_->SetOutputDeviceForAec(*output_device_id_for_aec_);

// Set up checker for detecting missing audio data. We pass a callback which
// holds a reference to this. |alive_checker_| is deleted in
// Stop() which we expect to always be called (see comment in
// destructor). Suspend/resume notifications are not supported on Linux and
// there's a risk of false positives when suspending. So on Linux we only detect
// missing audio data until the first audio buffer arrives. Note that there's
// also a risk of false positives if we are suspending when starting the stream
// here. See comments in AliveChecker and PowerObserverHelper for details and
// todos.
#if defined(OS_LINUX)
  const bool stop_at_first_alive_notification = true;
  const bool pause_check_during_suspend = false;
#else
  const bool stop_at_first_alive_notification = false;
  const bool pause_check_during_suspend = true;
#endif
  alive_checker_ = std::make_unique<AliveChecker>(
      base::BindRepeating(&AudioInputDevice::DetectedDeadInputStream, this),
      base::TimeDelta::FromSeconds(kCheckMissingCallbacksIntervalSeconds),
      base::TimeDelta::FromSeconds(kMissingCallbacksTimeBeforeErrorSeconds),
      stop_at_first_alive_notification, pause_check_during_suspend);

  // Unretained is safe since |alive_checker_| outlives |audio_callback_|.
  audio_callback_ = std::make_unique<AudioInputDevice::AudioThreadCallback>(
      audio_parameters_, std::move(shared_memory_region),
      kRequestedSharedMemoryCount, enable_uma_, callback_,
      base::BindRepeating(&AliveChecker::NotifyAlive,
                          base::Unretained(alive_checker_.get())));
  audio_thread_ =
      std::make_unique<AudioDeviceThread>(audio_callback_.get(), socket_handle,
                                          "AudioInputDevice", thread_priority_);

  state_ = RECORDING;
  ipc_->RecordStream();

  // Start detecting missing audio data.
  alive_checker_->Start();
}

void AudioInputDevice::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "AudioInputDevice::OnError");

  // Do nothing if the stream has been closed.
  if (state_ < CREATING_STREAM)
    return;


  if (state_ == CREATING_STREAM) {
    // At this point, we haven't attempted to start the audio thread.
    // Accessing the hardware might have failed or we may have reached
    // the limit of the number of allowed concurrent streams.
    // We must report the error to the |callback_| so that a potential
    // audio source object will enter the correct state (e.g. 'ended' for
    // a local audio source).
    had_error_ = kErrorDuringCreation;
    callback_->OnCaptureError(
        "Maximum allowed input device limit reached or OS failure.");
  } else {
    // Don't dereference the callback object if the audio thread
    // is stopped or stopping.  That could mean that the callback
    // object has been deleted.
    // TODO(tommi): Add an explicit contract for clearing the callback
    // object.  Possibly require calling Initialize again or provide
    // a callback object via Start() and clear it in Stop().
    had_error_ = kErrorDuringCapture;
    if (audio_thread_)
      callback_->OnCaptureError("IPC delegate state error.");
  }
}

void AudioInputDevice::OnMuted(bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "AudioInputDevice::OnMuted");

  // Do nothing if the stream has been closed.
  if (state_ < CREATING_STREAM)
    return;
  callback_->OnCaptureMuted(is_muted);
}

void AudioInputDevice::OnIPCClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "AudioInputDevice::OnIPCClosed");

  state_ = IPC_CLOSED;
  ipc_.reset();
}

AudioInputDevice::~AudioInputDevice() {
#if DCHECK_IS_ON()
  // Make sure we've stopped the stream properly before destructing |this|.
  DCHECK_LE(state_, IDLE);
  DCHECK(!audio_thread_);
  DCHECK(!audio_callback_);
  DCHECK(!alive_checker_);
#endif  // DCHECK_IS_ON()
}

void AudioInputDevice::DetectedDeadInputStream() {
  callback_->OnCaptureError("No audio received from audio capture device.");
}

// AudioInputDevice::AudioThreadCallback
AudioInputDevice::AudioThreadCallback::AudioThreadCallback(
    const AudioParameters& audio_parameters,
    base::ReadOnlySharedMemoryRegion shared_memory_region,
    uint32_t total_segments,
    bool enable_uma,
    CaptureCallback* capture_callback,
    base::RepeatingClosure got_data_callback_)
    : AudioDeviceThread::Callback(
          audio_parameters,
          ComputeAudioInputBufferSize(audio_parameters, 1u),
          total_segments),
      enable_uma_(enable_uma),
      shared_memory_region_(std::move(shared_memory_region)),
      start_time_(base::TimeTicks::Now()),
      no_callbacks_received_(true),
      current_segment_id_(0u),
      last_buffer_id_(UINT32_MAX),
      capture_callback_(capture_callback),
      got_data_callback_interval_in_frames_(kGotDataCallbackIntervalSeconds *
                                            audio_parameters.sample_rate()),
      frames_since_last_got_data_callback_(0),
      got_data_callback_(std::move(got_data_callback_)) {
  // CHECK that the shared memory is large enough. The memory allocated must
  // be at least as large as expected.
  CHECK_LE(memory_length_, shared_memory_region_.GetSize());
}

AudioInputDevice::AudioThreadCallback::~AudioThreadCallback() {
  if (enable_uma_) {
    UMA_HISTOGRAM_LONG_TIMES("Media.Audio.Capture.InputStreamDuration",
                             base::TimeTicks::Now() - start_time_);
  }
}

void AudioInputDevice::AudioThreadCallback::MapSharedMemory() {
  shared_memory_mapping_ = shared_memory_region_.MapAt(0, memory_length_);

  // Create vector of audio buses by wrapping existing blocks of memory.
  const uint8_t* ptr =
      static_cast<const uint8_t*>(shared_memory_mapping_.memory());
  for (uint32_t i = 0; i < total_segments_; ++i) {
    const media::AudioInputBuffer* buffer =
        reinterpret_cast<const media::AudioInputBuffer*>(ptr);
    audio_buses_.push_back(
        media::AudioBus::WrapReadOnlyMemory(audio_parameters_, buffer->audio));
    ptr += segment_length_;
  }

  // Indicate that browser side capture initialization has succeeded and IPC
  // channel initialized. This effectively completes the
  // AudioCapturerSource::Start()' phase as far as the caller of that function
  // is concerned.
  capture_callback_->OnCaptureStarted();
}

void AudioInputDevice::AudioThreadCallback::Process(uint32_t pending_data) {
  TRACE_EVENT_BEGIN0("audio", "AudioInputDevice::AudioThreadCallback::Process");

  if (no_callbacks_received_) {
    if (enable_uma_) {
      UMA_HISTOGRAM_TIMES("Media.Audio.Render.InputDeviceStartTime",
                          base::TimeTicks::Now() - start_time_);
    }
    no_callbacks_received_ = false;
  }

  // The shared memory represents parameters, size of the data buffer and the
  // actual data buffer containing audio data. Map the memory into this
  // structure and parse out parameters and the data area.
  const uint8_t* ptr =
      static_cast<const uint8_t*>(shared_memory_mapping_.memory());
  ptr += current_segment_id_ * segment_length_;
  const AudioInputBuffer* buffer =
      reinterpret_cast<const AudioInputBuffer*>(ptr);

  // Usually this will be equal but in the case of low sample rate (e.g. 8kHz,
  // the buffer may be bigger (on mac at least)).
  DCHECK_GE(buffer->params.size,
            segment_length_ - sizeof(AudioInputBufferParameters));

  // Verify correct sequence.
  if (buffer->params.id != last_buffer_id_ + 1) {
    std::string message = base::StringPrintf(
        "Incorrect buffer sequence. Expected = %u. Actual = %u.",
        last_buffer_id_ + 1, buffer->params.id);
    LOG(ERROR) << message;
    capture_callback_->OnCaptureError(message);
  }
  if (current_segment_id_ != pending_data) {
    std::string message = base::StringPrintf(
        "Segment id not matching. Remote = %u. Local = %" PRIuS ".",
        pending_data, current_segment_id_);
    LOG(ERROR) << message;
    capture_callback_->OnCaptureError(message);
  }
  last_buffer_id_ = buffer->params.id;

  // Use pre-allocated audio bus wrapping existing block of shared memory.
  const media::AudioBus* audio_bus = audio_buses_[current_segment_id_].get();

  // Regularly inform that we have gotten data.
  frames_since_last_got_data_callback_ += audio_bus->frames();
  if (frames_since_last_got_data_callback_ >=
      got_data_callback_interval_in_frames_) {
    got_data_callback_.Run();
    frames_since_last_got_data_callback_ = 0;
  }

  // Deliver captured data to the client in floating point format and update
  // the audio delay measurement.
  // TODO(olka, tommi): Take advantage of |capture_time| in the renderer.
  const base::TimeTicks capture_time =
      base::TimeTicks() +
      base::TimeDelta::FromMicroseconds(buffer->params.capture_time_us);
  const base::TimeTicks now_time = base::TimeTicks::Now();
  DCHECK_GE(now_time, capture_time);

  capture_callback_->Capture(audio_bus, capture_time, buffer->params.volume,
                             buffer->params.key_pressed);

  if (++current_segment_id_ >= total_segments_)
    current_segment_id_ = 0u;

  TRACE_EVENT_END2(
      "audio", "AudioInputDevice::AudioThreadCallback::Process",
      "capture_time (ms)", (capture_time - base::TimeTicks()).InMillisecondsF(),
      "now_time (ms)", (now_time - base::TimeTicks()).InMillisecondsF());
}

}  // namespace media
