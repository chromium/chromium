// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_win.h"

#include <objbase.h>

#include <windows.h>

#include <avrt.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "remoting/host/win/default_audio_device_change_detector.h"

namespace {
const int kBytesPerSample = 2;
const int kBitsPerSample = kBytesPerSample * 8;
// Conversion factor from 100ns to 1ms.
const int k100nsPerMillisecond = 10000;

// Tolerance for catching packets of silence. If all samples have absolute
// value less than this threshold, the packet will be counted as a packet of
// silence. A value of 2 was chosen, because Windows can give samples of 1 and
// -1, even when no audio is playing.
const int kSilenceThreshold = 2;

// Lower bound for timer intervals, in milliseconds.
const int kMinTimerInterval = 30;

// Upper bound for the timer precision error, in milliseconds.
// Timers are supposed to be accurate to 20ms, so we use 30ms to be safe.
const int kMaxExpectedTimerLag = 30;

}  // namespace

namespace remoting {

AudioCapturerWin::AudioCapturerWin()
    : sampling_rate_(AudioPacket::SAMPLING_RATE_INVALID),
      volume_filter_(kSilenceThreshold),
      last_capture_error_(S_OK) {
  thread_checker_.DetachFromThread();
}

AudioCapturerWin::~AudioCapturerWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Deinitialize();
}

bool AudioCapturerWin::Start(const PacketCapturedCallback& callback) {
  callback_ = callback;

  if (!Initialize()) {
    return false;
  }

  // Initialize the capture timer and start capturing. Note, this timer won't
  // be reset or restarted in ResetAndInitialize() function. Which means we
  // expect the audio_device_period_ is a system wide configuration, it would
  // not be changed with the default audio device.
  capture_timer_ = std::make_unique<base::RepeatingTimer>();
  capture_timer_->Start(FROM_HERE, audio_device_period_, this,
                        &AudioCapturerWin::DoCapture);
  return true;
}

bool AudioCapturerWin::ResetAndInitialize() {
  Deinitialize();
  if (!Initialize()) {
    Deinitialize();
    return false;
  }
  return true;
}

void AudioCapturerWin::Deinitialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  wave_format_ex_.Reset(nullptr);
  default_device_detector_.reset();
  audio_capture_client_.Reset();
  if (audio_client_) {
    audio_client_->Stop();
  }
  audio_client_.Reset();
  mm_device_.Reset();
}

bool AudioCapturerWin::Initialize() {
  DCHECK(!audio_capture_client_.Get());
  DCHECK(!audio_client_.Get());
  DCHECK(!mm_device_.Get());
  DCHECK(static_cast<PWAVEFORMATEX>(wave_format_ex_) == nullptr);
  DCHECK(thread_checker_.CalledOnValidThread());

  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> mm_device_enumerator;
  hr = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          IID_PPV_ARGS(&mm_device_enumerator));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create IMMDeviceEnumerator. Error " << hr;
    return false;
  }

  default_device_detector_ =
      std::make_unique<DefaultAudioDeviceChangeDetector>(mm_device_enumerator);

  // Get the audio endpoint.
  hr = mm_device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                     &mm_device_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get IMMDevice. Error " << hr;
    return false;
  }

  // Get an audio client.
  hr = mm_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                            &audio_client_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get an IAudioClient. Error " << hr;
    return false;
  }

  REFERENCE_TIME device_period;
  hr = audio_client_->GetDevicePeriod(&device_period, nullptr);
  if (FAILED(hr)) {
    LOG(ERROR) << "IAudioClient::GetDevicePeriod failed. Error " << hr;
    return false;
  }
  // We round up, if |device_period| / |k100nsPerMillisecond|
  // is not a whole number.
  int device_period_in_milliseconds =
      1 + ((device_period - 1) / k100nsPerMillisecond);
  audio_device_period_ = base::Milliseconds(
      std::max(device_period_in_milliseconds, kMinTimerInterval));

  // Get the wave format.
  hr = audio_client_->GetMixFormat(&wave_format_ex_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get WAVEFORMATEX. Error " << hr;
    return false;
  }

  if (wave_format_ex_->wFormatTag != WAVE_FORMAT_IEEE_FLOAT &&
      wave_format_ex_->wFormatTag != WAVE_FORMAT_PCM &&
      wave_format_ex_->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
    LOG(ERROR) << "Failed to force 16-bit PCM";
    return false;
  }

  if (!AudioCapturer::IsValidSampleRate(wave_format_ex_->nSamplesPerSec)) {
    LOG(ERROR) << "Host sampling rate is neither 44.1 kHz nor 48 kHz. "
               << wave_format_ex_->nSamplesPerSec;
    return false;
  }

  // We support from mono to 7.1. This check should be consistent with
  // AudioPacket::Channels.
  if (wave_format_ex_->nChannels > 8 || wave_format_ex_->nChannels <= 0) {
    LOG(ERROR) << "Unsupported channels " << wave_format_ex_->nChannels;
    return false;
  }

  sampling_rate_ =
      static_cast<AudioPacket::SamplingRate>(wave_format_ex_->nSamplesPerSec);

  wave_format_ex_->wBitsPerSample = kBitsPerSample;
  wave_format_ex_->nBlockAlign = wave_format_ex_->nChannels * kBytesPerSample;
  wave_format_ex_->nAvgBytesPerSec =
      sampling_rate_ * wave_format_ex_->nBlockAlign;

  if (wave_format_ex_->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    PWAVEFORMATEXTENSIBLE wave_format_extensible =
        reinterpret_cast<WAVEFORMATEXTENSIBLE*>(
            static_cast<WAVEFORMATEX*>(wave_format_ex_));
    if (!IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
                     wave_format_extensible->SubFormat) &&
        !IsEqualGUID(KSDATAFORMAT_SUBTYPE_PCM,
                     wave_format_extensible->SubFormat)) {
      LOG(ERROR) << "Failed to force 16-bit samples";
      return false;
    }

    wave_format_extensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    wave_format_extensible->Samples.wValidBitsPerSample = kBitsPerSample;
  } else {
    wave_format_ex_->wFormatTag = WAVE_FORMAT_PCM;
  }

  // Initialize the IAudioClient.
  hr = audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
      (kMaxExpectedTimerLag + audio_device_period_.InMilliseconds()) *
          k100nsPerMillisecond,
      0, wave_format_ex_, nullptr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to initialize IAudioClient. Error " << hr;
    return false;
  }

  // Get an IAudioCaptureClient.
  hr = audio_client_->GetService(IID_PPV_ARGS(&audio_capture_client_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get an IAudioCaptureClient. Error " << hr;
    return false;
  }

  // Start the IAudioClient.
  hr = audio_client_->Start();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to start IAudioClient. Error " << hr;
    return false;
  }

  volume_filter_.ActivateBy(mm_device_.Get());
  volume_filter_.Initialize(sampling_rate_, wave_format_ex_->nChannels);

  return true;
}

