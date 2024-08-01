// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/pulse/pulse_input.h"

#include <stdint.h>

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/pulse/audio_manager_pulse.h"
#include "media/audio/pulse/pulse_util.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

using pulse::AutoPulseLock;
using pulse::WaitForOperationCompletion;

// Number of blocks of buffers used in the |fifo_|.
const int kNumberOfBlocksBufferInFifo = 2;

PulseAudioInputStream::PulseAudioInputStream(
    AudioManagerPulse* audio_manager,
    const std::string& source_name,
    const AudioParameters& params,
    pa_threaded_mainloop* mainloop,
    pa_context* context,
    AudioManager::LogCallback log_callback)
    : audio_manager_(audio_manager),
      callback_(nullptr),
      source_name_(source_name),
      params_(params),
      channels_(0),
      volume_(0.0),
      stream_started_(false),
      muted_(false),
      fifo_(params.channels(),
            params.frames_per_buffer(),
            kNumberOfBlocksBufferInFifo),
      pa_mainloop_(mainloop),
      pa_context_(context),
      log_callback_(std::move(log_callback)),
      handle_(nullptr),
      peak_detector_(
          audio_manager ? base::BindRepeating(&AudioManager::TraceAmplitudePeak,
                                              base::Unretained(audio_manager_),
                                              /*trace_start=*/true)
                        : base::RepeatingClosure()) {
  DCHECK(mainloop);
  DCHECK(context);
  CHECK(params_.IsValid());
  SendLogMessage("%s({device_id=%s}, {params=[%s]})", __func__,
                 source_name.c_str(), params.AsHumanReadableString().c_str());
  // TODO(crbug.com/40281249): PulseLoopbackAudioStream gives
  // PulseAudioInputStream a nullptr for `audio_manager`, which is risky.
  // Refactor such that this is not the case, or separate the
  // AudioManager-independent logic into a "PulseUnmanagedAudioInputStream".
}

PulseAudioInputStream::~PulseAudioInputStream() {
  // All internal structures should already have been freed in Close(),
  // which calls AudioManagerPulse::Release which deletes this object.
  DCHECK(!handle_);
}

AudioInputStream::OpenOutcome PulseAudioInputStream::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage("%s()", __func__);
  if (source_name_ == AudioDeviceDescription::kDefaultDeviceId &&
      audio_manager_ && audio_manager_->DefaultSourceIsMonitor()) {
    SendLogMessage("%s => (ERROR: can't open monitor device)", __func__);
    return OpenOutcome::kFailed;
  }

  AutoPulseLock auto_lock(pa_mainloop_);
  if (!pulse::CreateInputStream(pa_mainloop_, pa_context_, &handle_, params_,
                                source_name_, &StreamNotifyCallback, this)) {
    SendLogMessage("%s => (ERROR: failed to open PA stream)", __func__);
    return OpenOutcome::kFailed;
  }

  DCHECK(handle_);

  return OpenOutcome::kSuccess;
}

void PulseAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  DCHECK(handle_);
  SendLogMessage("%s()", __func__);

  // AGC needs to be started out of the lock.
  StartAgc();

  AutoPulseLock auto_lock(pa_mainloop_);

  if (stream_started_)
    return;

  // Start the streaming.
  callback_ = callback;
  pa_stream_set_read_callback(handle_, &ReadCallback, this);
  pa_stream_readable_size(handle_);
  stream_started_ = true;

  pa_operation* operation =
      pa_stream_cork(handle_, 0, &pulse::StreamSuccessCallback, pa_mainloop_);

  if (!WaitForOperationCompletion(pa_mainloop_, operation, pa_context_,
                                  handle_)) {
    callback_->OnError();
  }
}

void PulseAudioInputStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage("%s()", __func__);
  AutoPulseLock auto_lock(pa_mainloop_);
  if (!stream_started_)
    return;

  StopAgc();

  // Set the flag to false to stop filling new data to soundcard.
  stream_started_ = false;

  // Clean up the old buffer.
  pa_stream_drop(handle_);
  fifo_.Clear();

  pa_operation* operation =
      pa_stream_flush(handle_, &pulse::StreamSuccessCallback, pa_mainloop_);
  if (!WaitForOperationCompletion(pa_mainloop_, operation, pa_context_,
                                  handle_)) {
    callback_->OnError();
  }

  // Stop the stream.
  pa_stream_set_read_callback(handle_, nullptr, nullptr);
  operation =
      pa_stream_cork(handle_, 1, &pulse::StreamSuccessCallback, pa_mainloop_);
  if (!WaitForOperationCompletion(pa_mainloop_, operation, pa_context_,
                                  handle_)) {
    callback_->OnError();
  }
  callback_ = nullptr;
}

