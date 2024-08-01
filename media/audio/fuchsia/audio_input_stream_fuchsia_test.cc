// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/fuchsia/audio_input_stream_fuchsia.h"

#include <fuchsia/media/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/fuchsia/audio/fake_audio_capturer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr size_t kFramesPerPacket = 480;

class TestCaptureCallback final : public AudioInputStream::AudioInputCallback {
 public:
  TestCaptureCallback() = default;
  ~TestCaptureCallback() override = default;

  TestCaptureCallback(const TestCaptureCallback&) = delete;
  TestCaptureCallback& operator=(const TestCaptureCallback&) = delete;

  bool have_error() const { return have_error_; }

  const std::vector<std::unique_ptr<AudioBus>>& packets() const {
    return packets_;
  }

  // AudioCapturerSource::CaptureCallback implementation.
  void OnData(const AudioBus* source,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    auto bus = AudioBus::Create(source->channels(), source->frames());
    source->CopyTo(bus.get());
    packets_.push_back(std::move(bus));
  }

  void OnError() override {
    EXPECT_FALSE(have_error_);
    have_error_ = true;
  }

 private:
  std::vector<std::unique_ptr<AudioBus>> packets_;
  bool have_error_ = false;
};

}  // namespace

class AudioInputStreamFuchsiaTest : public testing::Test {
 public:
  AudioInputStreamFuchsiaTest() {}

  ~AudioInputStreamFuchsiaTest() override {
    if (input_stream_) {
      input_stream_->Stop();
      input_stream_.reset();
    }

    base::RunLoop().RunUntilIdle();
  }

  void InitializeCapturer(ChannelLayoutConfig channel_layout_config) {
    base::TestComponentContextForProcess test_context;
    FakeAudioCapturerFactory audio_capturer_factory(
        test_context.additional_services());

    input_stream_ = std::make_unique<AudioInputStreamFuchsia>(
        /*manager=*/nullptr,
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        channel_layout_config,
                        /*sample_rate=*/48000, kFramesPerPacket),
        AudioDeviceDescription::kDefaultDeviceId);

    AudioInputStream::OpenOutcome result = input_stream_->Open();
    EXPECT_EQ(result, AudioInputStream::OpenOutcome::kSuccess);

    base::RunLoop().RunUntilIdle();

    test_capturer_ = audio_capturer_factory.TakeCapturer();
    ASSERT_TRUE(test_capturer_);
    test_capturer_->SetDataGeneration(
        FakeAudioCapturer::DataGeneration::MANUAL);

    // Verify no other capturers were created.
    ASSERT_FALSE(audio_capturer_factory.TakeCapturer());
  }

  void TestCapture(ChannelLayoutConfig channel_layout_config) {
    InitializeCapturer(channel_layout_config);
    input_stream_->Start(&callback_);
    base::RunLoop().RunUntilIdle();

    size_t num_channels = channel_layout_config.channels();

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

  std::unique_ptr<FakeAudioCapturer> test_capturer_;
  TestCaptureCallback callback_;
  std::unique_ptr<AudioInputStreamFuchsia> input_stream_;
};

TEST_F(AudioInputStreamFuchsiaTest, CreateAndDestroy) {}

TEST_F(AudioInputStreamFuchsiaTest, InitializeAndDestroy) {
  InitializeCapturer(ChannelLayoutConfig::Mono());
}

TEST_F(AudioInputStreamFuchsiaTest, InitializeAndStart) {
  const auto kChannelLayoutConfig = ChannelLayoutConfig::Mono();
  const auto kNumChannels = kChannelLayoutConfig.channels();

  InitializeCapturer(kChannelLayoutConfig);
  input_stream_->Start(&callback_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_capturer_->is_active());
  EXPECT_EQ(test_capturer_->GetPacketSize(),
            sizeof(float) * kFramesPerPacket * kNumChannels);

  EXPECT_TRUE(callback_.packets().empty());
}

TEST_F(AudioInputStreamFuchsiaTest, InitializeStereo) {
  const auto kChannelLayoutConfig = ChannelLayoutConfig::Stereo();
  const auto kNumChannels = kChannelLayoutConfig.channels();

  InitializeCapturer(kChannelLayoutConfig);
  input_stream_->Start(&callback_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_capturer_->is_active());
  EXPECT_EQ(test_capturer_->GetPacketSize(),
            sizeof(float) * kNumChannels * kFramesPerPacket);
}

TEST_F(AudioInputStreamFuchsiaTest, StartAndStop) {
  InitializeCapturer(ChannelLayoutConfig::Stereo());
  input_stream_->Start(&callback_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_capturer_->is_active());

  input_stream_->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioInputStreamFuchsiaTest, CaptureMono) {
  TestCapture(ChannelLayoutConfig::Mono());
}

TEST_F(AudioInputStreamFuchsiaTest, CaptureStereo) {
  TestCapture(ChannelLayoutConfig::Stereo());
}

TEST_F(AudioInputStreamFuchsiaTest, CaptureTwoPackets) {
  InitializeCapturer(ChannelLayoutConfig::Mono());
  input_stream_->Start(&callback_);
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

TEST_F(AudioInputStreamFuchsiaTest, CaptureAfterStop) {
  InitializeCapturer(ChannelLayoutConfig::Mono());
  input_stream_->Start(&callback_);
  base::RunLoop().RunUntilIdle();
  input_stream_->Stop();

  std::vector<float> samples(kFramesPerPacket);
  base::TimeTicks ts = base::TimeTicks::FromZxTime(100);
  test_capturer_->SendData(ts, samples.data());
  base::RunLoop().RunUntilIdle();

  // Packets produced after Stop() should not be passed to the callback.
  ASSERT_EQ(callback_.packets().size(), 0U);
}

}  // namespace media
