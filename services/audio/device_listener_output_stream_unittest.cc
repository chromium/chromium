// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/device_listener_output_stream.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;

using media::AudioBus;
using media::AudioManager;
using media::AudioOutputStream;
using media::MockAudioSourceCallback;

using base::test::RunOnceClosure;

namespace audio {
namespace {

class MockAudioOutputStream : public AudioOutputStream {
 public:
  MockAudioOutputStream() = default;
  ~MockAudioOutputStream() override = default;

  void Start(AudioSourceCallback* callback) override {
    provided_callback_ = callback;
    StartCalled(provided_callback_);
  }

  MOCK_METHOD1(StartCalled, void(AudioSourceCallback*));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Flush, void());

  int SimulateOnMoreData(base::TimeDelta delay,
                         base::TimeTicks delay_timestamp,
                         const media::AudioGlitchInfo& glitch_info,
                         AudioBus* dest) {
    DCHECK(provided_callback_);
    return provided_callback_->OnMoreData(delay, delay_timestamp, glitch_info,
                                          dest);
  }

  void SimulateError(AudioSourceCallback::ErrorType error) {
    DCHECK(provided_callback_);
    provided_callback_->OnError(error);
  }

 private:
  raw_ptr<AudioOutputStream::AudioSourceCallback, DanglingUntriaged>
      provided_callback_ = nullptr;
};

class FakeAudioManagerForDeviceChange : public media::FakeAudioManager {
 public:
  FakeAudioManagerForDeviceChange()
      : FakeAudioManager(std::make_unique<media::TestAudioThread>(),
                         &fake_audio_log_factory_) {}
  ~FakeAudioManagerForDeviceChange() override = default;

  void AddOutputDeviceChangeListener(AudioDeviceListener* listener) override {
    media::FakeAudioManager::AddOutputDeviceChangeListener(listener);
    AddOutputDeviceChangeListenerCalled(listener);
  }

  void RemoveOutputDeviceChangeListener(
      AudioDeviceListener* listener) override {
    media::FakeAudioManager::RemoveOutputDeviceChangeListener(listener);
    RemoveOutputDeviceChangeListenerCalled(listener);
  }

  MOCK_METHOD1(AddOutputDeviceChangeListenerCalled,
               void(AudioManager::AudioDeviceListener*));
  MOCK_METHOD1(RemoveOutputDeviceChangeListenerCalled,
               void(AudioManager::AudioDeviceListener*));

  void SimulateDeviceChange() { NotifyAllOutputDeviceChangeListeners(); }

 private:
  media::FakeAudioLogFactory fake_audio_log_factory_;
};

class DeviceListenerOutputStreamTest : public ::testing::Test {
 public:
  using ErrorType = AudioOutputStream::AudioSourceCallback::ErrorType;

  MOCK_METHOD0(DeviceChangeCallbackCalled, void());
};

// Verifies DeviceListenerOutputStream forwards all calls to the wrapped stream.
TEST_F(DeviceListenerOutputStreamTest, DelegatesCallsToWrappedStream) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeAudioManagerForDeviceChange mock_audio_manager;
  MockAudioOutputStream mock_stream;
  MockAudioSourceCallback mock_callback;

  double volume = 200;
  base::TimeDelta delay = base::Milliseconds(30);
  base::TimeTicks delay_timestamp =
      base::TimeTicks() + base::Milliseconds(21);
  media::AudioGlitchInfo glitch_info{.duration = base::Seconds(5),
                                     .count = 123};
  std::unique_ptr<media::AudioBus> dest = media::AudioBus::Create(1, 128);

  InSequence sequence;
  EXPECT_CALL(mock_audio_manager, AddOutputDeviceChangeListenerCalled(_))
      .Times(1);
  EXPECT_CALL(mock_stream, Open()).Times(1);
  EXPECT_CALL(mock_stream, StartCalled(_)).Times(1);
  EXPECT_CALL(mock_callback,
              OnMoreData(delay, delay_timestamp, glitch_info, dest.get()));
  EXPECT_CALL(mock_stream, SetVolume(volume)).Times(1);
  EXPECT_CALL(mock_stream, GetVolume(&volume)).Times(1);
  EXPECT_CALL(mock_stream, Stop()).Times(1);
  EXPECT_CALL(mock_stream, Flush()).Times(1);
  EXPECT_CALL(mock_stream, Close()).Times(1);

  DeviceListenerOutputStream* stream_under_test =
      new DeviceListenerOutputStream(
          &mock_audio_manager, &mock_stream,
          base::BindRepeating(
              &DeviceListenerOutputStreamTest::DeviceChangeCallbackCalled,
              base::Unretained(this)));
  EXPECT_CALL(mock_audio_manager,
              RemoveOutputDeviceChangeListenerCalled(stream_under_test))
      .Times(1);

  stream_under_test->Open();
  stream_under_test->Start(&mock_callback);
  mock_stream.SimulateOnMoreData(delay, delay_timestamp, glitch_info,
                                 dest.get());
  stream_under_test->SetVolume(volume);
  stream_under_test->GetVolume(&volume);
  stream_under_test->Stop();
  stream_under_test->Flush();
  stream_under_test->Close();

  mock_audio_manager.Shutdown();
}

