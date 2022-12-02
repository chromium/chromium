// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_controller.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/audio/aecdump_recording_manager.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_processing.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/device_output_listener.h"
#include "services/audio/processing_audio_fifo.h"
#include "services/audio/reference_output.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::StrictMock;

namespace audio {

namespace {

const int kSampleRate = media::AudioParameters::kAudioCDSampleRate;
const media::ChannelLayoutConfig kChannelLayoutConfig =
    media::ChannelLayoutConfig::Stereo();
const int kSamplesPerPacket = kSampleRate / 100;

// InputController will poll once every second, so wait at most a bit
// more than that for the callbacks.
constexpr base::TimeDelta kOnMutePollInterval = base::Milliseconds(1000);

enum class ProcessingFifoSetting {
  kEnabled,
  kEnabledWithSizeZero,
  kDisabled,
};

const std::string kFifoSizeParameter = "processing_fifo_size";
const std::map<std::string, std::string> kDisabledFifoParam{
    {kFifoSizeParameter, "0"}};
const std::map<std::string, std::string> kEnabledFifoParam{
    {kFifoSizeParameter, "110"}};

}  // namespace

class MockInputControllerEventHandler : public InputController::EventHandler {
 public:
  MockInputControllerEventHandler() = default;

  MockInputControllerEventHandler(const MockInputControllerEventHandler&) =
      delete;
  MockInputControllerEventHandler& operator=(
      const MockInputControllerEventHandler&) = delete;

  void OnLog(base::StringPiece) override {}

