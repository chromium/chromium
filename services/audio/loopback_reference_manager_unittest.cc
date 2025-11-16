// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_reference_manager.h"

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "input_controller.h"
#include "input_stream.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace audio {
namespace {

using ReferenceOpenOutcome = ReferenceSignalProvider::ReferenceOpenOutcome;
using OpenOutcome = media::AudioInputStream::OpenOutcome;
using AudioInputCallback = media::AudioInputStream::AudioInputCallback;

class MockAudioLog : public media::AudioLog {
 public:
  MockAudioLog() {}
  MOCK_METHOD2(OnCreated,
               void(const media::AudioParameters& params,
                    const std::string& device_id));

  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnSetVolume, void(double));
  MOCK_METHOD1(OnProcessingStateChanged, void(const std::string&));
  void OnLogMessage(const std::string& message) override {
    logged_messages += message;
  }
  std::string logged_messages = "";
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

class LocalMockAudioManager : public media::MockAudioManager {
 public:
  LocalMockAudioManager()
      : media::MockAudioManager(
            std::make_unique<media::TestAudioThread>(false)) {}
  ~LocalMockAudioManager() override = default;

  MOCK_METHOD(media::AudioParameters,
              GetInputStreamParameters,
              (const std::string& device_id),
              (override));
  MOCK_METHOD(media::AudioInputStream*,
              MakeAudioInputStream,
              (const media::AudioParameters& params,
               const std::string& device_id,
               const LogCallback& log_callback),
              (override));
  MOCK_METHOD(std::unique_ptr<media::AudioLog>,
              CreateAudioLog,
              (media::AudioLogFactory::AudioComponent component,
               int component_id),
              (override));
};

class MockListener : public ReferenceOutput::Listener {
 public:
  MockListener() = default;
  ~MockListener() override = default;

  MOCK_METHOD(void,
              OnPlayoutData,
              (const media::AudioBus&, int, base::TimeDelta),
              (override));
  MOCK_METHOD(void, OnReferenceStreamError, (), (override));
};

class LoopbackReferenceManagerTest : public ::testing::Test {
 public:
  LoopbackReferenceManagerTest()
      : loopback_reference_manager_(
            std::make_unique<LoopbackReferenceManager>(&audio_manager_)) {}

  LoopbackReferenceManagerTest(const LoopbackReferenceManagerTest&) = delete;
  LoopbackReferenceManagerTest& operator=(const LoopbackReferenceManagerTest&) =
      delete;

  ~LoopbackReferenceManagerTest() override {
    // Ensure the loopback manager is destroyed before audio_manager_
    loopback_reference_manager_.reset();
    audio_manager_.Shutdown();
  }

 protected:
  // Helper to quickly setup the mock expectations for creating a new loopback
  // stream.
  std::unique_ptr<StrictMock<MockAudioInputStream>> ExpectCreateLoopbackStream(
      int component_id);

  // Used for testing that a specific OpenOutcome is translated to a
  // ReferenceOpenOutcome.
  void TestStreamOpenError(
      OpenOutcome loopback_open_outcome,
      ReferenceOpenOutcome expected_reference_open_outcome);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  scoped_refptr<base::SingleThreadTaskRunner> audio_thread_task_runner_ =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  StrictMock<LocalMockAudioManager> audio_manager_;
  std::unique_ptr<LoopbackReferenceManager> loopback_reference_manager_;

  const media::AudioParameters loopback_params_ =
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Stereo(),
                             48000,
                             480);
  const std::string loopback_device_id_ =
      media::AudioDeviceDescription::kLoopbackAllDevicesId;
  const std::string output_device_id_ =
      media::AudioDeviceDescription::kDefaultDeviceId;
};

// Simple matcher for verifying AudioBus data.
MATCHER_P(FirstSampleEquals, sample_value, "") {
  return arg.channel_span(0)[0] == sample_value;
}

// There is no equality operator for AudioParameters, so we need to create a
// matcher instead.
MATCHER_P(AudioParamsMatches, expected, "") {
  return !(arg < expected) && !(expected < arg);
}

// Matcher which sends a test message on a log callback.
MATCHER_P(RunLogCallback, test_message, "") {
  arg.Run(test_message);
  return true;
}

