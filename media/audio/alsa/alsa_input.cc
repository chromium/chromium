// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/alsa/alsa_input.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "media/audio/alsa/alsa_output.h"
#include "media/audio/alsa/alsa_util.h"
#include "media/audio/alsa/alsa_wrapper.h"
#include "media/audio/alsa/audio_manager_alsa.h"
#include "media/audio/audio_manager.h"

namespace media {

static const SampleFormat kSampleFormat = kSampleFormatS16;
static const snd_pcm_format_t kAlsaSampleFormat = SND_PCM_FORMAT_S16;

static const int kNumPacketsInRingBuffer = 3;

static const char kDefaultDevice1[] = "default";
static const char kDefaultDevice2[] = "plug:default";

const char AlsaPcmInputStream::kAutoSelectDevice[] = "";

AlsaPcmInputStream::AlsaPcmInputStream(AudioManagerBase* audio_manager,
                                       const std::string& device_name,
                                       const AudioParameters& params,
                                       AlsaWrapper* wrapper)
    : audio_manager_(audio_manager),
      device_name_(device_name),
      params_(params),
      bytes_per_buffer_(params.GetBytesPerBuffer(kSampleFormat)),
      wrapper_(wrapper),
      buffer_duration_(base::Microseconds(
          params.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
          static_cast<float>(params.sample_rate()))),
      callback_(nullptr),
      device_handle_(nullptr),
      mixer_handle_(nullptr),
      mixer_element_handle_(nullptr),
      read_callback_behind_schedule_(false),
      audio_bus_(AudioBus::Create(params)),
      capture_thread_("AlsaInput"),
      running_(false) {}

AlsaPcmInputStream::~AlsaPcmInputStream() = default;

AudioInputStream::OpenOutcome AlsaPcmInputStream::Open() {
  if (device_handle_)
    return OpenOutcome::kAlreadyOpen;

  uint32_t packet_us = buffer_duration_.InMicroseconds();
  uint32_t buffer_us = packet_us * kNumPacketsInRingBuffer;

  // Use the same minimum required latency as output.
  buffer_us = std::max(buffer_us, AlsaPcmOutputStream::kMinLatencyMicros);

  if (device_name_ == kAutoSelectDevice) {
    const char* device_names[] = { kDefaultDevice1, kDefaultDevice2 };
    for (size_t i = 0; i < std::size(device_names); ++i) {
      device_handle_ = alsa_util::OpenCaptureDevice(
          wrapper_, device_names[i], params_.channels(), params_.sample_rate(),
          kAlsaSampleFormat, buffer_us, packet_us);

      if (device_handle_) {
        device_name_ = device_names[i];
        break;
      }
    }
  } else {
    device_handle_ = alsa_util::OpenCaptureDevice(
        wrapper_, device_name_.c_str(), params_.channels(),
        params_.sample_rate(), kAlsaSampleFormat, buffer_us, packet_us);
  }

  if (device_handle_) {
    audio_buffer_.reset(new uint8_t[bytes_per_buffer_]);

    // Open the microphone mixer.
    mixer_handle_ = alsa_util::OpenMixer(wrapper_, device_name_);
    if (mixer_handle_) {
      mixer_element_handle_ = alsa_util::LoadCaptureMixerElement(
          wrapper_, mixer_handle_);
    }
  }

  return device_handle_ != nullptr ? OpenOutcome::kSuccess
                                   : OpenOutcome::kFailed;
}

void AlsaPcmInputStream::Start(AudioInputCallback* callback) {
  DCHECK(!callback_ && callback);
  callback_ = callback;
  StartAgc();
  int error = wrapper_->PcmPrepare(device_handle_);
  if (error < 0) {
    HandleError("PcmPrepare", error);
  } else {
    error = wrapper_->PcmStart(device_handle_);
    if (error < 0)
      HandleError("PcmStart", error);
  }

  if (error < 0) {
    callback_ = nullptr;
  } else {
    CHECK(capture_thread_.StartWithOptions(
        base::Thread::Options(base::ThreadType::kRealtimeAudio)));

    // We start reading data half |buffer_duration_| later than when the
    // buffer might have got filled, to accommodate some delays in the audio
    // driver. This could also give us a smooth read sequence going forward.
    base::TimeDelta delay = buffer_duration_ + buffer_duration_ / 2;
    next_read_time_ = base::TimeTicks::Now() + delay;
    running_ = true;
    capture_thread_.task_runner()->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
        base::BindOnce(&AlsaPcmInputStream::ReadAudio, base::Unretained(this)),
        next_read_time_, base::subtle::DelayPolicy::kPrecise);
  }
}