  MOCK_METHOD1(OnCreated, void(bool initially_muted));
  MOCK_METHOD1(OnError, void(InputController::ErrorCode error_code));
  MOCK_METHOD1(OnMuted, void(bool is_muted));
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

template <base::test::TaskEnvironment::TimeSource TimeSource =
              base::test::TaskEnvironment::TimeSource::MOCK_TIME>
class TimeSourceInputControllerTest
    : public ::testing::TestWithParam<ProcessingFifoSetting> {
 public:
  TimeSourceInputControllerTest()
      : task_environment_(TimeSource),
        audio_manager_(std::make_unique<media::FakeAudioManager>(
            std::make_unique<media::TestAudioThread>(false),
            &log_factory_)),
        aecdump_recording_manager_(audio_manager_->GetTaskRunner()),
        params_(media::AudioParameters::AUDIO_FAKE,
                kChannelLayoutConfig,
                kSampleRate,
                kSamplesPerPacket) {
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
    switch (GetParam()) {
      case ProcessingFifoSetting::kEnabled:
        processing_fifo_feature_.InitAndEnableFeatureWithParameters(
            media::kChromeWideEchoCancellation, kEnabledFifoParam);
        break;
      case ProcessingFifoSetting::kEnabledWithSizeZero:
        processing_fifo_feature_.InitAndEnableFeatureWithParameters(
            media::kChromeWideEchoCancellation, kDisabledFifoParam);
        break;
      case ProcessingFifoSetting::kDisabled:
        processing_fifo_feature_.InitAndDisableFeature(
            media::kChromeWideEchoCancellation);
        break;
    };
#endif
  }

  TimeSourceInputControllerTest(const TimeSourceInputControllerTest&) = delete;
  TimeSourceInputControllerTest& operator=(
      const TimeSourceInputControllerTest&) = delete;

  ~TimeSourceInputControllerTest() override {
    audio_manager_->Shutdown();
    task_environment_.RunUntilIdle();
  }

 protected:
  virtual void CreateAudioController() {
    controller_ = InputController::Create(
        audio_manager_.get(), &event_handler_, &sync_writer_,
        &user_input_monitor_,
        /*device_output_listener =*/nullptr, &aecdump_recording_manager_,
        /*processing_config =*/nullptr, params_,
        media::AudioDeviceDescription::kDefaultDeviceId, false);
  }

  bool IsProcessingFifoEnabled() {
    return GetParam() == ProcessingFifoSetting::kEnabled;
  }

  base::test::TaskEnvironment task_environment_;

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  base::test::ScopedFeatureList processing_fifo_feature_;
#endif

  std::unique_ptr<media::AudioManager> audio_manager_;
  media::AecdumpRecordingManager aecdump_recording_manager_;
  std::unique_ptr<InputController> controller_;
  media::FakeAudioLogFactory log_factory_;
  MockInputControllerEventHandler event_handler_;
  MockSyncWriter sync_writer_;
  MockUserInputMonitor user_input_monitor_;
  media::AudioParameters params_;
};

auto test_name_generator =
    [](const ::testing::TestParamInfo<
        TimeSourceInputControllerTest<>::ParamType>& info) {
      switch (info.param) {
        case ProcessingFifoSetting::kEnabled:
          return "FifoEnabled";
        case ProcessingFifoSetting::kEnabledWithSizeZero:
          return "FifoEnabledWithSizeZero";
        case ProcessingFifoSetting::kDisabled:
          return "FifoDisabled";
      }
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

TEST_P(InputControllerTest, RecordTwice) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  EXPECT_CALL(user_input_monitor_, EnableKeyPressMonitoring());
  controller_->Record();
  controller_->Record();

  EXPECT_CALL(user_input_monitor_, DisableKeyPressMonitoring());
  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();
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

INSTANTIATE_TEST_SUITE_P(
    InputControllerTest,
    InputControllerTest,
    testing::Values(ProcessingFifoSetting::kEnabled,
                    ProcessingFifoSetting::kEnabledWithSizeZero,
                    ProcessingFifoSetting::kDisabled),
    test_name_generator);

INSTANTIATE_TEST_SUITE_P(
    SystemTimeInputControllerTest,
    SystemTimeInputControllerTest,
    testing::Values(ProcessingFifoSetting::kEnabled,
                    ProcessingFifoSetting::kEnabledWithSizeZero,
                    ProcessingFifoSetting::kDisabled),
    test_name_generator);

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
class InputControllerTestHelper {
 public:
  explicit InputControllerTestHelper(InputController* controller)
      : controller_(controller) {}

  bool IsUsingProcessingThread() {
    return !!controller_->processing_fifo_.get();
  }

  // Adds a callback that will be run immediately after processing is done, in
  // the same sequence as the processing callback.
  // Should be called before starting the processing thread.
  void AttachOnProcessedCallback(base::RepeatingClosure on_processed_callback) {
    controller_->processing_fifo_->AttachOnProcessedCallbackForTesting(
        std::move(on_processed_callback));
  }

 private:
  raw_ptr<InputController> controller_;
};

class MockDeviceOutputListener : public DeviceOutputListener {
 public:
  MockDeviceOutputListener() = default;
  ~MockDeviceOutputListener() override = default;

  MOCK_METHOD2(StartListening,
               void(ReferenceOutput::Listener*, const std::string&));
  MOCK_METHOD1(StopListening, void(ReferenceOutput::Listener*));
};

template <base::test::TaskEnvironment::TimeSource TimeSource =
              base::test::TaskEnvironment::TimeSource::MOCK_TIME>
class TimeSourceInputControllerTestWithDeviceListener
    : public TimeSourceInputControllerTest<TimeSource> {
 protected:
  void CreateAudioController() final {
    // Must use |this| to access template base class members:
    // https://stackoverflow.com/q/4643074
    this->controller_ = InputController::Create(
        this->audio_manager_.get(), &this->event_handler_, &this->sync_writer_,
        &this->user_input_monitor_, &this->device_output_listener_,
        &this->aecdump_recording_manager_, std::move(processing_config_),
        this->params_, media::AudioDeviceDescription::kDefaultDeviceId, false);

    helper_ =
        std::make_unique<InputControllerTestHelper>(this->controller_.get());
  }

  enum class AudioProcessingType {
    // No effects, audio does not need to be modified.
    kNone,
    // Effects that modify audio but do not require a playout reference signal.
    kWithoutPlayoutReference,
    // Effects that require a playout reference signal.
    kWithPlayoutReference
  };

  void SetupProcessingConfig(AudioProcessingType audio_processing_type) {
    media::AudioProcessingSettings settings;
    settings.echo_cancellation = false;
    settings.noise_suppression = false;
    settings.transient_noise_suppression = false;
    settings.automatic_gain_control = false;
    settings.high_pass_filter = false;
    settings.multi_channel_capture_processing = false;
    settings.stereo_mirroring = false;
    settings.force_apm_creation = false;
    switch (audio_processing_type) {
      case AudioProcessingType::kNone:
        break;
      case AudioProcessingType::kWithoutPlayoutReference:
        settings.noise_suppression = true;
        break;
      case AudioProcessingType::kWithPlayoutReference:
        settings.echo_cancellation = true;
        break;
    }
    processing_config_ = media::mojom::AudioProcessingConfig::New(
        remote_controls_.BindNewPipeAndPassReceiver(), settings);
  }

  NiceMock<MockDeviceOutputListener> device_output_listener_;
  media::mojom::AudioProcessingConfigPtr processing_config_;
  mojo::Remote<media::mojom::AudioProcessorControls> remote_controls_;
  std::unique_ptr<InputControllerTestHelper> helper_;
};

using SystemTimeInputControllerTestWithDeviceListener =
    TimeSourceInputControllerTestWithDeviceListener<
        base::test::TaskEnvironment::TimeSource::SYSTEM_TIME>;
using InputControllerTestWithDeviceListener =
    TimeSourceInputControllerTestWithDeviceListener<>;

TEST_P(InputControllerTestWithDeviceListener,
       CreateWithAudioProcessingConfig_WithSomeEffectsEnabled) {
  SetupProcessingConfig(AudioProcessingType::kWithoutPlayoutReference);

  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           loop.QuitClosure());
  loop.Run();

  // |controller_| should have bound the pending AudioProcessorControls
  // receiver it received through its ctor.
  EXPECT_TRUE(remote_controls_.is_connected());

  // InputController shouldn't offload processing work when there is no playout
  // reference.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());

  // Test cleanup.
  controller_->Close();
}

TEST_P(InputControllerTestWithDeviceListener,
       CreateWithAudioProcessingConfig_WithoutEnablingEffects) {
  SetupProcessingConfig(AudioProcessingType::kNone);

  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           loop.QuitClosure());
  loop.Run();

