// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_controller.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "input_controller.h"
#include "media/audio/aecdump_recording_manager.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_processing.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/audio_processor_handler.h"
#include "services/audio/loopback_signal_provider.h"
#include "services/audio/processing_audio_fifo.h"
#include "services/audio/reference_output.h"
#include "services/audio/reference_signal_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
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

using ReferenceOpenOutcome = ReferenceSignalProvider::ReferenceOpenOutcome;

// Struct to hold the parameters for UMA delay tests.
struct DelayUmaTestData {
  ReferenceSignalProvider::Type provider_type;
  const char* expected_uma_name;
};

std::unique_ptr<LoopbackMixin> DoNotCreateLoopbackMixin(
    std::string_view device_id,
    const media::AudioParameters& params,
    LoopbackMixin::OnDataCallback on_data_callback) {
  return nullptr;
}
}  // namespace

class MockInputControllerEventHandler : public InputController::EventHandler {
 public:
  MockInputControllerEventHandler() = default;

  MockInputControllerEventHandler(const MockInputControllerEventHandler&) =
      delete;
  MockInputControllerEventHandler& operator=(
      const MockInputControllerEventHandler&) = delete;

  void OnLog(std::string_view) override {}

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
                    base::TimeTicks capture_time,
                    const media::AudioGlitchInfo& audio_glitch_info));
  MOCK_METHOD0(Close, void());
};

class MockAudioInputStream : public media::AudioInputStream {
 public:
  MOCK_METHOD0(Open, OpenOutcome());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(GetMaxVolume, double());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD0(GetVolume, double());
  MOCK_METHOD1(SetAutomaticGainControl, bool(bool enabled));
  MOCK_METHOD0(GetAutomaticGainControl, bool());
  MOCK_METHOD0(IsMuted, bool());
  MOCK_METHOD1(SetOutputDeviceForAec,
               void(const std::string& output_device_id));

  void Start(AudioInputCallback* callback) override {
    captured_callback_ = callback;
  }

  std::optional<AudioInputCallback*> captured_callback_;
};

class FakeLoopbackSignalProvider : public LoopbackSignalProviderInterface {
 public:
  FakeLoopbackSignalProvider(const media::AudioParameters& params,
                             LoopbackSignalProviderInterface* callback_receiver)
      : params_(params), callback_receiver_(callback_receiver) {}

  ~FakeLoopbackSignalProvider() override = default;

  void Start() override { callback_receiver_->Start(); }

  base::TimeTicks PullLoopbackData(media::AudioBus* audio_bus,
                                   base::TimeTicks capture_time,
                                   double volume) override {
    EXPECT_EQ(audio_bus->frames(), params_.frames_per_buffer());
    EXPECT_EQ(audio_bus->channels(), params_.channels());
    return callback_receiver_->PullLoopbackData(audio_bus, capture_time,
                                                volume);
  }

 private:
  const media::AudioParameters params_;
  raw_ptr<LoopbackSignalProviderInterface> const callback_receiver_;
};

class LoopbackMixinVerifier : public LoopbackSignalProviderInterface {
 public:
  LoopbackMixinVerifier() = default;
  ~LoopbackMixinVerifier() override = default;

  // LoopbackSignalProviderInterface
  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(base::TimeTicks,
              PullLoopbackData,
              (media::AudioBus * audio_bus,
               base::TimeTicks capture_time,
               double volume),
              (override));

  MOCK_METHOD(void, MaybeCreateMixinCalled, (std::string_view device_id));

  std::unique_ptr<LoopbackMixin> MaybeCreateMixin(
      std::string_view device_id,
      const media::AudioParameters& params,
      LoopbackMixin::OnDataCallback on_data_callback) {
    MaybeCreateMixinCalled(device_id);

    if (device_id != media::AudioDeviceDescription::kLoopbackWithoutChromeId) {
      return nullptr;
    }

    return std::make_unique<LoopbackMixinUnderTest>(
        std::make_unique<FakeLoopbackSignalProvider>(params, this), params,
        std::move(on_data_callback));
  }

