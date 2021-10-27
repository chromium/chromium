// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fuchsia_audio_capturer_source.h"

#include <fuchsia/media/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/channel_layout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr size_t kFramesPerPacket = 480;
constexpr uint32_t kBufferId = 0;

class TestAudioCapturer final
    : public fuchsia::media::testing::AudioCapturer_TestBase {
 public:
  TestAudioCapturer(
      fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request)
      : binding_(this, std::move(request)) {}

  ~TestAudioCapturer() override = default;

  TestAudioCapturer(const TestAudioCapturer&) = delete;
  TestAudioCapturer& operator=(const TestAudioCapturer&) = delete;

  bool is_active() const { return is_active_; }

  // Size of a single packet in bytes.
  size_t packet_size() const {
    return frames_per_packet_ * stream_type_->channels * sizeof(float);
  }

  void SendData(base::TimeTicks timestamp, void* data) {
    CHECK(buffer_vmo_);
    CHECK(is_active_);

    // Find unused packet.
    auto it = std::find(packets_usage_.begin(), packets_usage_.end(), false);

    // Currently tests don't try to send more than 2 packets and the buffer
    // always will have space for at least 2 packets.
    CHECK(it != packets_usage_.end());

    size_t buffer_index = it - packets_usage_.begin();
    size_t buffer_pos = buffer_index * packet_size();

    packets_usage_[buffer_index] = true;

    // Write data to the shared VMO.
    zx_status_t status = buffer_vmo_.write(data, buffer_pos, packet_size());
    ZX_CHECK(status == ZX_OK, status);

    // Send the new packet.
    fuchsia::media::StreamPacket packet;
    packet.payload_buffer_id = kBufferId;
    packet.pts = timestamp.ToZxTime();
    packet.payload_offset = buffer_pos;
    packet.payload_size = packet_size();
    binding_.events().OnPacketProduced(std::move(packet));
  }

  // fuchsia::media::AudioCapturer implementation.
  void SetPcmStreamType(fuchsia::media::AudioStreamType stream_type) override {
    ASSERT_FALSE(stream_type_.has_value());
    ASSERT_EQ(stream_type.sample_format,
              fuchsia::media::AudioSampleFormat::FLOAT);

    stream_type_ = std::move(stream_type);
  }

  void AddPayloadBuffer(uint32_t id, zx::vmo payload_buffer) override {
    ASSERT_EQ(id, kBufferId);
    ASSERT_FALSE(buffer_vmo_);
    ASSERT_TRUE(stream_type_.has_value());

    buffer_vmo_ = std::move(payload_buffer);
    zx_status_t status = buffer_vmo_.get_size(&buffer_size_);
    ZX_CHECK(status == ZX_OK, status);
  }

  void StartAsyncCapture(uint32_t frames_per_packet) override {
    ASSERT_TRUE(buffer_vmo_);
    ASSERT_FALSE(is_active_);

    is_active_ = true;
    frames_per_packet_ = frames_per_packet;
    size_t num_packets = buffer_size_ / packet_size();

    // AudioCapturer protocol requires that we can fit at least 2 packets in the
    // buffer in async mode.
    ASSERT_GE(num_packets, 2U);

    packets_usage_.clear();
    packets_usage_.resize(num_packets, false);
  }

  void ReleasePacket(fuchsia::media::StreamPacket packet) override {
    ASSERT_EQ(packet.payload_buffer_id, kBufferId);
    ASSERT_EQ(packet.payload_offset % packet_size(), 0U);
    size_t buffer_index = packet.payload_offset / packet_size();
    ASSERT_LT(buffer_index, packets_usage_.size());
    ASSERT_TRUE(packets_usage_[buffer_index]);
    packets_usage_[buffer_index] = false;
  }

  // No other methods are expected to be called.
  void NotImplemented_(const std::string& name) override {
    FAIL() << ": " << name;
  }

 private:
  fidl::Binding<fuchsia::media::AudioCapturer> binding_;

  zx::vmo buffer_vmo_;
  uint64_t buffer_size_ = 0;
  absl::optional<fuchsia::media::AudioStreamType> stream_type_;
  bool is_active_ = false;
  size_t frames_per_packet_ = 0;
  std::vector<bool> packets_usage_;
};