  // When all forms of audio processing are disabled, |controller_| should
  // ignore the pending AudioProcessorControls Receiver it received in its ctor.
  EXPECT_FALSE(remote_controls_.is_connected());

  // InputController shouldn't spin up a processing thread if it's not needed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());

  // Test cleanup.
  controller_->Close();
}

TEST_P(InputControllerTestWithDeviceListener,
       CreateWithAudioProcessingConfig_VerifyFifoUsage) {
  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);

  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  // The processing thread should only be enabled when the processing FIFO
  // is explicitly enabled.
  EXPECT_EQ(IsProcessingFifoEnabled(), helper_->IsUsingProcessingThread());

  // Test cleanup.
  controller_->Close();
}

TEST_P(
    InputControllerTestWithDeviceListener,
    CreateWithAudioProcessingConfig_DoesNotListenForPlayoutReferenceIfNotRequired) {
  base::test::ScopedFeatureList features;
  const std::string kOutputDeviceId = "0x123";

  EXPECT_CALL(device_output_listener_, StartListening(_, _)).Times(0);
  EXPECT_CALL(device_output_listener_, StopListening(_)).Times(0);

  SetupProcessingConfig(AudioProcessingType::kWithoutPlayoutReference);
  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  controller_->Record();
  controller_->SetOutputDeviceForAec(kOutputDeviceId);

  // InputController spin up a processing thread if it's not needed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());

  controller_->Close();

  EXPECT_FALSE(helper_->IsUsingProcessingThread());
}

TEST_P(InputControllerTestWithDeviceListener, RecordBeforeSetOutputForAec) {
  const std::string kOutputDeviceId = "0x123";

  // Calling Record() will start listening to the "" device by default.
  EXPECT_CALL(device_output_listener_, StartListening(_, "")).Times(1);
  EXPECT_CALL(device_output_listener_, StartListening(_, kOutputDeviceId))
      .Times(1);
  EXPECT_CALL(device_output_listener_, StopListening(_)).Times(1);

  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  controller_->Record();
  controller_->SetOutputDeviceForAec(kOutputDeviceId);

  // InputController should offload processing to its own thread, if enabled.
  EXPECT_EQ(IsProcessingFifoEnabled(), helper_->IsUsingProcessingThread());

  controller_->Close();

  // The processing thread should be stopped after controller has closed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());
}