void PulseAudioInputStream::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage("%s()", __func__);
  {
    AutoPulseLock auto_lock(pa_mainloop_);
    if (handle_) {
      // Disable all the callbacks before disconnecting.
      pa_stream_set_state_callback(handle_, nullptr, nullptr);
      pa_operation* operation =
          pa_stream_flush(handle_, &pulse::StreamSuccessCallback, pa_mainloop_);
      WaitForOperationCompletion(pa_mainloop_, operation, pa_context_, handle_);

      if (pa_stream_get_state(handle_) != PA_STREAM_UNCONNECTED)
        pa_stream_disconnect(handle_);

      // Release PulseAudio structures.
      pa_stream_unref(handle_.ExtractAsDangling());
      handle_ = nullptr;
    }
  }

  // If the stream is not managed by AudioManager, the owner is responsible to
  // destroy the object.
  if (audio_manager_) {
    // Signal to the manager that we're closed and can be removed.
    // This should be the last call in the function as it deletes `this`.
    audio_manager_->ReleaseInputStream(this);
  }
}

double PulseAudioInputStream::GetMaxVolume() {
  return static_cast<double>(PA_VOLUME_NORM);
}

void PulseAudioInputStream::SetVolume(double volume) {
  AutoPulseLock auto_lock(pa_mainloop_);
  if (!handle_)
    return;
  SendLogMessage("%s({volume=%.2f})", __func__, volume);

  size_t index = pa_stream_get_device_index(handle_);
  pa_operation* operation = nullptr;
  if (!channels_) {
    // Get the number of channels for the source only when the |channels_| is 0.
    // We are assuming the stream source is not changed on the fly here.
    operation = pa_context_get_source_info_by_index(pa_context_, index,
                                                    &VolumeCallback, this);
    if (!WaitForOperationCompletion(pa_mainloop_, operation, pa_context_,
                                    handle_) ||
        !channels_) {
      SendLogMessage("%s => (WARNING: failed to read number of channels)",
                     __func__);
      return;
    }
  }

  pa_cvolume pa_volume;
  pa_cvolume_set(&pa_volume, channels_, volume);
  operation = pa_context_set_source_volume_by_index(
      pa_context_, index, &pa_volume, nullptr, nullptr);

  // Don't need to wait for this task to complete.
  pa_operation_unref(operation);
}

double PulseAudioInputStream::GetVolume() {
  if (pa_threaded_mainloop_in_thread(pa_mainloop_)) {
    // When being called by the pulse thread, GetVolume() is asynchronous and
    // called under AutoPulseLock.
    if (!handle_)
      return 0.0;

    size_t index = pa_stream_get_device_index(handle_);
    pa_operation* operation = pa_context_get_source_info_by_index(
        pa_context_, index, &VolumeCallback, this);
    // Do not wait for the operation since we can't block the pulse thread.
    pa_operation_unref(operation);

    // Return zero and the callback will asynchronously update the |volume_|.
    return 0.0;
  } else {
    GetSourceInformation(&VolumeCallback);
    return volume_;
  }
}

bool PulseAudioInputStream::IsMuted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetSourceInformation(&MuteCallback);
  return muted_;
}

void PulseAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

void PulseAudioInputStream::SendLogMessage(const char* format, ...) {
  if (log_callback_.is_null())
    return;
  va_list args;
  va_start(args, format);
  log_callback_.Run("PAIS::" + base::StringPrintV(format, args));
  va_end(args);
}

// static, used by pa_stream_set_read_callback.
void PulseAudioInputStream::ReadCallback(pa_stream* handle,
                                         size_t length,
                                         void* user_data) {
  PulseAudioInputStream* stream =
      reinterpret_cast<PulseAudioInputStream*>(user_data);

  stream->ReadData();
}

