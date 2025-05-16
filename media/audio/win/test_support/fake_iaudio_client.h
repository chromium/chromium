// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_IAUDIO_CLIENT_H_
#define MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_IAUDIO_CLIENT_H_

#include <mmdeviceapi.h>
#include <windows.h>

#include <audioclient.h>
#include <wrl.h>

#include "base/task/sequenced_task_runner.h"

namespace media {

// FakeIAudioClient is a mock implementation of the IAudioClient interface.
// It is used for testing purposes and simulates the behavior of an IAudioClient
// without requiring actual audio hardware or a real WASAPI environment.
class FakeIAudioClient
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::FtmBase,
          IAudioClient> {
 public:
  enum class ClientType { kDefaultDevice, kApplicationLoopbackDevice };

  FakeIAudioClient(ClientType client_type);
  FakeIAudioClient(const FakeIAudioClient&) = delete;
  FakeIAudioClient& operator=(const FakeIAudioClient&) = delete;
  ~FakeIAudioClient() override;

  // IAudioClient methods
  IFACEMETHODIMP(GetBufferSize)(UINT32* buffer_size) override;
  IFACEMETHODIMP(GetCurrentPadding)(UINT32* padding) override;
  IFACEMETHODIMP(GetDevicePeriod)(
      REFERENCE_TIME* default_device_period,
      REFERENCE_TIME* minimum_device_period) override;
  IFACEMETHODIMP(GetMixFormat)(WAVEFORMATEX** device_format) override;
  IFACEMETHODIMP(GetService)(REFIID riid, void** service) override;
  IFACEMETHODIMP(GetStreamLatency)(REFERENCE_TIME* phnsLatency) override;
  IFACEMETHODIMP(Initialize)(AUDCLNT_SHAREMODE share_mode,
                             DWORD stream_flags,
                             REFERENCE_TIME buffer_duration,
                             REFERENCE_TIME periodicity,
                             const WAVEFORMATEX* format,
                             LPCGUID audio_session_guid) override;
  IFACEMETHODIMP(IsFormatSupported)(AUDCLNT_SHAREMODE share_mode,
                                    const WAVEFORMATEX* format,
                                    WAVEFORMATEX** closest_match) override;
  IFACEMETHODIMP(Reset)() override;
  IFACEMETHODIMP(SetEventHandle)(HANDLE event_handle) override;
  IFACEMETHODIMP(Start)() override;
  IFACEMETHODIMP(Stop)() override;

 private:
  void StartDataStreaming();
  void StreamData();

  const ClientType client_type_;
  HANDLE buffer_ready_event_handle_ = nullptr;
  Microsoft::WRL::ComPtr<IAudioCaptureClient> audio_capture_client_ = nullptr;
  bool should_stop_streaming_ = false;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_IAUDIO_CLIENT_H_
