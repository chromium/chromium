// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/test_support/fake_iaudio_client.h"

#include <mmdeviceapi.h>
#include <windows.h>

#include <wrl.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/audio/win/test_support/fake_iaudio_capture_client.h"

namespace {

// The audio engine typically processes events at a rate of 100Hz.
constexpr base::TimeDelta kSamplingPeriodMs = base::Milliseconds(10);
// Maximum latency for the current stream.
constexpr base::TimeDelta kStreamLatencyMs = 10 * kSamplingPeriodMs;

}  // namespace

namespace media {

FakeIAudioClient::DataStreamer::DataStreamer(HANDLE buffer_ready_event_handle)
    : buffer_ready_event_handle_(buffer_ready_event_handle) {
  // Stream once immediately.
  StreamData();
  // And again every kSamplingPeriodMs milliseconds.
  timer_.Start(FROM_HERE, kSamplingPeriodMs,
               base::BindRepeating(&FakeIAudioClient::DataStreamer::StreamData,
                                   base::Unretained(this)));
}

FakeIAudioClient::DataStreamer::~DataStreamer() = default;

void FakeIAudioClient::DataStreamer::Stop(base::WaitableEvent& stop_event) {
  timer_.Stop();
  stop_event.Signal();
}

void FakeIAudioClient::DataStreamer::StreamData() {
  if (buffer_ready_event_handle_) {
    ::SetEvent(buffer_ready_event_handle_);
  }
}

FakeIAudioClient::FakeIAudioClient(ClientType client_type)
    : client_type_(client_type) {}

FakeIAudioClient::~FakeIAudioClient() = default;

IFACEMETHODIMP FakeIAudioClient::GetBufferSize(UINT32* buffer_size) {
  *buffer_size = buffer_size_frames_;
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::GetCurrentPadding(UINT32* padding) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeIAudioClient::GetDevicePeriod(
    REFERENCE_TIME* default_device_period,
    REFERENCE_TIME* minimum_device_period) {
  const REFERENCE_TIME period_in_100ns_units =
      kSamplingPeriodMs.InMicroseconds() * 10;
  *default_device_period = period_in_100ns_units;
  *minimum_device_period = period_in_100ns_units;
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::GetMixFormat(WAVEFORMATEX** device_format) {
  // For testing purposes, we default to Signed 16-bit integer.
  WAVEFORMATEXTENSIBLE* pFormat =
      (WAVEFORMATEXTENSIBLE*)CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE));
  pFormat->Format.wFormatTag = WAVE_FORMAT_PCM;
  pFormat->Format.nChannels = 2;
  pFormat->Format.nSamplesPerSec = 44100;
  pFormat->Format.wBitsPerSample = 16;
  pFormat->Format.nBlockAlign =
      (pFormat->Format.nChannels * pFormat->Format.wBitsPerSample) / 8;
  pFormat->Format.nAvgBytesPerSec =
      pFormat->Format.nSamplesPerSec * pFormat->Format.nBlockAlign;
  pFormat->Format.cbSize = 22;

  pFormat->Samples.wValidBitsPerSample = 16;
  pFormat->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
  pFormat->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  *device_format = (WAVEFORMATEX*)pFormat;

  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::GetService(REFIID riid, void** service) {
  if (frame_size_bytes_ == 0) {
    return AUDCLNT_E_NOT_INITIALIZED;
  }

  if (riid == __uuidof(IAudioCaptureClient)) {
    audio_capture_client_ = Microsoft::WRL::Make<FakeIAudioCaptureClient>(
        client_type_, buffer_size_frames_, frame_size_bytes_);
    audio_capture_client_.CopyTo(
        reinterpret_cast<IAudioCaptureClient**>(service));
    return S_OK;
  }

  return E_NOTIMPL;
}

IFACEMETHODIMP FakeIAudioClient::GetStreamLatency(REFERENCE_TIME* latency) {
  const REFERENCE_TIME latency_in_100ns_units =
      kStreamLatencyMs.InMicroseconds() * 10;
  *latency = latency_in_100ns_units;
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::Initialize(AUDCLNT_SHAREMODE share_mode,
                                            DWORD stream_flags,
                                            REFERENCE_TIME buffer_duration,
                                            REFERENCE_TIME periodicity,
                                            const WAVEFORMATEX* format,
                                            LPCGUID audio_session_guid) {
  VLOG(1) << CoreAudioUtil::WaveFormatToString(
      const_cast<WAVEFORMATEX*>(format));
  buffer_size_frames_ = static_cast<UINT32>(
      (format->nSamplesPerSec * kSamplingPeriodMs.InMicroseconds()) /
      base::Time::kMicrosecondsPerSecond);
  frame_size_bytes_ = (format->wBitsPerSample / 8) * format->nChannels;
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::IsFormatSupported(
    AUDCLNT_SHAREMODE share_mode,
    const WAVEFORMATEX* format,
    WAVEFORMATEX** closest_match) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeIAudioClient::Reset() {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeIAudioClient::SetEventHandle(HANDLE event_handle) {
  buffer_ready_event_handle_ = event_handle;
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::Start() {
  if (!streamer_.is_null()) {
    return S_OK;  // Already running.
  }

  // Start the streamer, giving it the client's event to be signaled.
  streamer_.emplace(base::ThreadPool::CreateSequencedTaskRunner({}),
                    buffer_ready_event_handle_);
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::Stop() {
  if (streamer_.is_null()) {
    return S_OK;  // Not running.
  }

  // Stop the streamer and wait for it to acknowledge.
  base::WaitableEvent stop_event;
  streamer_.AsyncCall(&DataStreamer::Stop).WithArgs(std::ref(stop_event));
  stop_event.Wait();

  // Destroy the streamer.
  streamer_.Reset();

  return S_OK;
}

}  // namespace media