class TestCaptureCallback final : public AudioCapturerSource::CaptureCallback {
 public:
  TestCaptureCallback() = default;
  ~TestCaptureCallback() override = default;

  TestCaptureCallback(const TestCaptureCallback&) = delete;
  TestCaptureCallback& operator=(const TestCaptureCallback&) = delete;

  bool is_started() const { return is_started_; }
  bool have_error() const { return have_error_; }

  const std::vector<std::unique_ptr<AudioBus>>& packets() const {
    return packets_;
  }

  // AudioCapturerSource::CaptureCallback implementation.
  void OnCaptureStarted() override { is_started_ = true; }

  void Capture(const AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) override {
    EXPECT_TRUE(is_started_);
    auto bus =
        AudioBus::Create(audio_source->channels(), audio_source->frames());
    audio_source->CopyTo(bus.get());
    packets_.push_back(std::move(bus));
  }

  void OnCaptureError(AudioCapturerSource::ErrorCode code,
                      const std::string& message) override {
    EXPECT_FALSE(have_error_);
    have_error_ = true;
  }

  void OnCaptureMuted(bool is_muted) override { FAIL(); }

  void OnCaptureProcessorCreated(AudioProcessorControls* controls) override {
    FAIL();
  }

 private:
  std::vector<std::unique_ptr<AudioBus>> packets_;
  bool is_started_ = false;
  bool have_error_ = false;
};

}  // namespace

class FuchsiaAudioCapturerSourceTest : public testing::Test {
 public:
  FuchsiaAudioCapturerSourceTest() {
    fidl::InterfaceHandle<fuchsia::media::AudioCapturer> capturer_handle;
    test_capturer_ =
        std::make_unique<TestAudioCapturer>(capturer_handle.NewRequest());
    capturer_source_ = base::MakeRefCounted<FuchsiaAudioCapturerSource>(
        std::move(capturer_handle), base::ThreadTaskRunnerHandle::Get());
  }

  ~FuchsiaAudioCapturerSourceTest() override {
    capturer_source_->Stop();
    capturer_source_ = nullptr;

    base::RunLoop().RunUntilIdle();
  }

  void InitializeCapturer(ChannelLayout layout) {
    capturer_source_->Initialize(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, layout,
                        /*sample_rate=*/48000, kFramesPerPacket),
        &callback_);
  }

  void TestCapture(ChannelLayout layout) {
    InitializeCapturer(layout);
    capturer_source_->Start();
    base::RunLoop().RunUntilIdle();

    size_t num_channels = ChannelLayoutToChannelCount(layout);

    // Produce a packet.
    std::vector<float> samples(kFramesPerPacket * num_channels);
    for (size_t i = 0; i < samples.size(); ++i) {
      samples[i] = i;
    }

    base::TimeTicks ts = base::TimeTicks::FromZxTime(100);
    test_capturer_->SendData(ts, samples.data());
    base::RunLoop().RunUntilIdle();

    // Verify that the packet was received.
    ASSERT_EQ(callback_.packets().size(), 1U);
    ASSERT_EQ(callback_.packets()[0]->frames(),
              static_cast<int>(kFramesPerPacket));
    for (size_t i = 0; i < samples.size(); ++i) {
      EXPECT_EQ(samples[i], callback_.packets()[0]->channel(
                                i % num_channels)[i / num_channels]);
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<TestAudioCapturer> test_capturer_;
  TestCaptureCallback callback_;
  scoped_refptr<FuchsiaAudioCapturerSource> capturer_source_;
};

TEST_F(FuchsiaAudioCapturerSourceTest, CreateAndDestroy) {}

TEST_F(FuchsiaAudioCapturerSourceTest, InitializeAndDestroy) {
  InitializeCapturer(CHANNEL_LAYOUT_MONO);
}

TEST_F(FuchsiaAudioCapturerSourceTest, InitializeAndStart) {
  const auto kLayout = CHANNEL_LAYOUT_MONO;
  const auto kNumChannels = ChannelLayoutToChannelCount(kLayout);

  InitializeCapturer(kLayout);
  capturer_source_->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_capturer_->is_active());
  EXPECT_EQ(test_capturer_->packet_size(),
            sizeof(float) * kFramesPerPacket * kNumChannels);

  EXPECT_TRUE(callback_.is_started());
  EXPECT_TRUE(callback_.packets().empty());
}

