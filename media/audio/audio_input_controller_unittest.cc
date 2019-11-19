// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_controller.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/user_input_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using base::WaitableEvent;

namespace media {

namespace {

const int kSampleRate = AudioParameters::kAudioCDSampleRate;
const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
const int kSamplesPerPacket = kSampleRate / 10;

const double kMaxVolume = 1.0;

// AudioInputController will poll once every second, so wait at most a bit
// more than that for the callbacks.
constexpr base::TimeDelta kOnMuteWaitTimeout =
    base::TimeDelta::FromMilliseconds(1500);

// Runs |quit_closure| after the |count| of calls reaches |limit|.
ACTION_P3(CheckCountAndPostQuitTask, count, limit, quit_closure) {
  if (++*count >= limit)
    quit_closure.Run();
}

void RunLoopWithTimeout(base::RunLoop* run_loop, base::TimeDelta timeout) {
  base::OneShotTimer timeout_timer;
  timeout_timer.Start(FROM_HERE, timeout, run_loop->QuitClosure());
  run_loop->Run();
}

}  // namespace

class MockAudioInputControllerEventHandler
    : public AudioInputController::EventHandler {
 public:
  MockAudioInputControllerEventHandler() = default;

  void OnLog(base::StringPiece) override {}

  MOCK_METHOD1(OnCreated, void(bool initially_muted));
  MOCK_METHOD1(OnError, void(AudioInputController::ErrorCode error_code));
  MOCK_METHOD1(OnMuted, void(bool is_muted));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputControllerEventHandler);
};

class MockSyncWriter : public AudioInputController::SyncWriter {
 public:
  MockSyncWriter() = default;

  MOCK_METHOD4(Write,
               void(const AudioBus* data,
                    double volume,
                    bool key_pressed,
                    base::TimeTicks capture_time));
  MOCK_METHOD0(Close, void());
};

class MockUserInputMonitor : public UserInputMonitor {
 public:
  MockUserInputMonitor() = default;

  uint32_t GetKeyPressCount() const override { return 0; }

  MOCK_METHOD0(EnableKeyPressMonitoring, void());
  MOCK_METHOD0(DisableKeyPressMonitoring, void());
};

class MockAudioInputStream : public AudioInputStream {
 public:
  MockAudioInputStream() {}
  ~MockAudioInputStream() override {}

  void Start(AudioInputCallback*) override {}
  void Stop() override {}
  void Close() override {}
  double GetMaxVolume() override { return kMaxVolume; }
  double GetVolume() override { return 0; }
  bool SetAutomaticGainControl(bool) override { return false; }
  bool GetAutomaticGainControl() override { return false; }
  bool IsMuted() override { return false; }
  void SetOutputDeviceForAec(const std::string&) override {}

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(SetVolume, void(double));
};

class AudioInputControllerTest : public testing::TestWithParam<bool> {
 public:
  AudioInputControllerTest()
      : run_on_audio_thread_(GetParam()),
        audio_manager_(AudioManager::CreateForTesting(
            std::make_unique<TestAudioThread>(!run_on_audio_thread_))),
        params_(AudioParameters::AUDIO_FAKE,
                kChannelLayout,
                kSampleRate,
                kSamplesPerPacket) {}

  ~AudioInputControllerTest() override {
    audio_manager_->Shutdown();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void CreateAudioController() {
    controller_ = AudioInputController::Create(
        audio_manager_.get(), &event_handler_, &sync_writer_,
        &user_input_monitor_, params_, AudioDeviceDescription::kDefaultDeviceId,
        false);
  }

  void CloseAudioController() {
    if (run_on_audio_thread_) {
      controller_->Close(base::OnceClosure());
      return;
    }

    base::RunLoop run_loop;
    controller_->Close(run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Parameterize tests to run AudioInputController either on audio thread
  // (synchronously), or on a different thread (non-blocking).
  bool run_on_audio_thread_;

  scoped_refptr<AudioInputController> controller_;
  std::unique_ptr<AudioManager> audio_manager_;
  MockAudioInputControllerEventHandler event_handler_;
  MockSyncWriter sync_writer_;
  MockUserInputMonitor user_input_monitor_;
  AudioParameters params_;
  MockAudioInputStream stream_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputControllerTest);
};

TEST_P(AudioInputControllerTest, CreateAndCloseWithoutRecording) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_.get());

  EXPECT_CALL(sync_writer_, Close());
  CloseAudioController();
}

