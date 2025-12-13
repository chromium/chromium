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
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
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
  ~FakeAudioHelperChromeOs() override = default;

  FakeAudioHelperChromeOs(const FakeAudioHelperChromeOs&) = delete;
  FakeAudioHelperChromeOs& operator=(const FakeAudioHelperChromeOs&) = delete;

  void StartAudioStream(AudioPlaybackMode audio_playback_mode,
                        OnDataCallback on_data_callback,
                        OnErrorCallback on_error_callback) override {
    audio_playback_mode_ = audio_playback_mode;
    on_data_callback_ = std::move(on_data_callback);
    on_error_callback_ = std::move(on_error_callback);
    audio_stream_started_ = true;
  }

  void SimulateData(std::unique_ptr<AudioPacket> packet) {
    on_data_callback_.Run(std::move(packet));
  }

  void SimulateError() { on_error_callback_.Run(); }

  int stop_audio_stream_call_count() { return stop_audio_stream_call_count_; }

  bool audio_stream_started() { return audio_stream_started_; }

 private:
  AudioPlaybackMode audio_playback_mode_;
  OnDataCallback on_data_callback_;
  OnErrorCallback on_error_callback_;
  int stop_audio_stream_call_count_ = 0;
  bool audio_stream_started_ = false;
};

// This test fixture is designed to accurately simulate the multi-sequenced
// environment in which `AudioCapturerChromeOs` operates within CRD on CrOS. The
// production code involves interactions across three distinct sequences:
//
// 1.  *CRD Object Creation Sequence*: The sequence on which the
//     `AudioCapturerChromeOs` instance is constructed. In this test, this
//     corresponds to the **main test thread**.
//
// 2.  *CRD Audio Capture Control Sequence*: A separate, dedicated sequence from
//     which CRD calls `AudioCapturerChromeOs::Start()` and
//     `AudioCapturerChromeOs::Stop()`. This is represented in the test by the
//     base::SequencedTaskRunner `capture_runner_`.
//
// 3.  *AudioManager Audio Thread Sequence*: The sequence managed by
//     `media::AudioManager`'s internal audio thread. All operations on the
//     `AudioHelperChromeOs` instance are bound to and executed on this specific
//     sequence. In the test, this is obtained via
//     `audio_manager_->GetTaskRunner()` and stored in `audio_runner_`.
//
// The complexity of this setup is necessary to ensure the unit tests
// reproduce the asynchronous and cross-sequence interactions present in the
// real system. By using `base::RunLoop`, `base::PostTaskAndReply`, and posting
// tasks to runners, these tests can verify correct behavior when calls
// originate from and complete on different sequences, mirroring how
// `AudioCapturerChromeOs` is used in production.
class AudioCapturerChromeOsTest : public testing::Test {
 public:
  AudioCapturerChromeOsTest() {}
  ~AudioCapturerChromeOsTest() override = default;

  void SetUp() override {
    // This test setup seeks to replicate the real audio capturing environment
    // which involves 3 different sequences:
    // 1) The sequence AudioCapturerChromeOs is created on by CRD.
    // 2) An audio sequence CRD creates and calls
    //    `AudioCapturerChromeOs::Start()` on.
    // 3) The AudioManager's audio sequence, ran on the AudioThread. All of the
    //    AudioHelperChromeOs's tasks are expected to run on this sequence.
    //
    //  For this test here is how these sequences are represented:
    //  - The main test thread sequence = the sequence AudioCapturerChromeOs is
    //    created on.
    //  - `capture_runner_` = audio sequence CRD creates and starts on
    //  - `audio_runner_` = AudioManager's audio sequence
    capture_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});

    audio_manager_ = media::AudioManager::CreateForTesting(
        std::make_unique<media::TestAudioThread>(/* use_real_thread= */ true));
    audio_runner_ = audio_manager_->GetTaskRunner();

    std::unique_ptr<FakeAudioHelperChromeOs> fake_audio_helper_chromeos =
        std::make_unique<FakeAudioHelperChromeOs>();
    audio_helper_chromeos_ = fake_audio_helper_chromeos.get();

    audio_capturer_ = std::make_unique<AudioCapturerChromeOs>(
        std::move(fake_audio_helper_chromeos));
  }

  void TearDown() override {
    // Release the helper's raw_ptr before it's destroyed by the resetting of
    // the capturer.
    audio_helper_chromeos_ = nullptr;

    // The `audio_capturer_` needs to be destroyed on the same sequence that
    // AudioCapturerChromeOs::Start() was called on.
    base::RunLoop run_loop;
    capture_runner_->DeleteSoon(FROM_HERE, std::move(audio_capturer_));
    capture_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    audio_manager_->Shutdown();
  }

  void OnAudioPacketCaptured(std::unique_ptr<AudioPacket> packet) {
    captured_audio_packets_.push_back(std::move(packet));
  }

  base::test::TaskEnvironment task_environment_;

 protected:
  std::unique_ptr<AudioCapturerChromeOs> audio_capturer_;
  raw_ptr<FakeAudioHelperChromeOs> audio_helper_chromeos_;
  std::vector<std::unique_ptr<AudioPacket>> captured_audio_packets_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Runner for AudioCapturerChromeOs::Start/Stop.
  scoped_refptr<base::SequencedTaskRunner> capture_runner_;
  // Runner for AudioManager.
  scoped_refptr<base::SequencedTaskRunner> audio_runner_;

 private:
  std::unique_ptr<media::AudioManager> audio_manager_;
};

