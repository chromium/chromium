// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/test_support/fake_iaudio_client.h"

#include <mmdeviceapi.h>
#include <windows.h>

#include <wrl.h>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/audio/win/test_support/fake_iaudio_capture_client.h"

namespace {

constexpr UINT32 kBufferSize = 256;
constexpr REFERENCE_TIME kSamplingPeriodMs = 1;
constexpr REFERENCE_TIME kStreamLatencyMs = 1;

}  // namespace

namespace media {

FakeIAudioClient::FakeIAudioClient(ClientType client_type)
    : client_type_(client_type) {}

FakeIAudioClient::~FakeIAudioClient() = default;

IFACEMETHODIMP FakeIAudioClient::GetBufferSize(UINT32* buffer_size) {
  *buffer_size = kBufferSize;
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::GetCurrentPadding(UINT32* padding) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeIAudioClient::GetDevicePeriod(
    REFERENCE_TIME* default_device_period,
    REFERENCE_TIME* minimum_device_period) {
  // Convert to 100ns units.
  *default_device_period = kSamplingPeriodMs * 10000;
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::GetMixFormat(WAVEFORMATEX** device_format) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeIAudioClient::GetService(REFIID riid, void** service) {
  if (riid == __uuidof(IAudioCaptureClient)) {
    audio_capture_client_ = Microsoft::WRL::Make<FakeIAudioCaptureClient>(
        client_type_, kBufferSize);
    audio_capture_client_.CopyTo(
        reinterpret_cast<IAudioCaptureClient**>(service));
    return S_OK;
  }

  return E_NOTIMPL;
}

IFACEMETHODIMP FakeIAudioClient::GetStreamLatency(REFERENCE_TIME* latency) {
  // Convert to 100ns units.
  *latency = kStreamLatencyMs * 10000;
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::Initialize(AUDCLNT_SHAREMODE share_mode,
                                            DWORD stream_flags,
                                            REFERENCE_TIME buffer_duration,
                                            REFERENCE_TIME periodicity,
                                            const WAVEFORMATEX* format,
                                            LPCGUID audio_session_guid) {
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
  SetEvent(buffer_ready_event_handle_);
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::Start() {
  StartDataStreaming();
  return S_OK;
}

IFACEMETHODIMP FakeIAudioClient::Stop() {
  should_stop_streaming_ = true;
  return S_OK;
}

void FakeIAudioClient::StartDataStreaming() {
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&FakeIAudioClient::StreamData, base::Unretained(this)));
}

void FakeIAudioClient::StreamData() {
  // Realistically, the AudioCaptureClient would need to signal the
  // `FakeIAudioClient` that new data is available. However, for testing
  // purposes, the `FakeIAudioCaptureClient` data buffer's contents never
  // change. Therefore, the `FakeIAudioClient` does not need to receive any type
  // of notification to signal the consumer that it can access the next batch of
  // data.
  if (buffer_ready_event_handle_) {
    SetEvent(buffer_ready_event_handle_);
  }

  if (should_stop_streaming_) {
    return;
  }

  // Simulate new data coming in every `kSamplingPeriodMs` milliseconds.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeIAudioClient::StreamData, base::Unretained(this)),
      base::Milliseconds(kSamplingPeriodMs));
}

}  // namespace media