 private:
  // Test class to access the protected constructor of LoopbackMixin.
  class LoopbackMixinUnderTest : public LoopbackMixin {
   public:
    LoopbackMixinUnderTest(
        std::unique_ptr<LoopbackSignalProviderInterface> signal_provider,
        const media::AudioParameters& params,
        OnDataCallback on_data_callback)
        : LoopbackMixin(std::move(signal_provider),
                        params,
                        std::move(on_data_callback)) {}
  };
};

enum class AudioManagerType { MOCK, FAKE };

template <base::test::TaskEnvironment::TimeSource TimeSource =
              base::test::TaskEnvironment::TimeSource::MOCK_TIME,
          AudioManagerType audio_manager_type = AudioManagerType::FAKE>
class TimeSourceInputControllerTest : public ::testing::Test {
 public:
  TimeSourceInputControllerTest()
      : task_environment_(TimeSource),
        audio_manager_(
            audio_manager_type == AudioManagerType::FAKE
                ? static_cast<std::unique_ptr<media::AudioManager>>(
                      std::make_unique<media::FakeAudioManager>(
                          std::make_unique<media::TestAudioThread>(false),
                          &log_factory_))
                : static_cast<std::unique_ptr<media::AudioManager>>(
                      std::make_unique<media::MockAudioManager>(
                          std::make_unique<media::TestAudioThread>(false)))),
        aecdump_recording_manager_(audio_manager_->GetTaskRunner()),
        params_(media::AudioParameters::AUDIO_FAKE,
                kChannelLayoutConfig,
                kSampleRate,
                kSamplesPerPacket) {}

  TimeSourceInputControllerTest(const TimeSourceInputControllerTest&) = delete;
  TimeSourceInputControllerTest& operator=(
      const TimeSourceInputControllerTest&) = delete;

  ~TimeSourceInputControllerTest() override {
    audio_manager_->Shutdown();
    task_environment_.RunUntilIdle();
  }

 protected:
  void CreateAudioControllerWithMixin(const std::string& device_id) {
    EXPECT_CALL(mixin_verifier_, MaybeCreateMixinCalled(device_id));
    controller_ = InputController::Create(
        audio_manager_.get(), &event_handler_, &sync_writer_,
        /*device_output_listener =*/nullptr, &aecdump_recording_manager_,
        /*ml_model_manager=*/nullptr,
        /*processing_config =*/nullptr,
        // base::Unretained is safe: `mixin_verifier_` outlives `controller_`
        base::BindOnce(&LoopbackMixinVerifier::MaybeCreateMixin,
                       base::Unretained(&mixin_verifier_)),
        params_, device_id, false);
  }

  virtual void CreateAudioController() {
    controller_ = InputController::Create(
        audio_manager_.get(), &event_handler_, &sync_writer_,
        /*device_output_listener =*/nullptr, &aecdump_recording_manager_,
        /*ml_model_manager=*/nullptr,
        /*processing_config =*/nullptr,
        base::BindOnce(&DoNotCreateLoopbackMixin), params_,
        media::AudioDeviceDescription::kDefaultDeviceId, false);
  }

  base::test::TaskEnvironment task_environment_;

  StrictMock<LoopbackMixinVerifier> mixin_verifier_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  media::AecdumpRecordingManager aecdump_recording_manager_;
  std::unique_ptr<InputController> controller_;
  media::FakeAudioLogFactory log_factory_;
  MockInputControllerEventHandler event_handler_;
  MockSyncWriter sync_writer_;
  media::AudioParameters params_;
};

using SystemTimeInputControllerTest = TimeSourceInputControllerTest<
    base::test::TaskEnvironment::TimeSource::SYSTEM_TIME>;
using InputControllerTest = TimeSourceInputControllerTest<>;
using InputControllerTestWithMockAudioManager = TimeSourceInputControllerTest<
    base::test::TaskEnvironment::TimeSource::MOCK_TIME,
    AudioManagerType::MOCK>;

