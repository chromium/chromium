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

// TOOD(crbug.com/418678875): make this configurable.
constexpr UINT32 kBufferSizeFrames = 256;
constexpr REFERENCE_TIME kSamplingPeriodMs = 1;
constexpr REFERENCE_TIME kStreamLatencyMs = 1;

}  // namespace

namespace media {

FakeIAudioClient::DataStreamer::DataStreamer(HANDLE buffer_ready_event_handle)
    : buffer_ready_event_handle_(buffer_ready_event_handle) {
  // Stream once immediately.
  StreamData();
  // And again every kSamplingPeriodMs milliseconds.
  timer_.Start(FROM_HERE, base::Milliseconds(kSamplingPeriodMs),
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
  // TOOD(crbug.com/418678875): Real devices often have a buffer size that is a
  // multiple of 10ms and depends on the sample rate. Consider making this
  // configurable and dependent on the sample rate.
  *buffer_size = kBufferSizeFrames;
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
  if (frame_size_bytes_ == 0) {
    return AUDCLNT_E_NOT_INITIALIZED;
  }

  if (riid == __uuidof(IAudioCaptureClient)) {
    audio_capture_client_ = Microsoft::WRL::Make<FakeIAudioCaptureClient>(
        client_type_, kBufferSizeFrames, frame_size_bytes_);
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
  // TOOD(crbug.com/418678875): validate the format.
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