// Verifies DeviceListenerOutputStream calls device change callback on device
// change.
TEST_F(DeviceListenerOutputStreamTest, DeviceChangeNotification) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeAudioManagerForDeviceChange mock_audio_manager;
  MockAudioOutputStream mock_stream;
  MockAudioSourceCallback mock_callback;

  // |stream_under_test| should call the provided callback on device change.
  EXPECT_CALL(*this, DeviceChangeCallbackCalled()).Times(1);

  DeviceListenerOutputStream* stream_under_test =
      new DeviceListenerOutputStream(
          &mock_audio_manager, &mock_stream,
          base::BindRepeating(
              &DeviceListenerOutputStreamTest::DeviceChangeCallbackCalled,
              base::Unretained(this)));

  stream_under_test->Open();
  stream_under_test->Start(&mock_callback);

  mock_audio_manager.SimulateDeviceChange();

  stream_under_test->Stop();
  stream_under_test->Close();
  mock_audio_manager.Shutdown();
}

// Verifies DeviceListenerOutputStream calls device change callbacks on device
// change errors.
TEST_F(DeviceListenerOutputStreamTest, DeviceChangeError) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeAudioManagerForDeviceChange mock_audio_manager;
  MockAudioOutputStream mock_stream;
  MockAudioSourceCallback mock_callback;

  base::RunLoop loop;
  // |stream_under_test| should call the provided callback if it received a
  // device change error.
  EXPECT_CALL(*this, DeviceChangeCallbackCalled())
      .WillOnce(RunOnceClosure(loop.QuitClosure()));

  // |stream_under_test| should not forward device change errors.
  EXPECT_CALL(mock_callback, OnError(_)).Times(0);

  DeviceListenerOutputStream* stream_under_test =
      new DeviceListenerOutputStream(
          &mock_audio_manager, &mock_stream,
          base::BindRepeating(
              &DeviceListenerOutputStreamTest::DeviceChangeCallbackCalled,
              base::Unretained(this)));

  stream_under_test->Open();
  stream_under_test->Start(&mock_callback);

  // Simulate a device change error.
  mock_stream.SimulateError(ErrorType::kDeviceChange);

  loop.Run();

  stream_under_test->Stop();
  stream_under_test->Close();
  mock_audio_manager.Shutdown();
}

// Verifies DeviceListenerOutputStream forwards error callbacks.
TEST_F(DeviceListenerOutputStreamTest, UnknownError) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeAudioManagerForDeviceChange mock_audio_manager;
  MockAudioOutputStream mock_stream;
  MockAudioSourceCallback mock_callback;

  base::RunLoop loop;
  // |stream_under_test| should forward errors.
  EXPECT_CALL(*this, DeviceChangeCallbackCalled()).Times(0);
  EXPECT_CALL(mock_callback, OnError(ErrorType::kUnknown))
      .WillOnce(RunOnceClosure(loop.QuitClosure()));

  DeviceListenerOutputStream* stream_under_test =
      new DeviceListenerOutputStream(
          &mock_audio_manager, &mock_stream,
          base::BindRepeating(
              &DeviceListenerOutputStreamTest::DeviceChangeCallbackCalled,
              base::Unretained(this)));

  stream_under_test->Open();
  stream_under_test->Start(&mock_callback);

  // Simulate a device change error.
  mock_stream.SimulateError(ErrorType::kUnknown);

  loop.Run();

  stream_under_test->Stop();
  stream_under_test->Close();
  mock_audio_manager.Shutdown();
}

// Verifies DeviceListenerOutputStream elides error notifications during device
// changes.
TEST_F(DeviceListenerOutputStreamTest, ErrorThenDeviceChange) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeAudioManagerForDeviceChange mock_audio_manager;
  MockAudioOutputStream mock_stream;
  MockAudioSourceCallback mock_callback;

  base::RunLoop loop;
  // |stream_under_test| should call device change callback.
  EXPECT_CALL(*this, DeviceChangeCallbackCalled())
      .WillOnce(RunOnceClosure(loop.QuitClosure()));

  // |stream_under_test| should drop deferred errors.
  EXPECT_CALL(mock_callback, OnError(_)).Times(0);

  DeviceListenerOutputStream* stream_under_test =
      new DeviceListenerOutputStream(
          &mock_audio_manager, &mock_stream,
          base::BindRepeating(
              &DeviceListenerOutputStreamTest::DeviceChangeCallbackCalled,
              base::Unretained(this)));

  stream_under_test->Open();
  stream_under_test->Start(&mock_callback);

  // Simulate an error, followed by a device change.
  mock_stream.SimulateError(ErrorType::kUnknown);
  mock_audio_manager.SimulateDeviceChange();

  loop.Run();

  stream_under_test->Stop();
  stream_under_test->Close();
  mock_audio_manager.Shutdown();
}

// Verifies DeviceListenerOutputStream can be stopped after receiving an error.
TEST_F(DeviceListenerOutputStreamTest, ErrorThenStop) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  FakeAudioManagerForDeviceChange mock_audio_manager;
  MockAudioOutputStream mock_stream;
  MockAudioSourceCallback mock_callback;

  // |stream_under_test| should not call its error callback after it has been
  // stopped.
  EXPECT_CALL(mock_callback, OnError(_)).Times(0);

  DeviceListenerOutputStream* stream_under_test =
      new DeviceListenerOutputStream(
          &mock_audio_manager, &mock_stream,
          base::BindRepeating(
              &DeviceListenerOutputStreamTest::DeviceChangeCallbackCalled,
              base::Unretained(this)));

  stream_under_test->Open();
  stream_under_test->Start(&mock_callback);

  // Call stop() immediately after an error.
  mock_stream.SimulateError(ErrorType::kUnknown);
  stream_under_test->Stop();

  // Reporting the error should be delayed by 1s.
  task_environment.FastForwardUntilNoTasksRemain();

  stream_under_test->Close();
  mock_audio_manager.Shutdown();
}

}  // namespace
}  // namespace audio