TEST_F(InputControllerTest, CreateAndCloseWithoutRecording) {
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
// that thread, and thus we must use SYSTEM_TIME. Also verifies that the
// NoAudioServiceAEC UMA is logged when InputController is created without a
// ReferenceSignalProvider.
TEST_F(SystemTimeInputControllerTest, CreateRecordAndClose) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  base::HistogramTester histogram_tester;
  base::RunLoop loop;

  {
    // Wait for Write() to be called ten times.
    testing::InSequence s;
    EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _)).Times(Exactly(9));
    EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _))
        .Times(AtLeast(1))
        .WillOnce(InvokeWithoutArgs([&]() { loop.Quit(); }));
  }
  controller_->Record();
  loop.Run();

  testing::Mock::VerifyAndClearExpectations(&sync_writer_);

  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();
  histogram_tester.ExpectTotalCount(
      "Media.Audio.InputController.Delay.NoAudioServiceAEC", 10);

  task_environment_.RunUntilIdle();
}

TEST_F(InputControllerTestWithMockAudioManager, LoopbackMixinIsEngaged) {
  MockAudioInputStream mock_stream;
  static_cast<media::MockAudioManager*>(audio_manager_.get())
      ->SetMakeInputStreamCB(base::BindRepeating(
          [](media::AudioInputStream* stream,
             const media::AudioParameters& params,
             const std::string& device_id) { return stream; },
          &mock_stream));
  auto audio_bus = media::AudioBus::Create(params_);

  CreateAudioControllerWithMixin(
      media::AudioDeviceDescription::kLoopbackWithoutChromeId);
  ASSERT_TRUE(controller_.get());

  EXPECT_CALL(mixin_verifier_, Start());
  controller_->Record();

  ASSERT_TRUE(mock_stream.captured_callback_);
  media::AudioInputStream::AudioInputCallback* callback =
      *mock_stream.captured_callback_;

  // Loopbackmixin::OnData() is called.
  EXPECT_CALL(mixin_verifier_, PullLoopbackData(_, _, _));
  // Loopback passed data back to InputController.
  EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _));
  callback->OnData(audio_bus.get(), base::TimeTicks(), 1, {});

  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();
}

TEST_F(InputControllerTestWithMockAudioManager, PropagatesGlitchInfo) {
  MockAudioInputStream mock_stream;
  static_cast<media::MockAudioManager*>(audio_manager_.get())
      ->SetMakeInputStreamCB(base::BindRepeating(
          [](media::AudioInputStream* stream,
             const media::AudioParameters& params,
             const std::string& device_id) { return stream; },
          &mock_stream));
  auto audio_bus = media::AudioBus::Create(params_);

  CreateAudioController();
  ASSERT_TRUE(controller_.get());
  controller_->Record();

  ASSERT_TRUE(mock_stream.captured_callback_);
  media::AudioInputStream::AudioInputCallback* callback =
      *mock_stream.captured_callback_;

  for (int i = 0; i < 5; i++) {
    media::AudioGlitchInfo audio_glitch_info{
        .duration = base::Milliseconds(123 + i), .count = 5};
    EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, audio_glitch_info));
    callback->OnData(audio_bus.get(), base::TimeTicks(), 1, audio_glitch_info);
    testing::Mock::VerifyAndClearExpectations(&sync_writer_);
  }

  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();
}

TEST_F(InputControllerTest, RecordTwice) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  controller_->Record();
  controller_->Record();

  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();
}

TEST_F(InputControllerTest, CloseTwice) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  controller_->Record();

  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();

  controller_->Close();
}