TEST_F(FuchsiaAudioCapturerSourceTest, InitializeStereo) {
  const auto kLayout = CHANNEL_LAYOUT_STEREO;
  const auto kNumChannels = ChannelLayoutToChannelCount(kLayout);

  InitializeCapturer(kLayout);
  capturer_source_->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_capturer_->is_active());
  EXPECT_EQ(test_capturer_->packet_size(),
            sizeof(float) * kNumChannels * kFramesPerPacket);
}

TEST_F(FuchsiaAudioCapturerSourceTest, StartAndStop) {
  InitializeCapturer(CHANNEL_LAYOUT_MONO);
  capturer_source_->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_capturer_->is_active());

  capturer_source_->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(FuchsiaAudioCapturerSourceTest, CaptureMono) {
  TestCapture(CHANNEL_LAYOUT_MONO);
}

TEST_F(FuchsiaAudioCapturerSourceTest, CaptureStereo) {
  TestCapture(CHANNEL_LAYOUT_STEREO);
}

TEST_F(FuchsiaAudioCapturerSourceTest, CaptureTwoPackets) {
  InitializeCapturer(CHANNEL_LAYOUT_MONO);
  capturer_source_->Start();
  base::RunLoop().RunUntilIdle();

  // Produce two packets.
  std::vector<float> samples1(kFramesPerPacket);
  std::vector<float> samples2(kFramesPerPacket);
  for (size_t i = 0; i < kFramesPerPacket; ++i) {
    samples1[i] = i;
    samples2[i] = i + 0.2;
  }

  base::TimeTicks ts = base::TimeTicks::FromZxTime(100);
  test_capturer_->SendData(ts, samples1.data());
  test_capturer_->SendData(ts + base::Milliseconds(10), samples2.data());
  base::RunLoop().RunUntilIdle();

  // Verify that both packets were received.
  ASSERT_EQ(callback_.packets().size(), 2U);
  ASSERT_EQ(callback_.packets()[0]->frames(),
            static_cast<int>(kFramesPerPacket));
  ASSERT_EQ(callback_.packets()[1]->frames(),
            static_cast<int>(kFramesPerPacket));
  for (size_t i = 0; i < kFramesPerPacket; ++i) {
    EXPECT_EQ(samples1[i], callback_.packets()[0]->channel(0)[i]);
    EXPECT_EQ(samples2[i], callback_.packets()[1]->channel(0)[i]);
  }
}

TEST_F(FuchsiaAudioCapturerSourceTest, CaptureAfterStop) {
  InitializeCapturer(CHANNEL_LAYOUT_MONO);
  capturer_source_->Start();
  base::RunLoop().RunUntilIdle();
  capturer_source_->Stop();

  std::vector<float> samples(kFramesPerPacket);
  base::TimeTicks ts = base::TimeTicks::FromZxTime(100);
  test_capturer_->SendData(ts, samples.data());
  base::RunLoop().RunUntilIdle();

  // Packets produced after Stop() should not be passed to the callback.
  ASSERT_EQ(callback_.packets().size(), 0U);
}

}  // namespace media
