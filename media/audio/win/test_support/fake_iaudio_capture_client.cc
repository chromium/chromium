// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/test_support/fake_iaudio_capture_client.h"

#include <wrl.h>

namespace media {

FakeIAudioCaptureClient::FakeIAudioCaptureClient(
    FakeIAudioClient::ClientType client_type,
    UINT32 buffer_size_frames,
    WORD frame_size_bytes)
    : client_type_(client_type),
      packet_size_frames_(buffer_size_frames),
      buffer_data_(buffer_size_frames * frame_size_bytes, /*initial_value=*/1),
      next_packet_size_(buffer_size_frames) {}

FakeIAudioCaptureClient::~FakeIAudioCaptureClient() = default;

IFACEMETHODIMP FakeIAudioCaptureClient::GetBuffer(BYTE** data,
                                                  UINT32* num_frames_available,
                                                  DWORD* flags,
                                                  UINT64* device_position,
                                                  UINT64* qpc_position) {
  *data = buffer_data_.data();
  *num_frames_available = packet_size_frames_;
  *flags = 0;
  *device_position = device_position_;
  *qpc_position = qpc_position_;

  // Simulate a device position increment for the next call.
  if (client_type_ != FakeIAudioClient::ClientType::kProcessLoopbackDevice) {
    // Process loopback uses a virtual device, which do not have a device
    // position and should always return 0.
    device_position_ += *num_frames_available;
  }
  qpc_position_ += *num_frames_available;

  // After the buffer content has been read, make the next call to
  // `GetNextPacketSize` return 0. This simulates the behavior of the audio
  // engine when there is no more data available, which will make the
  // `WASAPIAudioInputStream` to break the loop in
  // `WASAPIAudioInputStream::PullCaptureDataAndPushToSink`.
  next_packet_size_ = 0;
  return S_OK;
}

IFACEMETHODIMP FakeIAudioCaptureClient::ReleaseBuffer(UINT32 num_frames_read) {
  return S_OK;
}

IFACEMETHODIMP FakeIAudioCaptureClient::GetNextPacketSize(UINT32* packet_size) {
  *packet_size = next_packet_size_;
  if (next_packet_size_ == 0) {
    next_packet_size_ = packet_size_frames_;
  }
  return S_OK;
}

}  // namespace media