bool AlsaPcmInputStream::Recover(int original_error) {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());
  int error = wrapper_->PcmRecover(device_handle_, original_error, 1);
  if (error < 0) {
    // Docs say snd_pcm_recover returns the original error if it is not one
    // of the recoverable ones, so this log message will probably contain the
    // same error twice.
    LOG(WARNING) << "Unable to recover from \""
                 << wrapper_->StrError(original_error) << "\": "
                 << wrapper_->StrError(error);
    return false;
  }

  if (original_error == -EPIPE) {  // Buffer underrun/overrun.
    // For capture streams we have to repeat the explicit start() to get
    // data flowing again.
    error = wrapper_->PcmStart(device_handle_);
    if (error < 0) {
      HandleError("PcmStart", error);
      return false;
    }
  }

  return true;
}

void AlsaPcmInputStream::StopRunningOnCaptureThread() {
  DCHECK(capture_thread_.IsRunning());
  if (!capture_thread_.task_runner()->BelongsToCurrentThread()) {
    capture_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AlsaPcmInputStream::StopRunningOnCaptureThread,
                       base::Unretained(this)));
    return;
  }
  running_ = false;
}

void AlsaPcmInputStream::ReadAudio() {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(callback_);
  if (!running_)
    return;

  snd_pcm_sframes_t frames = wrapper_->PcmAvailUpdate(device_handle_);
  if (frames < 0) {  // Potentially recoverable error?
    LOG(WARNING) << "PcmAvailUpdate(): " << wrapper_->StrError(frames);
    Recover(frames);
  }

  if (frames < params_.frames_per_buffer()) {
    base::TimeTicks now = base::TimeTicks::Now();
    // Not enough data yet or error happened. In both cases wait for a very
    // small duration before checking again.
    // Even Though read callback was behind schedule, there is no data, so
    // reset the next_read_time_.
    if (read_callback_behind_schedule_) {
      next_read_time_ = now;
      read_callback_behind_schedule_ = false;
    }

    base::TimeTicks next_check_time = now + buffer_duration_ / 2;
    capture_thread_.task_runner()->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
        base::BindOnce(&AlsaPcmInputStream::ReadAudio, base::Unretained(this)),
        next_check_time, base::subtle::DelayPolicy::kPrecise);
    return;
  }

  // Update the AGC volume level once every second. Note that, |volume| is
  // also updated each time SetVolume() is called through IPC by the
  // render-side AGC.
  double normalized_volume = 0.0;
  GetAgcVolume(&normalized_volume);

  int num_buffers = frames / params_.frames_per_buffer();
  while (num_buffers--) {
    int frames_read = wrapper_->PcmReadi(device_handle_, audio_buffer_.get(),
                                         params_.frames_per_buffer());
    if (frames_read == params_.frames_per_buffer()) {
      audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(
          reinterpret_cast<int16_t*>(audio_buffer_.get()),
          audio_bus_->frames());

      // TODO(dalecurtis): This should probably use snd_pcm_htimestamp() so that
      // we can have |capture_time| directly instead of computing it as
      // Now() - available frames.
      snd_pcm_sframes_t avail_frames = wrapper_->PcmAvailUpdate(device_handle_);
      if (avail_frames < 0) {
        LOG(WARNING) << "PcmAvailUpdate(): "
                     << wrapper_->StrError(avail_frames);
        avail_frames = 0;  // Error getting number of avail frames, set it to 0
      }
      base::TimeDelta hardware_delay = base::Seconds(
          avail_frames / static_cast<double>(params_.sample_rate()));

      callback_->OnData(audio_bus_.get(),
                        base::TimeTicks::Now() - hardware_delay,
                        normalized_volume, {});
    } else if (frames_read < 0) {
      bool success = Recover(frames_read);
      LOG(WARNING) << "PcmReadi failed with error "
                   << wrapper_->StrError(frames_read) << ". "
                   << (success ? "Successfully" : "Unsuccessfully")
                   << " recovered.";
    } else {
      LOG(WARNING) << "PcmReadi returning less than expected frames: "
                   << frames_read << " vs. " << params_.frames_per_buffer()
                   << ". Dropping this buffer.";
    }
  }

  next_read_time_ += buffer_duration_;
  base::TimeTicks now = base::TimeTicks::Now();
  if (next_read_time_ < now) {
    base::TimeDelta delay = now - next_read_time_;
    DVLOG(1) << "Audio read callback behind schedule by "
             << (buffer_duration_ + delay).InMicroseconds() << " (us).";
    // Read callback is behind schedule. Assuming there is data pending in
    // the soundcard, invoke the read callback immediate in order to catch up.
    read_callback_behind_schedule_ = true;
  }

  // If |next_read_time_| is in the past, it will be scheduled immediately.
  capture_thread_.task_runner()->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&AlsaPcmInputStream::ReadAudio, base::Unretained(this)),
      next_read_time_, base::subtle::DelayPolicy::kPrecise);
}