bool AudioCapturerWin::is_initialized() const {
  // All Com components should be initialized / deinitialized together.
  return !!audio_client_;
}

void AudioCapturerWin::DoCapture() {
  DCHECK(AudioCapturer::IsValidSampleRate(sampling_rate_));
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!is_initialized() || default_device_detector_->GetAndReset()) {
    if (!ResetAndInitialize()) {
      // Initialization failed, we should wait for next DoCapture call.
      return;
    }
  }

  // Fetch all packets from the audio capture endpoint buffer.
  HRESULT hr = S_OK;
  while (true) {
    UINT32 next_packet_size;
    hr = audio_capture_client_->GetNextPacketSize(&next_packet_size);
    if (FAILED(hr)) {
      break;
    }

    if (next_packet_size <= 0) {
      return;
    }

    BYTE* data;
    UINT32 frames;
    DWORD flags;
    hr = audio_capture_client_->GetBuffer(&data, &frames, &flags, nullptr,
                                          nullptr);
    if (FAILED(hr)) {
      break;
    }

    if (volume_filter_.Apply(reinterpret_cast<int16_t*>(data), frames)) {
      std::unique_ptr<AudioPacket> packet(new AudioPacket());
      packet->add_data(data, frames * wave_format_ex_->nBlockAlign);
      packet->set_encoding(AudioPacket::ENCODING_RAW);
      packet->set_sampling_rate(sampling_rate_);
      packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
      // Only the count of channels is taken into account now, we should also
      // consider dwChannelMask.
      // TODO(zijiehe): Convert dwChannelMask to layout and pass it to
      // AudioPump. So the stream can be downmixed properly with both number and
      // layouts of speakers.
      packet->set_channels(
          static_cast<AudioPacket::Channels>(wave_format_ex_->nChannels));

      callback_.Run(std::move(packet));
    }

    hr = audio_capture_client_->ReleaseBuffer(frames);
    if (FAILED(hr)) {
      break;
    }
  }

  // There is nothing to capture if the audio endpoint device has been unplugged
  // or disabled.
  if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
    return;
  }

  // Avoid reporting the same error multiple times.
  if (FAILED(hr) && hr != last_capture_error_) {
    last_capture_error_ = hr;
    LOG(ERROR) << "Failed to capture an audio packet: 0x" << std::hex << hr
               << std::dec << ".";
  }
}

bool AudioCapturer::IsSupported() {
  return true;
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create() {
  return base::WrapUnique(new AudioCapturerWin());
}

}  // namespace remoting