// Test a normal call sequence of create, record and close.
TEST_P(AudioInputControllerTest, CreateRecordAndClose) {
  int count = 0;

  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  // Write() should be called ten times.
  base::RunLoop run_loop;
  EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _))
      .Times(AtLeast(10))
      .WillRepeatedly(
          CheckCountAndPostQuitTask(&count, 10, run_loop.QuitClosure()));
  EXPECT_CALL(user_input_monitor_, EnableKeyPressMonitoring());
  controller_->Record();

  // Record and wait until ten Write() callbacks are received.
  run_loop.Run();

  EXPECT_CALL(user_input_monitor_, DisableKeyPressMonitoring());
  EXPECT_CALL(sync_writer_, Close());
  CloseAudioController();
}

TEST_P(AudioInputControllerTest, CloseTwice) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  EXPECT_CALL(user_input_monitor_, EnableKeyPressMonitoring());
  controller_->Record();

  EXPECT_CALL(user_input_monitor_, DisableKeyPressMonitoring());
  EXPECT_CALL(sync_writer_, Close());
  CloseAudioController();

  CloseAudioController();
}

// Test that AudioInputController sends OnMute callbacks properly.
TEST_P(AudioInputControllerTest, TestOnmutedCallbackInitiallyUnmuted) {
  const auto timeout = kOnMuteWaitTimeout;

  WaitableEvent callback_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);

  base::RunLoop unmute_run_loop;
  base::RunLoop mute_run_loop;
  base::RunLoop setup_run_loop;
  EXPECT_CALL(event_handler_, OnCreated(false)).WillOnce(InvokeWithoutArgs([&] {
    setup_run_loop.QuitWhenIdle();
  }));
  EXPECT_CALL(sync_writer_, Close());
  EXPECT_CALL(event_handler_, OnMuted(true)).WillOnce(InvokeWithoutArgs([&] {
    mute_run_loop.Quit();
  }));
  EXPECT_CALL(event_handler_, OnMuted(false)).WillOnce(InvokeWithoutArgs([&] {
    unmute_run_loop.Quit();
  }));

  FakeAudioInputStream::SetGlobalMutedState(false);
  CreateAudioController();
  ASSERT_TRUE(controller_.get());
  RunLoopWithTimeout(&setup_run_loop, timeout);

  FakeAudioInputStream::SetGlobalMutedState(true);
  RunLoopWithTimeout(&mute_run_loop, timeout);
  FakeAudioInputStream::SetGlobalMutedState(false);
  RunLoopWithTimeout(&unmute_run_loop, timeout);

  CloseAudioController();
}

TEST_P(AudioInputControllerTest, TestOnmutedCallbackInitiallyMuted) {
  const auto timeout = kOnMuteWaitTimeout;

  WaitableEvent callback_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);

  base::RunLoop unmute_run_loop;
  base::RunLoop setup_run_loop;
  EXPECT_CALL(event_handler_, OnCreated(true)).WillOnce(InvokeWithoutArgs([&] {
    setup_run_loop.QuitWhenIdle();
  }));
  EXPECT_CALL(sync_writer_, Close());
  EXPECT_CALL(event_handler_, OnMuted(false)).WillOnce(InvokeWithoutArgs([&] {
    unmute_run_loop.Quit();
  }));

  FakeAudioInputStream::SetGlobalMutedState(true);
  CreateAudioController();
  ASSERT_TRUE(controller_.get());
  RunLoopWithTimeout(&setup_run_loop, timeout);

  FakeAudioInputStream::SetGlobalMutedState(false);
  RunLoopWithTimeout(&unmute_run_loop, timeout);

  CloseAudioController();
}

TEST_P(AudioInputControllerTest, CreateForStream) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  EXPECT_CALL(stream_, Open()).WillOnce(InvokeWithoutArgs([] { return true; }));
  controller_ = AudioInputController::CreateForStream(
      audio_manager_->GetTaskRunner(), &event_handler_, &stream_, &sync_writer_,
      &user_input_monitor_);

  EXPECT_CALL(sync_writer_, Close());
  CloseAudioController();
}

TEST_P(AudioInputControllerTest, SetVolume) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  EXPECT_CALL(stream_, Open()).WillOnce(InvokeWithoutArgs([] { return true; }));
  controller_ = AudioInputController::CreateForStream(
      audio_manager_->GetTaskRunner(), &event_handler_, &stream_, &sync_writer_,
      &user_input_monitor_);

  const double volume = 0.5;
  EXPECT_CALL(stream_, SetVolume(volume));
  controller_->SetVolume(volume);

  EXPECT_CALL(sync_writer_, Close());
  CloseAudioController();
}

INSTANTIATE_TEST_SUITE_P(SyncAsync, AudioInputControllerTest, testing::Bool());

}  // namespace media