// Test that InputController sends OnMute callbacks properly.
TEST_F(InputControllerTest, TestOnmutedCallbackInitiallyUnmuted) {
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

TEST_F(InputControllerTest, TestOnmutedCallbackInitiallyMuted) {
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

  int FifoSize() {
    CHECK(IsUsingProcessingThread());
    return controller_->processing_fifo_->fifo_size();
  }

  // Simulates the AudioProcessorHandler receiving an error.
  void CallOnReferenceStreamError() {
    // Cast to ReferenceOutput::Listener* to get public access to
    // OnReferenceStreamError.
    static_cast<ReferenceOutput::Listener*>(
        controller_->audio_processor_handler_.get())
        ->OnReferenceStreamError();
  }

 private:
  raw_ptr<InputController> controller_;
};

class MockReferenceSignalProvider : public ReferenceSignalProvider {
 public:
  MockReferenceSignalProvider() = default;
  ~MockReferenceSignalProvider() override = default;

  MOCK_METHOD(Type, GetType, (), (const, override));
  MOCK_METHOD(ReferenceOpenOutcome,
              StartListening,
              (ReferenceOutput::Listener*, const std::string&),
              (override));
  MOCK_METHOD(void, StopListening, (ReferenceOutput::Listener*), (override));
};

template <base::test::TaskEnvironment::TimeSource TimeSource =
              base::test::TaskEnvironment::TimeSource::MOCK_TIME>
class TimeSourceInputControllerTestWithReferenceSignalProvider
    : public TimeSourceInputControllerTest<TimeSource> {
 protected:
  void CreateAudioController() final {
    // Must use |this| to access template base class members:
    // https://stackoverflow.com/q/4643074
    this->controller_ = InputController::Create(
        this->audio_manager_.get(), &this->event_handler_, &this->sync_writer_,
        std::move(reference_signal_provider_unique_),
        &this->aecdump_recording_manager_, /*ml_model_manager=*/nullptr,
        std::move(processing_config_),
        base::BindOnce(&DoNotCreateLoopbackMixin), this->params_,
        media::AudioDeviceDescription::kDefaultDeviceId, false);

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
    settings.automatic_gain_control = false;
    settings.multi_channel_capture_processing = false;
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

  // Used for testing that a specific OpenOutcome is translated to a specific
  // ErrorCode.
  void TestReferenceOpenError(ReferenceOpenOutcome reference_open_outcome,
                              InputController::ErrorCode expected_error_code);

  // This may or may not be moved into the input controller on creation,
  // depending on if the InputController is going to do echo cancellation.
  std::unique_ptr<NiceMock<MockReferenceSignalProvider>>
      reference_signal_provider_unique_ =
          std::make_unique<NiceMock<MockReferenceSignalProvider>>();
  // The MockReferenceSignalProvider will be destroyed automatically when the
  // InputController is destroyed. We retain a pointer to it to be able to
  // expect mock calls. It will be dangling between the destruction of the
  // InputController and the destruction of the test suite, so we disable
  // dangling pointer detection.
  raw_ptr<NiceMock<MockReferenceSignalProvider>, DisableDanglingPtrDetection>
      reference_signal_provider_ = reference_signal_provider_unique_.get();
  media::mojom::AudioProcessingConfigPtr processing_config_;
  mojo::Remote<media::mojom::AudioProcessorControls> remote_controls_;
  std::unique_ptr<InputControllerTestHelper> helper_;
};

using SystemTimeInputControllerTestWithReferenceSignalProvider =
    TimeSourceInputControllerTestWithReferenceSignalProvider<
        base::test::TaskEnvironment::TimeSource::SYSTEM_TIME>;
using InputControllerTestWithReferenceSignalProvider =
    TimeSourceInputControllerTestWithReferenceSignalProvider<>;

TEST_F(InputControllerTestWithReferenceSignalProvider,
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

TEST_F(InputControllerTestWithReferenceSignalProvider,
       CreateWithAudioProcessingConfig_WithoutEnablingEffects) {
  SetupProcessingConfig(AudioProcessingType::kNone);

  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           loop.QuitClosure());
  loop.Run();

  // When all forms of audio processing are disabled, |controller_| should
  // ignore the pending AudioProcessorControls Receiver it received in its
  // ctor.
  EXPECT_FALSE(remote_controls_.is_connected());

  // InputController shouldn't spin up a processing thread if it's not needed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());

  // Test cleanup.
  controller_->Close();
}

TEST_F(InputControllerTestWithReferenceSignalProvider,
       CreateWithAudioProcessingConfig_VerifyFifoUsage) {
  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);

  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  EXPECT_TRUE(helper_->IsUsingProcessingThread());

  // Test cleanup.
  controller_->Close();
}

TEST_F(
    InputControllerTestWithReferenceSignalProvider,
    CreateWithAudioProcessingConfig_DoesNotListenForPlayoutReferenceIfNotRequired) {
  const std::string kOutputDeviceId = "0x123";

  EXPECT_CALL(*reference_signal_provider_, StartListening(_, _)).Times(0);
  EXPECT_CALL(*reference_signal_provider_, StopListening(_)).Times(0);

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

TEST_F(InputControllerTestWithReferenceSignalProvider,
       RecordBeforeSetOutputForAec) {
  const std::string kOutputDeviceId = "0x123";

  // Calling Record() will start listening to the "" device by default.
  EXPECT_CALL(*reference_signal_provider_, StartListening(_, ""))
      .Times(1)
      .WillOnce(Return(ReferenceOpenOutcome::SUCCESS));
  EXPECT_CALL(*reference_signal_provider_, StartListening(_, kOutputDeviceId))
      .Times(1)
      .WillOnce(Return(ReferenceOpenOutcome::SUCCESS));
  EXPECT_CALL(*reference_signal_provider_, StopListening(_)).Times(1);

  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);

  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  controller_->Record();
  controller_->SetOutputDeviceForAec(kOutputDeviceId);

  // InputController should offload processing to its own thread.
  EXPECT_TRUE(helper_->IsUsingProcessingThread());

  controller_->Close();

  // The processing thread should be stopped after controller has closed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());
}

TEST_F(InputControllerTestWithReferenceSignalProvider,
       RecordAfterSetOutputForAec) {
  const std::string kOutputDeviceId = "0x123";

  EXPECT_CALL(*reference_signal_provider_, StartListening(_, kOutputDeviceId))
      .Times(1)
      .WillOnce(Return(ReferenceOpenOutcome::SUCCESS));
  EXPECT_CALL(*reference_signal_provider_, StopListening(_)).Times(1);

  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  controller_->SetOutputDeviceForAec(kOutputDeviceId);
  controller_->Record();

  // InputController should offload processing to its own thread.
  EXPECT_TRUE(helper_->IsUsingProcessingThread());

  controller_->Close();

  // The processing thread should be stopped after controller has closed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());
}

TEST_F(InputControllerTestWithReferenceSignalProvider, FifoSize) {
  const std::string kOutputDeviceId = "0x123";
  EXPECT_CALL(*reference_signal_provider_, StartListening(_, kOutputDeviceId))
      .Times(1)
      .WillOnce(Return(ReferenceOpenOutcome::SUCCESS));
  EXPECT_CALL(*reference_signal_provider_, StopListening(_)).Times(1);

  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  controller_->SetOutputDeviceForAec(kOutputDeviceId);
  controller_->Record();

  EXPECT_TRUE(helper_->IsUsingProcessingThread());
  EXPECT_EQ(helper_->FifoSize(), InputController::kProcessingFifoSize);

  // InputController should offload processing to its own thread.
  EXPECT_TRUE(helper_->IsUsingProcessingThread());

  controller_->Close();
  EXPECT_FALSE(helper_->IsUsingProcessingThread());
}

TEST_F(InputControllerTestWithReferenceSignalProvider, ChangeOutputForAec) {
  const std::string kOutputDeviceId = "0x123";
  const std::string kOtherOutputDeviceId = "0x987";

  // Each output ID should receive one call to StartListening().
  EXPECT_CALL(*reference_signal_provider_, StartListening(_, kOutputDeviceId))
      .Times(1)
      .WillOnce(Return(ReferenceOpenOutcome::SUCCESS));
  EXPECT_CALL(*reference_signal_provider_,
              StartListening(_, kOtherOutputDeviceId))
      .Times(1)
      .WillOnce(Return(ReferenceOpenOutcome::SUCCESS));

  // StopListening() should be called once, regardless of how many ID changes.
  EXPECT_CALL(*reference_signal_provider_, StopListening(_)).Times(1);

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
TEST_F(SystemTimeInputControllerTestWithReferenceSignalProvider,
       CreateRecordAndClose) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  bool data_processed_by_fifo = false;

  // Test that the fifo is enabled.
  auto main_sequence = base::SequencedTaskRunner::GetCurrentDefault();
  auto verify_data_processed = [&data_processed_by_fifo, main_sequence]() {
    // Data should be processed on its own thread.
    EXPECT_FALSE(main_sequence->RunsTasksInCurrentSequence());

    data_processed_by_fifo = true;
  };

  helper_->AttachOnProcessedCallback(
      base::BindLambdaForTesting(verify_data_processed));

  ASSERT_TRUE(controller_.get());

  base::RunLoop loop;

  {
    // Wait for Write() to be called ten times.
    testing::InSequence s;
    EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _)).Times(Exactly(9));
    EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _))
        .Times(AtLeast(1))
        .WillOnce(InvokeWithoutArgs([&]() { loop.Quit(); }));
  }
  controller_->Record();

  // InputController should offload processing to its own thread if the
  // processing FIFO is enabled.
  EXPECT_TRUE(helper_->IsUsingProcessingThread());

  loop.Run();

  testing::Mock::VerifyAndClearExpectations(&sync_writer_);

  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();

  // The processing thread should be stopped after controller has closed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(data_processed_by_fifo);
}

