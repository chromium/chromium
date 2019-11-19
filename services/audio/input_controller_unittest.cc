// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_controller.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_processing.h"
#include "media/base/user_input_monitor.h"
#include "media/webrtc/audio_processor.h"
#include "media/webrtc/webrtc_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using base::WaitableEvent;

namespace audio {

namespace {

const int kSampleRate = media::AudioParameters::kAudioCDSampleRate;
const media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
const int kSamplesPerPacket = kSampleRate / 100;

const double kMaxVolume = 1.0;

// InputController will poll once every second, so wait at most a bit
// more than that for the callbacks.
constexpr base::TimeDelta kOnMutePollInterval =
    base::TimeDelta::FromMilliseconds(1000);

}  // namespace

class MockInputControllerEventHandler : public InputController::EventHandler {
 public:
  MockInputControllerEventHandler() = default;

  void OnLog(base::StringPiece) override {}

  MOCK_METHOD1(OnCreated, void(bool initially_muted));
  MOCK_METHOD1(OnError, void(InputController::ErrorCode error_code));
  MOCK_METHOD1(OnMuted, void(bool is_muted));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputControllerEventHandler);
};

class MockSyncWriter : public InputController::SyncWriter {
 public:
  MockSyncWriter() = default;

  MOCK_METHOD4(Write,
               void(const media::AudioBus* data,
                    double volume,
                    bool key_pressed,
                    base::TimeTicks capture_time));
  MOCK_METHOD0(Close, void());
};

class MockUserInputMonitor : public media::UserInputMonitor {
 public:
  MockUserInputMonitor() = default;

  uint32_t GetKeyPressCount() const override { return 0; }

  MOCK_METHOD0(EnableKeyPressMonitoring, void());
  MOCK_METHOD0(DisableKeyPressMonitoring, void());
};

class MockAudioInputStream : public media::AudioInputStream {
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

// Parameter: use audio processing.
template <base::test::TaskEnvironment::TimeSource TimeSource =
              base::test::TaskEnvironment::TimeSource::MOCK_TIME>
class TimeSourceInputControllerTest : public ::testing::TestWithParam<bool> {
 public:
  TimeSourceInputControllerTest()
      : task_environment_(TimeSource),
        audio_manager_(std::make_unique<media::FakeAudioManager>(
            std::make_unique<media::TestAudioThread>(false),
            &log_factory_)),
        params_(media::AudioParameters::AUDIO_FAKE,
                kChannelLayout,
                kSampleRate,
                kSamplesPerPacket) {
#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
    if (GetParam()) {
      audio_processing_feature_.InitWithFeatures(
          {features::kWebRtcApmInAudioService}, {});
    }
#endif
  }

  ~TimeSourceInputControllerTest() override {
    audio_manager_->Shutdown();
    task_environment_.RunUntilIdle();
  }

 protected:
  void CreateAudioController() {
    mojom::AudioProcessingConfigPtr config_ptr;
#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
    if (GetParam()) {
      media::AudioProcessingSettings settings;
      settings.echo_cancellation = media::EchoCancellationType::kAec3;
      config_ptr = mojom::AudioProcessingConfigPtr(
          base::in_place, remote_controls_.BindNewPipeAndPassReceiver(),
          base::UnguessableToken::Create(), settings);
    }
#endif

    controller_ = InputController::Create(
        audio_manager_.get(), &event_handler_, &sync_writer_,
        &user_input_monitor_, params_,
        media::AudioDeviceDescription::kDefaultDeviceId, false,
        &stream_monitor_coordinator_, std::move(config_ptr));
  }

  base::test::TaskEnvironment task_environment_;

  StreamMonitorCoordinator stream_monitor_coordinator_;
  std::unique_ptr<InputController> controller_;
  media::FakeAudioLogFactory log_factory_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  MockInputControllerEventHandler event_handler_;
  MockSyncWriter sync_writer_;
  MockUserInputMonitor user_input_monitor_;
  media::AudioParameters params_;
  MockAudioInputStream stream_;
  base::test::ScopedFeatureList audio_processing_feature_;
  mojo::Remote<mojom::AudioProcessorControls> remote_controls_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeSourceInputControllerTest);
};