std::unique_ptr<StrictMock<MockAudioInputStream>>
LoopbackReferenceManagerTest::ExpectCreateLoopbackStream(int component_id) {
  auto mock_input_stream = std::make_unique<StrictMock<MockAudioInputStream>>();
  EXPECT_CALL(audio_manager_, GetInputStreamParameters(loopback_device_id_))
      .WillOnce(Return(loopback_params_));
  EXPECT_CALL(audio_manager_,
              CreateAudioLog(
                  media::AudioLogFactory::AudioComponent::kAudioInputController,
                  component_id))
      .WillOnce(Return(std::make_unique<NiceMock<MockAudioLog>>()));
  EXPECT_CALL(audio_manager_,
              MakeAudioInputStream(AudioParamsMatches(loopback_params_),
                                   loopback_device_id_, _))
      .WillOnce(Return(mock_input_stream.get()));
  EXPECT_CALL(*mock_input_stream, Open())
      .WillOnce(Return(media::AudioInputStream::OpenOutcome::kSuccess));
  EXPECT_CALL(*mock_input_stream, Stop());
  EXPECT_CALL(*mock_input_stream, Close());
  return mock_input_stream;
}

TEST_F(LoopbackReferenceManagerTest, DistributesAudioToListenersSameProvider) {
  StrictMock<MockAudioInputStream> mock_input_stream;
  std::unique_ptr<StrictMock<MockAudioLog>> mock_audio_log =
      std::make_unique<StrictMock<MockAudioLog>>();
  // Retain a pointer to the mock audio log to expect calls to it.
  StrictMock<MockAudioLog>* mock_audio_log_raw_ptr = mock_audio_log.get();
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(loopback_params_);
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider =
      loopback_reference_manager_->GetReferenceSignalProvider();

  // Setup the expectations for starting the loopback stream.
  EXPECT_CALL(audio_manager_, GetInputStreamParameters(loopback_device_id_))
      .WillOnce(Return(loopback_params_));
  EXPECT_CALL(audio_manager_,
              CreateAudioLog(
                  media::AudioLogFactory::AudioComponent::kAudioInputController,
                  1000000))
      .WillOnce(Return(std::move(mock_audio_log)));
  EXPECT_CALL(audio_manager_,
              MakeAudioInputStream(AudioParamsMatches(loopback_params_),
                                   loopback_device_id_,
                                   RunLogCallback("LOG CALLBACK TEST MESSAGE")))
      .WillOnce(Return(&mock_input_stream));
  EXPECT_CALL(mock_input_stream, Open())
      .WillOnce(Return(media::AudioInputStream::OpenOutcome::kSuccess));
  EXPECT_CALL(
      *mock_audio_log_raw_ptr,
      OnCreated(AudioParamsMatches(loopback_params_), loopback_device_id_));
  EXPECT_CALL(*mock_audio_log_raw_ptr, OnStarted());
  // MockAudioInputStream::Start() is not a mocked function, it is implemented
  // to capture the callback, so we do not need to EXPECT it.

  // Add the first listener. This should create the stream.
  StrictMock<MockListener> mock_listener_1;
  reference_signal_provider->StartListening(&mock_listener_1,
                                            output_device_id_);
  EXPECT_THAT(mock_audio_log_raw_ptr->logged_messages,
              ::testing::HasSubstr("LOG CALLBACK TEST MESSAGE"));

  CHECK(mock_input_stream.captured_callback_);
  AudioInputCallback* audio_callback = *(mock_input_stream.captured_callback_);

  // Send some data, which should be delivered to the first listener.
  EXPECT_CALL(mock_listener_1,
              OnPlayoutData(FirstSampleEquals(111),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel_span(0)[0] = 111;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Add another listener. This shouldn't create a new stream.
  StrictMock<MockListener> mock_listener_2;
  reference_signal_provider->StartListening(&mock_listener_2,
                                            output_device_id_);

  // Send some more data, which should be delivered to both listeners
  EXPECT_CALL(mock_listener_1,
              OnPlayoutData(FirstSampleEquals(222),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  EXPECT_CALL(mock_listener_2,
              OnPlayoutData(FirstSampleEquals(222),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel_span(0)[0] = 222;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the first listener, which should not stop the loopback stream.
  reference_signal_provider->StopListening(&mock_listener_1);

  // Send some more data, which should only be delivered to the second listener.
  EXPECT_CALL(mock_listener_2,
              OnPlayoutData(FirstSampleEquals(333),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel_span(0)[0] = 333;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the second listener, this will stop the loopback stream.
  EXPECT_CALL(mock_input_stream, Stop());
  EXPECT_CALL(*mock_audio_log_raw_ptr, OnStopped());
  EXPECT_CALL(mock_input_stream, Close());
  EXPECT_CALL(*mock_audio_log_raw_ptr, OnClosed());
  reference_signal_provider->StopListening(&mock_listener_2);
  // Superfluous calls to StopListening should be no-ops.
  reference_signal_provider->StopListening(&mock_listener_2);
}

TEST_F(LoopbackReferenceManagerTest,
       DistributesAudioToListenersSeparateProviders) {
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(loopback_params_);

  // These should use he same underlying stream.
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider_1 =
      loopback_reference_manager_->GetReferenceSignalProvider();
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider_2 =
      loopback_reference_manager_->GetReferenceSignalProvider();

  // Setup the expectations for starting the loopback stream. This should only
  // happen once, even with two providers.
  std::unique_ptr<StrictMock<MockAudioInputStream>> mock_input_stream =
      ExpectCreateLoopbackStream(1000000);

  // Add the first listener to the first provider. This should create the
  // stream.
  StrictMock<MockListener> mock_listener_1;
  reference_signal_provider_1->StartListening(&mock_listener_1,
                                              output_device_id_);
  CHECK(mock_input_stream->captured_callback_);
  AudioInputCallback* audio_callback = *(mock_input_stream->captured_callback_);

  // Send some data, which should be delivered to the first listener.
  EXPECT_CALL(mock_listener_1,
              OnPlayoutData(FirstSampleEquals(111),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel_span(0)[0] = 111;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Add a listener to the second provider. This shouldn't create a new stream.
  StrictMock<MockListener> mock_listener_2;
  reference_signal_provider_2->StartListening(&mock_listener_2,
                                              output_device_id_);

  // Send some more data, which should be delivered to both listeners.
  EXPECT_CALL(mock_listener_1,
              OnPlayoutData(FirstSampleEquals(222),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  EXPECT_CALL(mock_listener_2,
              OnPlayoutData(FirstSampleEquals(222),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel_span(0)[0] = 222;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the listener from the first provider. The stream should not stop.
  reference_signal_provider_1->StopListening(&mock_listener_1);

  // Send some more data, which should only be delivered to the listener on the
  // second provider.
  EXPECT_CALL(mock_listener_2,
              OnPlayoutData(FirstSampleEquals(333),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel_span(0)[0] = 333;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the listener from the second provider. This should stop the loopback
  // stream.
  reference_signal_provider_2->StopListening(&mock_listener_2);
}

TEST_F(LoopbackReferenceManagerTest,
       OpensAndClosesStreamWithMultipleListenCycles) {
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(loopback_params_);
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider =
      loopback_reference_manager_->GetReferenceSignalProvider();
  StrictMock<MockListener> mock_listener;

  // --- First listen cycle ---
  std::unique_ptr<StrictMock<MockAudioInputStream>> mock_input_stream_1 =
      ExpectCreateLoopbackStream(1000000);

  reference_signal_provider->StartListening(&mock_listener, output_device_id_);
  CHECK(mock_input_stream_1->captured_callback_);
  AudioInputCallback* audio_callback_1 =
      *(mock_input_stream_1->captured_callback_);

  // Send some data to the listener.
  EXPECT_CALL(mock_listener,
              OnPlayoutData(FirstSampleEquals(111),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel_span(0)[0] = 111;
  audio_callback_1->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the listener, which closes the new stream.
  reference_signal_provider->StopListening(&mock_listener);

  // --- Second listen cycle ---
  std::unique_ptr<StrictMock<MockAudioInputStream>> mock_input_stream_2 =
      ExpectCreateLoopbackStream(1000001);

  reference_signal_provider->StartListening(&mock_listener, output_device_id_);
  CHECK(mock_input_stream_2->captured_callback_);
  AudioInputCallback* audio_callback_2 =
      *(mock_input_stream_2->captured_callback_);

  // Send some data to the listener again, via the new stream.
  EXPECT_CALL(mock_listener,
              OnPlayoutData(FirstSampleEquals(222),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel_span(0)[0] = 222;
  audio_callback_2->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the listener again, which closes the new stream.
  reference_signal_provider->StopListening(&mock_listener);
}

TEST_F(LoopbackReferenceManagerTest, StreamCreateError) {
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider =
      loopback_reference_manager_->GetReferenceSignalProvider();
  base::HistogramTester histogram_tester;

  EXPECT_CALL(audio_manager_, GetInputStreamParameters(loopback_device_id_))
      .WillOnce(Return(loopback_params_));
  EXPECT_CALL(audio_manager_,
              CreateAudioLog(
                  media::AudioLogFactory::AudioComponent::kAudioInputController,
                  1000000))
      .WillOnce(Return(std::make_unique<NiceMock<MockAudioLog>>()));
  // Fail to create the loopback stream
  EXPECT_CALL(audio_manager_,
              MakeAudioInputStream(AudioParamsMatches(loopback_params_),
                                   loopback_device_id_, _))
      .WillOnce(Return(nullptr));

  // Attempt to start the stream but since MakeAudioInputStream returns nullptr,
  // StartListening should return an error.
  StrictMock<MockListener> mock_listener;
  ReferenceOpenOutcome outcome = reference_signal_provider->StartListening(
      &mock_listener, output_device_id_);
  EXPECT_EQ(outcome, ReferenceOpenOutcome::STREAM_CREATE_ERROR);
  histogram_tester.ExpectUniqueSample(
      "Media.Audio.LoopbackReference.OpenResult",
      static_cast<int>(ReferenceOpenOutcome::STREAM_CREATE_ERROR), 1);
  histogram_tester.ExpectTotalCount(
      "Media.Audio.LoopbackReference.HadRuntimeError", 0);
  // Destroy the loopback manager explicitly to trigger the destructor which
  // should generate a histogram.
  loopback_reference_manager_.reset();
  histogram_tester.ExpectBucketCount(
      "Media.Audio.LoopbackReference.HadRuntimeError", false, 1);
}

void LoopbackReferenceManagerTest::TestStreamOpenError(
    OpenOutcome loopback_open_outcome,
    ReferenceOpenOutcome expected_reference_open_outcome) {
  StrictMock<MockAudioInputStream> mock_input_stream;
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider =
      loopback_reference_manager_->GetReferenceSignalProvider();
  base::HistogramTester histogram_tester;

  EXPECT_CALL(audio_manager_, GetInputStreamParameters(loopback_device_id_))
      .WillOnce(Return(loopback_params_));
  EXPECT_CALL(audio_manager_,
              CreateAudioLog(
                  media::AudioLogFactory::AudioComponent::kAudioInputController,
                  1000000))
      .WillOnce(Return(std::make_unique<NiceMock<MockAudioLog>>()));
  EXPECT_CALL(audio_manager_,
              MakeAudioInputStream(AudioParamsMatches(loopback_params_),
                                   loopback_device_id_, _))
      .WillOnce(Return(&mock_input_stream));
  EXPECT_CALL(mock_input_stream, Open())
      .WillOnce(Return(loopback_open_outcome));
  EXPECT_CALL(mock_input_stream, Close());

  // Attempt to start the stream but since mock_input_strean.Open() returns an
  // error, StartListening should return an error and call
  // mock_input_strean.Close().
  StrictMock<MockListener> mock_listener;
  ReferenceOpenOutcome outcome = reference_signal_provider->StartListening(
      &mock_listener, output_device_id_);
  EXPECT_EQ(outcome, expected_reference_open_outcome);
  histogram_tester.ExpectUniqueSample(
      "Media.Audio.LoopbackReference.OpenResult",
      static_cast<int>(expected_reference_open_outcome), 1);
  histogram_tester.ExpectTotalCount(
      "Media.Audio.LoopbackReference.HadRuntimeError", 0);
}

TEST_F(LoopbackReferenceManagerTest, StreamOpenError) {
  TestStreamOpenError(OpenOutcome::kFailed,
                      ReferenceOpenOutcome::STREAM_OPEN_ERROR);
}

TEST_F(LoopbackReferenceManagerTest, StreamOpenSystemPermissionsError) {
  TestStreamOpenError(
      OpenOutcome::kFailedSystemPermissions,
      ReferenceOpenOutcome::STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR);
}

TEST_F(LoopbackReferenceManagerTest, StreamOpenDeviceInUseError) {
  TestStreamOpenError(OpenOutcome::kFailedInUse,
                      ReferenceOpenOutcome::STREAM_OPEN_DEVICE_IN_USE_ERROR);
}

TEST_F(LoopbackReferenceManagerTest, OnReferenceStreamError) {
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(loopback_params_);
  base::HistogramTester histogram_tester;

  // These should use he same underlying core.
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider_1 =
      loopback_reference_manager_->GetReferenceSignalProvider();
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider_2 =
      loopback_reference_manager_->GetReferenceSignalProvider();

  // Setup the expectations for starting the loopback stream.
  std::unique_ptr<StrictMock<MockAudioInputStream>> mock_input_stream_1 =
      ExpectCreateLoopbackStream(1000000);

  // Add the first listener to the first provider. This should create the
  // stream.
  StrictMock<MockListener> mock_listener_1;
  EXPECT_EQ(ReferenceOpenOutcome::SUCCESS,
            reference_signal_provider_1->StartListening(&mock_listener_1,
                                                        output_device_id_));
  CHECK(mock_input_stream_1->captured_callback_);
  AudioInputCallback* audio_callback =
      *(mock_input_stream_1->captured_callback_);

  // Report an error, causing the core to be invalidated, and scheduled to be
  // deleted. Note that this will not normally be called on the main thread, but
  // we do so in this test to check the various cases in which the scheduled
  // deletion interacts with StartListening() and GetReferenceSignalProvider().
  audio_callback->OnError();

  // We have had an error but destruction of the core has not yet occurred, so
  // listening will be successful.
  StrictMock<MockListener> mock_listener_2;
  EXPECT_EQ(ReferenceOpenOutcome::SUCCESS,
            reference_signal_provider_2->StartListening(&mock_listener_2,
                                                        output_device_id_));

  // Fast-forwarding will run the scheduled deletion on the main thread, which
  // should send the error to the listeners.
  EXPECT_CALL(mock_listener_1, OnReferenceStreamError());
  EXPECT_CALL(mock_listener_2, OnReferenceStreamError());
  task_environment_.FastForwardBy(base::TimeDelta());

  {
    // Trying to listen to the providers with broken cores will now fail.
    StrictMock<MockListener> temp_mock_listener;
    EXPECT_EQ(ReferenceOpenOutcome::STREAM_PREVIOUS_ERROR,
              reference_signal_provider_1->StartListening(&temp_mock_listener,
                                                          output_device_id_));
  }

  // Get a new ReferenceSignalProvider after the error has been processed.
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider_3 =
      loopback_reference_manager_->GetReferenceSignalProvider();

  std::unique_ptr<StrictMock<MockAudioInputStream>> mock_input_stream_2 =
      ExpectCreateLoopbackStream(1000001);
  {
    // reference_signal_provider_3 was gotten after the error, so it should have
    // a new core.
    StrictMock<MockListener> temp_mock_listener;
    EXPECT_EQ(ReferenceOpenOutcome::SUCCESS,
              reference_signal_provider_3->StartListening(&temp_mock_listener,
                                                          output_device_id_));
    reference_signal_provider_3->StopListening(&temp_mock_listener);
  }

  histogram_tester.ExpectBucketCount(
      "Media.Audio.LoopbackReference.OpenResult",
      static_cast<int>(ReferenceOpenOutcome::SUCCESS), 3);
  histogram_tester.ExpectBucketCount(
      "Media.Audio.LoopbackReference.OpenResult",
      static_cast<int>(ReferenceOpenOutcome::STREAM_PREVIOUS_ERROR), 1);
  histogram_tester.ExpectUniqueSample(
      "Media.Audio.LoopbackReference.HadRuntimeError", true, 1);
}

TEST_F(LoopbackReferenceManagerTest, StopListeningAfterOnError) {
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(loopback_params_);

  // These should use he same underlying core.
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider =
      loopback_reference_manager_->GetReferenceSignalProvider();

  // Setup the expectations for starting the loopback stream.
  std::unique_ptr<StrictMock<MockAudioInputStream>> mock_input_stream =
      ExpectCreateLoopbackStream(1000000);

  // Add the first listener to the first provider. This should create the
  // stream.
  StrictMock<MockListener> mock_listener;
  EXPECT_EQ(ReferenceOpenOutcome::SUCCESS,
            reference_signal_provider->StartListening(&mock_listener,
                                                      output_device_id_));
  CHECK(mock_input_stream->captured_callback_);
  AudioInputCallback* audio_callback = *(mock_input_stream->captured_callback_);

  // Send an error.
  audio_callback->OnError();

  // Stop listening to the provider before the error has been processed.
  reference_signal_provider->StopListening(&mock_listener);

  // mock_listener will not get an error, because it has already stopped
  // listening.
  task_environment_.FastForwardBy(base::TimeDelta());
}

// Implementation of ReferenceOutput::Listener that expects OnPlayoutData and
// OnReferenceStreamError to be called on specific threads.
class ThreadedReferenceListener : public ReferenceOutput::Listener {
 public:
  ThreadedReferenceListener(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> audio_thread_task_runner,
      base::WaitableEvent* on_playout_data_event)
      : main_task_runner_(main_task_runner),
        audio_thread_task_runner_(audio_thread_task_runner),
        on_playout_data_event_(on_playout_data_event) {}
  ~ThreadedReferenceListener() override = default;

  MOCK_METHOD(void,
              MockOnPlayoutData,
              (const media::AudioBus&, int, base::TimeDelta),
              ());
  MOCK_METHOD(void, MockOnReferenceStreamError, (), ());

  void OnPlayoutData(const media::AudioBus& audio_bus,
                     int sample_rate,
                     base::TimeDelta audio_delay) final {
    // If this is called on the main thread, the test might deadlock.
    CHECK(audio_thread_task_runner_->BelongsToCurrentThread());
    MockOnPlayoutData(audio_bus, sample_rate, audio_delay);
    on_playout_data_event_->Signal();
  }

  void OnReferenceStreamError() final {
    CHECK(main_task_runner_->BelongsToCurrentThread());
    MockOnReferenceStreamError();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_thread_task_runner_;
  raw_ptr<base::WaitableEvent> on_playout_data_event_;
};

TEST_F(LoopbackReferenceManagerTest, DeliversAudioOnAudioThread) {
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(loopback_params_);

  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider =
      loopback_reference_manager_->GetReferenceSignalProvider();

  // Setup the expectations for starting the loopback stream.
  std::unique_ptr<StrictMock<MockAudioInputStream>> mock_input_stream =
      ExpectCreateLoopbackStream(1000000);

  // Create a listener which checks that the methods are called on the right
  // threads.
  base::WaitableEvent on_playout_data_event;
  StrictMock<ThreadedReferenceListener> threaded_listener(
      main_task_runner_, audio_thread_task_runner_, &on_playout_data_event);

  // Add the first listener to the first provider. This should create the
  // stream.
  reference_signal_provider->StartListening(&threaded_listener,
                                            output_device_id_);
  CHECK(mock_input_stream->captured_callback_);
  AudioInputCallback* audio_callback = *(mock_input_stream->captured_callback_);

  audio_bus->channel_span(0)[0] = 111;
  EXPECT_CALL(
      threaded_listener,
      MockOnPlayoutData(FirstSampleEquals(111), loopback_params_.sample_rate(),
                        base::TimeDelta()));
  audio_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioInputCallback::OnData,
                     base::Unretained(audio_callback), audio_bus.get(),
                     base::TimeTicks::Now(), 0, media::AudioGlitchInfo()));

  // on_playout_data_event will be signaled when OnPlayoutData is called.
  // TimedWait returns true when the signal is called, and false on timeout.
  EXPECT_TRUE(on_playout_data_event.TimedWait(base::Seconds(1)));

  reference_signal_provider->StopListening(&threaded_listener);
}

TEST_F(LoopbackReferenceManagerTest, DeliversErrorsOnMainThread) {
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(loopback_params_);

  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider =
      loopback_reference_manager_->GetReferenceSignalProvider();

  // Setup the expectations for starting the loopback stream.
  std::unique_ptr<StrictMock<MockAudioInputStream>> mock_input_stream =
      ExpectCreateLoopbackStream(1000000);

  // Create a listener which checks that the methods are called on the right
  // threads.
  base::WaitableEvent on_playout_data_event;
  StrictMock<ThreadedReferenceListener> threaded_listener(
      main_task_runner_, audio_thread_task_runner_, &on_playout_data_event);

  // Add the first listener to the first provider. This should create the
  // stream.
  reference_signal_provider->StartListening(&threaded_listener,
                                            output_device_id_);
  CHECK(mock_input_stream->captured_callback_);
  AudioInputCallback* audio_callback = *(mock_input_stream->captured_callback_);

  base::WaitableEvent fired_error_event;
  audio_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WaitableEvent* fired_error_event,
                        AudioInputCallback* audio_callback) {
                       audio_callback->OnError();
                       fired_error_event->Signal();
                     },
                     &fired_error_event, audio_callback));

  // TimedWait returns true when the signal is called, and false on timeout.
  EXPECT_TRUE(fired_error_event.TimedWait(base::Seconds(1)));

  // At this point OnError has been called on the audio thread, scheduling the
  // error on the main thread. Fast-forwarding will run the deletion on the main
  // thread, which  should send the error to the listener.
  EXPECT_CALL(threaded_listener, MockOnReferenceStreamError());
  task_environment_.FastForwardBy(base::TimeDelta());
}

}  // namespace
}  // namespace audio
