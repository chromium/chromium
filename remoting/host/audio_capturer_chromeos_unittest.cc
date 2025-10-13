// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_chromeos.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromeos/audio_helper_chromeos.h"
#include "remoting/proto/audio.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;

namespace remoting {

class FakeAudioHelperChromeOs : public AudioHelperChromeOs {
 public:
  FakeAudioHelperChromeOs() = default;
  ~FakeAudioHelperChromeOs() override {
    if (destruction_callback_) {
      std::move(destruction_callback_).Run();
    }
  }

  FakeAudioHelperChromeOs(const FakeAudioHelperChromeOs&) = delete;
  FakeAudioHelperChromeOs& operator=(const FakeAudioHelperChromeOs&) = delete;

  void StartAudioStream(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      OnDataCallback on_data_callback,
      OnErrorCallback on_error_callback) override {
    on_data_callback_ = std::move(on_data_callback);
    on_error_callback_ = std::move(on_error_callback);
    audio_stream_started_ = true;
  }

  void StopAudioStream() override { ++stop_audio_stream_call_count_; }

  void SimulateData(std::unique_ptr<AudioPacket> packet) {
    on_data_callback_.Run(std::move(packet));
  }

  void SimulateError() { on_error_callback_.Run(); }

  void SetDestructionCallback(base::OnceClosure callback) {
    destruction_callback_ = std::move(callback);
  }

  int stop_audio_stream_call_count() { return stop_audio_stream_call_count_; }

  bool audio_stream_started() { return audio_stream_started_; }

 private:
  OnDataCallback on_data_callback_;
  OnErrorCallback on_error_callback_;
  int stop_audio_stream_call_count_ = 0;
  bool audio_stream_started_ = false;
  base::OnceClosure destruction_callback_;
};

class AudioCapturerChromeOsTest : public testing::Test {
 public:
  AudioCapturerChromeOsTest() {}
  ~AudioCapturerChromeOsTest() override = default;

  void SetUp() override {
    // Create a fake AudioManager to hold the TestAudioThread when
    // AudioCapturerChromeOs calls
    // `media::AudioManager::Get()->GetTaskRunner()`.
    audio_manager_ = media::AudioManager::CreateForTesting(
        std::make_unique<media::TestAudioThread>());

    std::unique_ptr<FakeAudioHelperChromeOs> fake_audio_helper_chromeos =
        std::make_unique<FakeAudioHelperChromeOs>();
    audio_helper_chromeos_ = fake_audio_helper_chromeos.get();
    audio_capturer_ = std::make_unique<AudioCapturerChromeOs>(
        std::move(fake_audio_helper_chromeos));
  }

  void TearDown() override {
    // Ensure the FakeAudioHelperChromeOs instance is fully destroyed on the
    // TestAudioThread before shutting down the `audio_manager_`.
    bool helper_destroyed = false;
    audio_helper_chromeos_->SetDestructionCallback(
        base::BindOnce([](bool* helper_destroyed) { *helper_destroyed = true; },
                       base::Unretained(&helper_destroyed)));

    // Release the helper's raw_ptr before it's destroyed by the resetting of
    // the capturer.
    audio_helper_chromeos_ = nullptr;

    // Destroy the capturer.
    audio_capturer_.reset();

    // Allow the destruction of the helper task to complete on the
    // TestAudioThread.
    EXPECT_TRUE(base::test::RunUntil([&]() { return helper_destroyed; }));

    audio_manager_->Shutdown();
  }

  void OnAudioPacketCaptured(std::unique_ptr<AudioPacket> packet) {
    captured_audio_packets_.push_back(std::move(packet));
  }

  base::test::TaskEnvironment task_environment_;

 protected:
  std::unique_ptr<AudioCapturerChromeOs> audio_capturer_;
  raw_ptr<FakeAudioHelperChromeOs> audio_helper_chromeos_;
  int captured_packet_count_ = 0;
  std::vector<std::unique_ptr<AudioPacket>> captured_audio_packets_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<media::AudioManager> audio_manager_;
};

TEST_F(AudioCapturerChromeOsTest, SimulateAudioPacket) {
  audio_capturer_->Start(
      base::BindRepeating(&AudioCapturerChromeOsTest::OnAudioPacketCaptured,
                          base::Unretained(this)));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return audio_helper_chromeos_->audio_stream_started(); }));

  std::unique_ptr<AudioPacket> expected_packet =
      std::make_unique<AudioPacket>();
  expected_packet->set_timestamp(123);
  expected_packet->set_encoding(remoting::AudioPacket::ENCODING_RAW);
  expected_packet->set_sampling_rate(
      remoting::AudioPacket::SAMPLING_RATE_48000);
  expected_packet->set_bytes_per_sample(
      remoting::AudioPacket::BYTES_PER_SAMPLE_2);
  expected_packet->set_channels(remoting::AudioPacket::CHANNELS_STEREO);
  // Create a buffer of silent (zero) audio data.
  std::string audio_data(1024, 0);
  expected_packet->add_data(std::move(audio_data));

  // Send the audio packet to AudioCapturerChromeOs and expect it to send it
  // to the PacketCapturedCallback.
  audio_helper_chromeos_->SimulateData(
      std::make_unique<remoting::AudioPacket>(*expected_packet));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return captured_audio_packets_.size() == 1; }));
  EXPECT_THAT(*expected_packet, EqualsProto(*captured_audio_packets_[0]));
}

TEST_F(AudioCapturerChromeOsTest, SimulateError) {
  audio_capturer_->Start(
      base::BindRepeating(&AudioCapturerChromeOsTest::OnAudioPacketCaptured,
                          base::Unretained(this)));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return audio_helper_chromeos_->audio_stream_started(); }));

  audio_helper_chromeos_->SimulateError();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return audio_helper_chromeos_->stop_audio_stream_call_count() == 1;
  }));
}

TEST_F(AudioCapturerChromeOsTest, CreateWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kBocaHostAudio);
  EXPECT_NE(AudioCapturer::Create(), nullptr);
}

TEST_F(AudioCapturerChromeOsTest, CreateWithFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(ash::features::kBocaHostAudio);
  EXPECT_EQ(AudioCapturer::Create(), nullptr);
}

}  // namespace remoting