TEST_P(InputControllerTestWithDeviceListener, RecordAfterSetOutputForAec) {
  const std::string kOutputDeviceId = "0x123";
  EXPECT_CALL(device_output_listener_, StartListening(_, kOutputDeviceId))
      .Times(1);
  EXPECT_CALL(device_output_listener_, StopListening(_)).Times(1);

  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  controller_->SetOutputDeviceForAec(kOutputDeviceId);
  controller_->Record();

  // InputController should offload processing to its own thread, if enabled.
  EXPECT_EQ(IsProcessingFifoEnabled(), helper_->IsUsingProcessingThread());

  controller_->Close();

  // The processing thread should be stopped after controller has closed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());
}

TEST_P(InputControllerTestWithDeviceListener, ChangeOutputForAec) {
  const std::string kOutputDeviceId = "0x123";
  const std::string kOtherOutputDeviceId = "0x987";

  // Each output ID should receive one call to StartListening().
  EXPECT_CALL(device_output_listener_, StartListening(_, kOutputDeviceId))
      .Times(1);
  EXPECT_CALL(device_output_listener_, StartListening(_, kOtherOutputDeviceId))
      .Times(1);

  // StopListening() should be called once, regardless of how many ID changes.
  EXPECT_CALL(device_output_listener_, StopListening(_)).Times(1);

  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  controller_->SetOutputDeviceForAec(kOutputDeviceId);
  controller_->Record();
  controller_->SetOutputDeviceForAec(kOtherOutputDeviceId);
  controller_->Close();
}

// Test a normal call sequence of create, record and close when audio processing
// is enabled.
// Note: Must use system time as MOCK_TIME does not support the threads created
// by the FakeAudioInputStream. The callbacks to sync_writer_.Write() are on
// that thread, and thus we must use SYSTEM_TIME.
TEST_P(SystemTimeInputControllerTestWithDeviceListener, CreateRecordAndClose) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  bool data_processed_by_fifo = false;

  if (IsProcessingFifoEnabled()) {
    auto main_sequence = base::SequencedTaskRunner::GetCurrentDefault();
    auto verify_data_processed = [&data_processed_by_fifo, main_sequence]() {
      // Data should be processed on its own thread.
      EXPECT_FALSE(main_sequence->RunsTasksInCurrentSequence());

      data_processed_by_fifo = true;
    };

    helper_->AttachOnProcessedCallback(
        base::BindLambdaForTesting(verify_data_processed));
  }

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

  // InputController should offload processing to its own thread if the
  // processing FIFO is enabled.
  EXPECT_EQ(IsProcessingFifoEnabled(), helper_->IsUsingProcessingThread());

  loop.Run();

  testing::Mock::VerifyAndClearExpectations(&user_input_monitor_);
  testing::Mock::VerifyAndClearExpectations(&sync_writer_);

  EXPECT_CALL(sync_writer_, Close());
  EXPECT_CALL(user_input_monitor_, DisableKeyPressMonitoring());
  controller_->Close();

  // The processing thread should be stopped after controller has closed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(data_processed_by_fifo, IsProcessingFifoEnabled());
}

INSTANTIATE_TEST_SUITE_P(
    InputControllerTestWithDeviceListener,
    InputControllerTestWithDeviceListener,
    testing::Values(ProcessingFifoSetting::kEnabled,
                    ProcessingFifoSetting::kEnabledWithSizeZero,
                    ProcessingFifoSetting::kDisabled),
    test_name_generator);

INSTANTIATE_TEST_SUITE_P(
    SystemTimeInputControllerTestWithDeviceListener,
    SystemTimeInputControllerTestWithDeviceListener,
    testing::Values(ProcessingFifoSetting::kEnabled,
                    ProcessingFifoSetting::kEnabledWithSizeZero,
                    ProcessingFifoSetting::kDisabled),
    test_name_generator);

#endif  // BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

}  // namespace audio
