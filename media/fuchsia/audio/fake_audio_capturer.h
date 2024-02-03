// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_CAPTURER_H_
#define MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_CAPTURER_H_

#include <fuchsia/media/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include <list>
#include <optional>
#include <vector>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace media {

// Fake implementation of fuchsia::media::testing::AudioCapturer interface. Used
// for tests.
class FakeAudioCapturer final
    : public fuchsia::media::testing::AudioCapturer_TestBase {
 public:
  // Specify whether the fake implementation should generate data periodically,
  // or whether the test will control the sent data using |SendData()|.
  enum class DataGeneration {
    AUTOMATIC,
    MANUAL,
  };

  // The buffer id used when sending data.
  static constexpr uint32_t kBufferId = 0;

  explicit FakeAudioCapturer(
      fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request = {});
  ~FakeAudioCapturer() override;

  FakeAudioCapturer(const FakeAudioCapturer&) = delete;
  FakeAudioCapturer& operator=(const FakeAudioCapturer&) = delete;

  void Bind(fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request);

  bool is_active() const { return is_active_; }

  // Size of a single packet in bytes.
  size_t GetPacketSize() const;

  void SetDataGeneration(DataGeneration data_generation);

  // Send the given data to the client. |data| length must be |GetPacketSize()|.
  void SendData(base::TimeTicks timestamp, void* data);

  // fuchsia::media::AudioCapturer implementation.
  void SetPcmStreamType(fuchsia::media::AudioStreamType stream_type) override;
  void AddPayloadBuffer(uint32_t id, zx::vmo payload_buffer) override;
  void StartAsyncCapture(uint32_t frames_per_packet) override;
  void StopAsyncCaptureNoReply() override;
  void ReleasePacket(fuchsia::media::StreamPacket packet) override;

  // No other methods are expected to be called.
  void NotImplemented_(const std::string& name) override;

 private:
  void ProducePackets();

  DataGeneration data_generation_ = DataGeneration::AUTOMATIC;
  fidl::Binding<fuchsia::media::AudioCapturer> binding_;

  zx::vmo buffer_vmo_;
  uint64_t buffer_size_ = 0;
  std::optional<fuchsia::media::AudioStreamType> stream_type_;
  bool is_active_ = false;
  size_t frames_per_packet_ = 0;
  std::vector<bool> packets_usage_;

  base::TimeTicks start_timestamp_;
  size_t packet_index_ = 0;
  base::OneShotTimer timer_;
};

class FakeAudioCapturerFactory final
    : public fuchsia::media::testing::Audio_TestBase {
 public:
  explicit FakeAudioCapturerFactory(sys::OutgoingDirectory* outgoing_directory);
  ~FakeAudioCapturerFactory() override;

  // Returns a capturer created by this class. Returns |nullptr| if there are no
  // new instances.
  std::unique_ptr<FakeAudioCapturer> TakeCapturer();

  // fuchsia::media::Audio overrides.
  void CreateAudioCapturer(
      fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request,
      bool loopback) override;

  // No other methods are expected to be called.
  void NotImplemented_(const std::string& name) override;

 private:
  base::ScopedServiceBinding<fuchsia::media::Audio> binding_;
  std::list<std::unique_ptr<FakeAudioCapturer>> capturers_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_CAPTURER_H_