using SystemTimeInputControllerTest = TimeSourceInputControllerTest<
    base::test::TaskEnvironment::TimeSource::SYSTEM_TIME>;
using InputControllerTest = TimeSourceInputControllerTest<>;

TEST_P(InputControllerTest, CreateAndCloseWithoutRecording) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(controller_.get());

  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();
}

// Test a normal call sequence of create, record and close.
// Note: Must use system time as MOCK_TIME does not support the threads created
// by the FakeAudioInputStream. The callbacks to sync_writer_.Write() are on
// that thread, and thus we must use SYSTEM_TIME.
TEST_P(SystemTimeInputControllerTest, CreateRecordAndClose) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  base::RunLoop loop;

  {
    // Wait for Write() to be called ten times.
    testing::InSequence s;
    EXPECT_CALL(user_input_monitor_, EnableKeyPressMonitoring());
    EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _)).Times(Exactly(9));
    EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _))
        .Times(AtLeast(1))
        .WillOnce(InvokeWithoutArgs([&]() { loop.Quit(); }));
  }
  controller_->Record();
  loop.Run();

  testing::Mock::VerifyAndClearExpectations(&user_input_monitor_);
  testing::Mock::VerifyAndClearExpectations(&sync_writer_);

  EXPECT_CALL(sync_writer_, Close());
  EXPECT_CALL(user_input_monitor_, DisableKeyPressMonitoring());
  controller_->Close();

  task_environment_.RunUntilIdle();
}

TEST_P(InputControllerTest, CloseTwice) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  EXPECT_CALL(user_input_monitor_, EnableKeyPressMonitoring());
  controller_->Record();

  EXPECT_CALL(user_input_monitor_, DisableKeyPressMonitoring());
  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();

  controller_->Close();
}

// Test that InputController sends OnMute callbacks properly.
TEST_P(InputControllerTest, TestOnmutedCallbackInitiallyUnmuted) {
  WaitableEvent callback_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(event_handler_, OnCreated(false));
  EXPECT_CALL(sync_writer_, Close());

  media::FakeAudioInputStream::SetGlobalMutedState(false);
  CreateAudioController();
  ASSERT_TRUE(controller_.get());
  task_environment_.FastForwardBy(kOnMutePollInterval);

  testing::Mock::VerifyAndClearExpectations(&event_handler_);
  EXPECT_CALL(event_handler_, OnMuted(true));
  media::FakeAudioInputStream::SetGlobalMutedState(true);
  task_environment_.FastForwardBy(kOnMutePollInterval);

  testing::Mock::VerifyAndClearExpectations(&event_handler_);
  EXPECT_CALL(event_handler_, OnMuted(false));
  media::FakeAudioInputStream::SetGlobalMutedState(false);
  task_environment_.FastForwardBy(kOnMutePollInterval);

  controller_->Close();
}

TEST_P(InputControllerTest, TestOnmutedCallbackInitiallyMuted) {
  WaitableEvent callback_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(event_handler_, OnCreated(true));
  EXPECT_CALL(sync_writer_, Close());

  media::FakeAudioInputStream::SetGlobalMutedState(true);
  CreateAudioController();
  ASSERT_TRUE(controller_.get());
  task_environment_.FastForwardBy(kOnMutePollInterval);

  testing::Mock::VerifyAndClearExpectations(&event_handler_);

  EXPECT_CALL(event_handler_, OnMuted(false));
  media::FakeAudioInputStream::SetGlobalMutedState(false);
  task_environment_.FastForwardBy(kOnMutePollInterval);

  controller_->Close();
}

#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
INSTANTIATE_TEST_SUITE_P(, InputControllerTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(, SystemTimeInputControllerTest, ::testing::Bool());
#else
INSTANTIATE_TEST_SUITE_P(, InputControllerTest, testing::Values(false));
INSTANTIATE_TEST_SUITE_P(,
                         SystemTimeInputControllerTest,
                         ::testing::Values(false));
#endif

}  // namespace audio
