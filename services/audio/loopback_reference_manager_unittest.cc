// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_reference_manager.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "input_stream.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace audio {
namespace {

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
  MOCK_METHOD1(OnLogMessage, void(const std::string&));
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
};

class LoopbackReferenceManagerTest : public ::testing::Test {
 public:
  LoopbackReferenceManagerTest()
      : loopback_reference_manager_(&audio_manager_) {}

  LoopbackReferenceManagerTest(const LoopbackReferenceManagerTest&) = delete;
  LoopbackReferenceManagerTest& operator=(const LoopbackReferenceManagerTest&) =
      delete;

  ~LoopbackReferenceManagerTest() override { audio_manager_.Shutdown(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  StrictMock<LocalMockAudioManager> audio_manager_;
  LoopbackReferenceManager loopback_reference_manager_;

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
  return arg.channel(0)[0] == sample_value;
}

// There is no equality operator for AudioParameters, so we need to create a
// matcher instead.
MATCHER_P(AudioParamsMatches, expected, "") {
  return !(arg < expected) && !(expected < arg);
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
      loopback_reference_manager_.GetReferenceSignalProvider();

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
                                   loopback_device_id_, _))
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
  CHECK(mock_input_stream.captured_callback_);
  media::AudioInputStream::AudioInputCallback* audio_callback =
      *(mock_input_stream.captured_callback_);

  // Send some data, which should be delivered to the first listener.
  EXPECT_CALL(mock_listener_1,
              OnPlayoutData(FirstSampleEquals(111),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel(0)[0] = 111;
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
  audio_bus->channel(0)[0] = 222;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the first listener, which should not stop the loopback stream.
  reference_signal_provider->StopListening(&mock_listener_1);

  // Send some more data, which should only be delivered to the second listener.
  EXPECT_CALL(mock_listener_2,
              OnPlayoutData(FirstSampleEquals(333),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel(0)[0] = 333;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the second listener, this will stop the loopback stream.
  EXPECT_CALL(mock_input_stream, Stop());
  EXPECT_CALL(*mock_audio_log_raw_ptr, OnStopped());
  EXPECT_CALL(mock_input_stream, Close());
  EXPECT_CALL(*mock_audio_log_raw_ptr, OnClosed());
  reference_signal_provider->StopListening(&mock_listener_2);
}

TEST_F(LoopbackReferenceManagerTest,
       DistributesAudioToListenersSeparateProviders) {
  StrictMock<MockAudioInputStream> mock_input_stream;
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(loopback_params_);

  // These should use he same underlying stream.
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider_1 =
      loopback_reference_manager_.GetReferenceSignalProvider();
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider_2 =
      loopback_reference_manager_.GetReferenceSignalProvider();

  // Setup the expectations for starting the loopback stream. This should only
  // happen once, even with two providers.
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
      .WillOnce(Return(media::AudioInputStream::OpenOutcome::kSuccess));

  // Add the first listener to the first provider. This should create the
  // stream.
  StrictMock<MockListener> mock_listener_1;
  reference_signal_provider_1->StartListening(&mock_listener_1,
                                              output_device_id_);
  CHECK(mock_input_stream.captured_callback_);
  media::AudioInputStream::AudioInputCallback* audio_callback =
      *(mock_input_stream.captured_callback_);

  // Send some data, which should be delivered to the first listener.
  EXPECT_CALL(mock_listener_1,
              OnPlayoutData(FirstSampleEquals(111),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel(0)[0] = 111;
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
  audio_bus->channel(0)[0] = 222;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the listener from the first provider. The stream should not stop.
  reference_signal_provider_1->StopListening(&mock_listener_1);

  // Send some more data, which should only be delivered to the listener on the
  // second provider.
  EXPECT_CALL(mock_listener_2,
              OnPlayoutData(FirstSampleEquals(333),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel(0)[0] = 333;
  audio_callback->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the listener from the second provider. This should stop the loopback
  // stream.
  EXPECT_CALL(mock_input_stream, Stop());
  EXPECT_CALL(mock_input_stream, Close());
  reference_signal_provider_2->StopListening(&mock_listener_2);
}

TEST_F(LoopbackReferenceManagerTest,
       OpensAndClosesStreamWithMultipleListenCycles) {
  StrictMock<MockAudioInputStream> mock_input_stream_1;

  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(loopback_params_);
  std::unique_ptr<ReferenceSignalProvider> reference_signal_provider =
      loopback_reference_manager_.GetReferenceSignalProvider();
  StrictMock<MockListener> mock_listener;

  // --- First listen cycle ---
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
      .WillOnce(Return(&mock_input_stream_1));
  EXPECT_CALL(mock_input_stream_1, Open())
      .WillOnce(Return(media::AudioInputStream::OpenOutcome::kSuccess));

  reference_signal_provider->StartListening(&mock_listener, output_device_id_);
  CHECK(mock_input_stream_1.captured_callback_);
  media::AudioInputStream::AudioInputCallback* audio_callback_1 =
      *(mock_input_stream_1.captured_callback_);

  // Send some data to the listener.
  EXPECT_CALL(mock_listener,
              OnPlayoutData(FirstSampleEquals(111),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel(0)[0] = 111;
  audio_callback_1->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the listener, which closes the new stream.
  EXPECT_CALL(mock_input_stream_1, Stop());
  EXPECT_CALL(mock_input_stream_1, Close());
  reference_signal_provider->StopListening(&mock_listener);

  // --- Second listen cycle ---
  StrictMock<MockAudioInputStream> mock_input_stream_2;
  EXPECT_CALL(audio_manager_, GetInputStreamParameters(loopback_device_id_))
      .WillOnce(Return(loopback_params_));
  EXPECT_CALL(audio_manager_,
              CreateAudioLog(
                  media::AudioLogFactory::AudioComponent::kAudioInputController,
                  1000001))  // Component ID increments
      .WillOnce(Return(std::make_unique<NiceMock<MockAudioLog>>()));
  EXPECT_CALL(audio_manager_,
              MakeAudioInputStream(AudioParamsMatches(loopback_params_),
                                   loopback_device_id_, _))
      .WillOnce(Return(&mock_input_stream_2));
  EXPECT_CALL(mock_input_stream_2, Open())
      .WillOnce(Return(media::AudioInputStream::OpenOutcome::kSuccess));

  reference_signal_provider->StartListening(&mock_listener, output_device_id_);
  CHECK(mock_input_stream_2.captured_callback_);
  media::AudioInputStream::AudioInputCallback* audio_callback_2 =
      *(mock_input_stream_2.captured_callback_);

  // Send some data to the listener again, via the new stream.
  EXPECT_CALL(mock_listener,
              OnPlayoutData(FirstSampleEquals(222),
                            loopback_params_.sample_rate(), base::TimeDelta()));
  audio_bus->channel(0)[0] = 222;
  audio_callback_2->OnData(audio_bus.get(), base::TimeTicks::Now(), 0, {});

  // Remove the listener again, which closes the new stream.
  EXPECT_CALL(mock_input_stream_2, Stop());
  EXPECT_CALL(mock_input_stream_2, Close());
  reference_signal_provider->StopListening(&mock_listener);
}

}  // namespace
}  // namespace audio