TEST_F(InputControllerTestWithReferenceSignalProvider, ReferenceStreamError) {
  const std::string kOutputDeviceId = "0x123";
  EXPECT_CALL(*reference_signal_provider_, StartListening(_, kOutputDeviceId))
      .Times(1)
      .WillOnce(Return(ReferenceOpenOutcome::SUCCESS));
  EXPECT_CALL(*reference_signal_provider_, StopListening(_)).Times(1);

  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  controller_->SetOutputDeviceForAec(kOutputDeviceId);
  controller_->Record();
  EXPECT_TRUE(helper_->IsUsingProcessingThread());

  // Sending a ReferenceStreamError should result in an error being sent to the
  // EventHandler.
  EXPECT_CALL(event_handler_, OnError(InputController::REFERENCE_STREAM_ERROR));
  helper_->CallOnReferenceStreamError();

  controller_->Close();
}

class ParameterizedInputControllerUmaDelayTest
    : public SystemTimeInputControllerTestWithReferenceSignalProvider,
      public ::testing::WithParamInterface<DelayUmaTestData> {};

// Test a normal call sequence of create, record and close when audio processing
// is enabled but also verify that capture delays are recorded correctly using
// two different UMA names where the name depends on the type returned by the
// ReferenceSignalProvider.
// Based on
// SystemTimeInputControllerTestWithReferenceSignalProvider.CreateRecordAndClose.
TEST_P(ParameterizedInputControllerUmaDelayTest, CreateRecordAndClose) {
  const DelayUmaTestData& param = GetParam();

  EXPECT_CALL(event_handler_, OnCreated(_));
  // Use the provider_type from the parameter.
  EXPECT_CALL(*reference_signal_provider_, GetType())
      .WillOnce(testing::Return(param.provider_type));
  EXPECT_CALL(*reference_signal_provider_, StartListening(_, _)).Times(1);
  EXPECT_CALL(*reference_signal_provider_, StopListening(_)).Times(1);
  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  bool data_processed_by_fifo = false;

  // Test that the fifo is enabled.
  auto main_sequence = base::SequencedTaskRunner::GetCurrentDefault();
  auto verify_data_processed = [&data_processed_by_fifo, main_sequence]() {
    // Data should be processed on its own thread.
    EXPECT_FALSE(main_sequence->RunsTasksInCurrentSequence());

    data_processed_by_fifo = true;
  };

  helper_->AttachOnProcessedCallback(
      base::BindLambdaForTesting(verify_data_processed));

  ASSERT_TRUE(controller_.get());

  base::HistogramTester histogram_tester;
  base::RunLoop loop;

  {
    // Wait for Write() to be called ten times.
    testing::InSequence s;
    EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _)).Times(Exactly(9));
    EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _))
        .Times(AtLeast(1))
        .WillOnce(InvokeWithoutArgs([&]() { loop.Quit(); }));
  }
  controller_->Record();

  // InputController should offload processing to its own thread if the
  // processing FIFO is enabled.
  EXPECT_TRUE(helper_->IsUsingProcessingThread());

  loop.Run();

  testing::Mock::VerifyAndClearExpectations(&sync_writer_);

  EXPECT_CALL(sync_writer_, Close());
  controller_->Close();

  // Use the expected_uma_name from the parameter.
  histogram_tester.ExpectTotalCount(param.expected_uma_name, 10);

  // The processing thread should be stopped after controller has closed.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());

  EXPECT_TRUE(data_processed_by_fifo);
}