void AlsaPcmInputStream::Stop() {
  if (!device_handle_ || !callback_)
    return;

  StopAgc();

  StopRunningOnCaptureThread();
  capture_thread_.Stop();
  int error = wrapper_->PcmDrop(device_handle_);
  if (error < 0)
    HandleError("PcmDrop", error);

  callback_ = nullptr;
}

void AlsaPcmInputStream::Close() {
  if (device_handle_) {
    Stop();
    int error =
        alsa_util::CloseDevice(wrapper_, device_handle_.ExtractAsDangling());

    if (error < 0) {
      HandleError("PcmClose", error);
    }

    mixer_element_handle_ = nullptr;

    if (mixer_handle_) {
      alsa_util::CloseMixer(wrapper_, mixer_handle_.ExtractAsDangling(),
                            device_name_);
    }

    audio_buffer_.reset();
  }

  audio_manager_->ReleaseInputStream(this);
}

double AlsaPcmInputStream::GetMaxVolume() {
  if (!mixer_handle_ || !mixer_element_handle_) {
    DLOG(WARNING) << "GetMaxVolume is not supported for " << device_name_;
    return 0.0;
  }

  if (!wrapper_->MixerSelemHasCaptureVolume(mixer_element_handle_)) {
    DLOG(WARNING) << "Unsupported microphone volume for " << device_name_;
    return 0.0;
  }

  long min = 0;
  long max = 0;
  if (wrapper_->MixerSelemGetCaptureVolumeRange(mixer_element_handle_,
                                                &min,
                                                &max)) {
    DLOG(WARNING) << "Unsupported max microphone volume for " << device_name_;
    return 0.0;
  }
  DCHECK(min == 0);
  DCHECK(max > 0);

  return static_cast<double>(max);
}

void AlsaPcmInputStream::SetVolume(double volume) {
  if (!mixer_handle_ || !mixer_element_handle_) {
    DLOG(WARNING) << "SetVolume is not supported for " << device_name_;
    return;
  }

  int error = wrapper_->MixerSelemSetCaptureVolumeAll(
      mixer_element_handle_, static_cast<long>(volume));
  if (error < 0) {
    DLOG(WARNING) << "Unable to set volume for " << device_name_;
  }

  // Update the AGC volume level based on the last setting above. Note that,
  // the volume-level resolution is not infinite and it is therefore not
  // possible to assume that the volume provided as input parameter can be
  // used directly. Instead, a new query to the audio hardware is required.
  // This method does nothing if AGC is disabled.
  UpdateAgcVolume();
}

double AlsaPcmInputStream::GetVolume() {
  if (!mixer_handle_ || !mixer_element_handle_) {
    DLOG(WARNING) << "GetVolume is not supported for " << device_name_;
    return 0.0;
  }

  long current_volume = 0;
  int error = wrapper_->MixerSelemGetCaptureVolume(
      mixer_element_handle_, static_cast<snd_mixer_selem_channel_id_t>(0),
      &current_volume);
  if (error < 0) {
    DLOG(WARNING) << "Unable to get volume for " << device_name_;
    return 0.0;
  }

  return static_cast<double>(current_volume);
}

bool AlsaPcmInputStream::IsMuted() {
  return false;
}

void AlsaPcmInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

void AlsaPcmInputStream::HandleError(const char* method, int error) {
  LOG(WARNING) << method << ": " << wrapper_->StrError(error);
  if (callback_)
    callback_->OnError();
}

}  // namespace media
