// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_IAUDIO_CAPTURE_CLIENT_H_
#define MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_IAUDIO_CAPTURE_CLIENT_H_

#include <audioclient.h>
#include <wrl.h>

#include <vector>

#include "base/win/windows_types.h"
#include "media/audio/win/test_support/fake_iaudio_client.h"

namespace media {

// FakeIAudioCaptureClient is a mock implementation of the IAudioCaptureClient
// interface. It is used for testing purposes and simulates the behavior of an
// IAudioCaptureClient without requiring actual audio hardware or a real
// WASAPI environment.
class FakeIAudioCaptureClient
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::FtmBase,
          IAudioCaptureClient> {
 public:
  FakeIAudioCaptureClient(FakeIAudioClient::ClientType client_type,
                          UINT32 buffer_size_frames,
                          WORD frame_size_bytes);
  FakeIAudioCaptureClient(const FakeIAudioCaptureClient&) = delete;
  FakeIAudioCaptureClient& operator=(const FakeIAudioCaptureClient&) = delete;
  ~FakeIAudioCaptureClient() override;

  // IAudioCaptureClient methods
  IFACEMETHODIMP(GetBuffer)(BYTE** data,
                            UINT32* num_frames_available,
                            DWORD* flags,
                            UINT64* device_position,
                            UINT64* qpc_position) override;
  IFACEMETHODIMP(ReleaseBuffer)(UINT32 num_frames_read) override;
  IFACEMETHODIMP(GetNextPacketSize)(UINT32* packet_size) override;

 private:
  const FakeIAudioClient::ClientType client_type_;
  const UINT32 packet_size_frames_;
  // Simulated buffer data
  std::vector<BYTE> buffer_data_;
  UINT64 device_position_ = 0;
  UINT64 qpc_position_ = 0;
  int next_packet_size_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_IAUDIO_CAPTURE_CLIENT_H_