// Instantiate the UMA test suite with the two scenarios.
INSTANTIATE_TEST_SUITE_P(
    AECTypeDelayUMAs,
    ParameterizedInputControllerUmaDelayTest,
    ::testing::Values(
        DelayUmaTestData{ReferenceSignalProvider::Type::kOutputDeviceMixer,
                         "Media.Audio.InputController.Delay.ChromeWideAEC"},
        DelayUmaTestData{ReferenceSignalProvider::Type::kLoopbackReference,
                         "Media.Audio.InputController.Delay.LoopbackAEC"}),
    // Provide a human-readable name for each test instance.
    [](const testing::TestParamInfo<
        ParameterizedInputControllerUmaDelayTest::ParamType>& info) {
      switch (info.param.provider_type) {
        case ReferenceSignalProvider::Type::kOutputDeviceMixer:
          return "ChromeWideAEC";
        case ReferenceSignalProvider::Type::kLoopbackReference:
          return "LoopbackAEC";
        default:
          return "UnknownAEC";
      }
    });

template <>
void InputControllerTestWithReferenceSignalProvider::TestReferenceOpenError(
    ReferenceOpenOutcome reference_open_outcome,
    InputController::ErrorCode expected_error_code) {
  const std::string kOutputDeviceId = "0x123";
  // Make StartListening return an error.
  EXPECT_CALL(*reference_signal_provider_, StartListening(_, kOutputDeviceId))
      .Times(1)
      .WillOnce(Return(reference_open_outcome));
  EXPECT_CALL(*reference_signal_provider_, StopListening(_)).Times(1);

  SetupProcessingConfig(AudioProcessingType::kWithPlayoutReference);
  CreateAudioController();

  ASSERT_TRUE(controller_.get());

  controller_->SetOutputDeviceForAec(kOutputDeviceId);

  // Since StartListening will fail with an error, we should get an error on
  // Record().
  EXPECT_CALL(event_handler_, OnError(expected_error_code));
  controller_->Record();
  controller_->Close();
}