TEST_F(AudioCapturerChromeOsTest, SimulateAudioPacket) {
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

  base::RunLoop run_loop;

  // Simulate `audio_capturer_` being started by CRD on the "start" sequence.
  base::OnceClosure start_capture_task = base::BindOnce(
      base::IgnoreResult(&AudioCapturerChromeOs::Start),
      base::Unretained(audio_capturer_.get()),
      base::BindRepeating(&AudioCapturerChromeOsTest::OnAudioPacketCaptured,
                          base::Unretained(this)));

  base::OnceClosure start_capture_reply = base::BindLambdaForTesting([&]() {
    // Simulate audio data via `audio_helper_chromeos_` on the `audio_runner_`.
    base::OnceClosure simulate_data_task = base::BindOnce(
        &FakeAudioHelperChromeOs::SimulateData,
        base::Unretained(audio_helper_chromeos_.get()),
        std::make_unique<remoting::AudioPacket>(*expected_packet));

    // Once simulation is complete and
    // AudioCapturerChromeOsTest::OnAudioPacketCaptured is triggered, validate
    // the result on the "start" sequence.
    base::OnceClosure validate_and_quit_reply = base::BindPostTask(
        capture_runner_, base::BindLambdaForTesting([&] {
          EXPECT_EQ(1U, captured_audio_packets_.size());
          EXPECT_THAT(*expected_packet,
                      EqualsProto(*captured_audio_packets_[0]));
          run_loop.Quit();
        }));

    // Use PostTaskAndReply() to allow tasks on `audio_runner_` to complete
    // before attempting to validate.
    audio_runner_->PostTaskAndReply(FROM_HERE, std::move(simulate_data_task),
                                    std::move(validate_and_quit_reply));
  });

  capture_runner_->PostTask(
      FROM_HERE,
      std::move(start_capture_task).Then(std::move(start_capture_reply)));
  run_loop.Run();
}

TEST_F(AudioCapturerChromeOsTest, SimulateError) {
  base::RunLoop run_loop;

  base::OnceClosure simulate_error_task =
      base::BindOnce(&FakeAudioHelperChromeOs::SimulateError,
                     base::Unretained(audio_helper_chromeos_.get()));

  base::OnceClosure verify_packet_callback_reset_task = base::BindPostTask(
      capture_runner_,
      base::BindOnce(&FakeAudioHelperChromeOs::SimulateData,
                     base::Unretained(audio_helper_chromeos_.get()),
                     std::make_unique<remoting::AudioPacket>(AudioPacket()))
          .Then(base::BindLambdaForTesting([&]() {
            EXPECT_EQ(0u, captured_audio_packets_.size());
            run_loop.Quit();
          })));

  capture_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Simulate `audio_capturer_` being started by CRD on the "start"
        // sequence.
        audio_capturer_->Start(base::BindRepeating(
            &AudioCapturerChromeOsTest::OnAudioPacketCaptured,
            base::Unretained(this)));

        // Simulate error on `audio_helper_chromeos_` on the `audio_runner_`
        // then verify error closed the stream and reset the packet callback.
        audio_runner_->PostTaskAndReply(
            FROM_HERE, std::move(simulate_error_task),
            std::move(verify_packet_callback_reset_task));
      }));
  run_loop.Run();
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