// static, used by pa_context_get_source_info_by_index.
void PulseAudioInputStream::VolumeCallback(pa_context* context,
                                           const pa_source_info* info,
                                           int error, void* user_data) {
  PulseAudioInputStream* stream =
      reinterpret_cast<PulseAudioInputStream*>(user_data);

  if (error) {
    pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
    return;
  }

  if (stream->channels_ != info->channel_map.channels)
    stream->channels_ = info->channel_map.channels;

  pa_volume_t volume = PA_VOLUME_MUTED;  // Minimum possible value.
  // Use the max volume of any channel as the volume.
  for (int i = 0; i < stream->channels_; ++i) {
    if (volume < info->volume.values[i])
      volume = info->volume.values[i];
  }

  // It is safe to access |volume_| here since VolumeCallback() is running
  // under PulseLock.
  stream->volume_ = static_cast<double>(volume);
}

// static, used by pa_context_get_source_info_by_index.
void PulseAudioInputStream::MuteCallback(pa_context* context,
                                         const pa_source_info* info,
                                         int error,
                                         void* user_data) {
  // Runs on PulseAudio callback thread. It might be possible to make this
  // method more thread safe by passing a struct (or pair) of a local copy of
  // |pa_mainloop_| and |muted_| instead.
  PulseAudioInputStream* stream =
      reinterpret_cast<PulseAudioInputStream*>(user_data);

  // Avoid infinite wait loop in case of error.
  if (error) {
    pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
    return;
  }

  stream->muted_ = info->mute != 0;
}

// static, used by pa_stream_set_state_callback.
void PulseAudioInputStream::StreamNotifyCallback(pa_stream* s,
                                                 void* user_data) {
  PulseAudioInputStream* stream =
      reinterpret_cast<PulseAudioInputStream*>(user_data);

  if (s && stream->callback_ &&
      pa_stream_get_state(s) == PA_STREAM_FAILED) {
    stream->callback_->OnError();
  }

  pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
}

void PulseAudioInputStream::ReadData() {
  // Update the AGC volume level once every second. Note that,
  // |volume| is also updated each time SetVolume() is called
  // through IPC by the render-side AGC.
  // We disregard the |normalized_volume| from GetAgcVolume()
  // and use the value calculated by |volume_|.
  double normalized_volume = 0.0;
  GetAgcVolume(&normalized_volume);
  normalized_volume = volume_ / GetMaxVolume();

  do {
    size_t length = 0;
    const void* data = nullptr;
    pa_stream_peek(handle_, &data, &length);
    if (!data || length == 0)
      break;

    const int number_of_frames =
        length / params_.GetBytesPerFrame(pulse::kInputSampleFormat);
    if (number_of_frames > fifo_.GetUnfilledFrames()) {
      // Dynamically increase capacity to the FIFO to handle larger buffer got
      // from Pulse.
      const int increase_blocks_of_buffer =
          static_cast<int>((number_of_frames - fifo_.GetUnfilledFrames()) /
                           params_.frames_per_buffer()) +
          1;
      fifo_.IncreaseCapacity(increase_blocks_of_buffer);
    }

    const int bytes_per_sample =
        SampleFormatToBytesPerChannel(pulse::kInputSampleFormat);

    peak_detector_.FindPeak(data, number_of_frames, bytes_per_sample);

    fifo_.Push(data, number_of_frames, bytes_per_sample);

    // Checks if we still have data.
    pa_stream_drop(handle_);
  } while (pa_stream_readable_size(handle_) > 0);

  const base::TimeTicks capture_time_base =
      base::TimeTicks::Now() - pulse::GetHardwareLatency(handle_);
  while (fifo_.available_blocks()) {
    // Compensate the audio delay caused by the FIFO.
    // TODO(dalecurtis): This should probably use pa_stream_get_time() so we can
    // get the capture time directly.
    const base::TimeTicks capture_time =
        capture_time_base -
        AudioTimestampHelper::FramesToTime(fifo_.GetAvailableFrames(),
                                           params_.sample_rate());
    const AudioBus* audio_bus = fifo_.Consume();

    callback_->OnData(audio_bus, capture_time, normalized_volume, {});

  }

  pa_threaded_mainloop_signal(pa_mainloop_, 0);
}

bool PulseAudioInputStream::GetSourceInformation(pa_source_info_cb_t callback) {
  AutoPulseLock auto_lock(pa_mainloop_);
  if (!handle_)
    return false;

  size_t index = pa_stream_get_device_index(handle_);
  pa_operation* operation =
      pa_context_get_source_info_by_index(pa_context_, index, callback, this);
  return WaitForOperationCompletion(pa_mainloop_, operation, pa_context_,
                                    handle_);
}

}  // namespace media