TEST_F(InputControllerTestWithReferenceSignalProvider,
       ReferenceStreamOpenError) {
  TestReferenceOpenError(ReferenceOpenOutcome::STREAM_OPEN_ERROR,
                         InputController::REFERENCE_STREAM_OPEN_ERROR);
}

TEST_F(InputControllerTestWithReferenceSignalProvider,
       ReferenceStreamPreviousError) {
  TestReferenceOpenError(ReferenceOpenOutcome::STREAM_PREVIOUS_ERROR,
                         InputController::REFERENCE_STREAM_ERROR);
}

TEST_F(InputControllerTestWithReferenceSignalProvider,
       ReferenceStreamCreateError) {
  TestReferenceOpenError(ReferenceOpenOutcome::STREAM_CREATE_ERROR,
                         InputController::REFERENCE_STREAM_CREATE_ERROR);
}

TEST_F(InputControllerTestWithReferenceSignalProvider,
       ReferenceStreamOpenDeviceInUseError) {
  TestReferenceOpenError(
      ReferenceOpenOutcome::STREAM_OPEN_DEVICE_IN_USE_ERROR,
      InputController::REFERENCE_STREAM_OPEN_DEVICE_IN_USE_ERROR);
}

TEST_F(InputControllerTestWithReferenceSignalProvider,
       ReferenceStreamOpenSystemPermissionsError) {
  TestReferenceOpenError(
      ReferenceOpenOutcome::STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR,
      InputController::REFERENCE_STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR);
}

TEST_F(InputControllerTestWithReferenceSignalProvider,
       CreateWithoutProcessingConfig_DoesNotUseFifo) {
  // This test simulates disabling ChromeWideEchoCancellation, in which case
  // both the AudioProcessingConfig and the ReferenceSignalProvider are null.

  // Destroy the ReferenceSignalProvider before moving it into the
  // InputController.
  reference_signal_provider_unique_.reset();
  // Additionally, we intentionally do not call SetupProcessingConfig(), leaving
  // the AudioProcessingConfig as null.
  CreateAudioController();

  ASSERT_TRUE(controller_.get());
  controller_->Record();

  // We are not doing echo cancellation, so we are not using the fifo.
  EXPECT_FALSE(helper_->IsUsingProcessingThread());

  // Test cleanup.
  controller_->Close();
}

#endif  // BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

}  // namespace audio
