// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_output_resampler.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArrayArgument;
using media::AudioBus;
using media::AudioInputStream;
using media::AudioManager;
using media::AudioManagerBase;
using media::AudioOutputDispatcher;
using media::AudioOutputProxy;
using media::AudioOutputStream;
using media::AudioParameters;
using media::FakeAudioOutputStream;
using media::TestAudioThread;

namespace {

static const int kTestCloseDelayMs = 10;

// Delay between callbacks to AudioSourceCallback::OnMoreData.
static const int kOnMoreDataCallbackDelayMs = 10;

// Let start run long enough for many OnMoreData callbacks to occur.
static const int kStartRunTimeMs = kOnMoreDataCallbackDelayMs * 10;

// Dummy function.
std::unique_ptr<media::AudioDebugRecorder> RegisterDebugRecording(
    const media::AudioParameters& params) {
  return nullptr;
}

class MockAudioOutputStream : public AudioOutputStream {
 public:
  MockAudioOutputStream(AudioManagerBase* manager,
                        const AudioParameters& params)
      : start_called_(false),
        stop_called_(false),
        params_(params),
        fake_output_stream_(
            FakeAudioOutputStream::MakeFakeStream(manager, params_)) {
  }

  void Start(AudioSourceCallback* callback) override {
    start_called_ = true;
    fake_output_stream_->Start(callback);
  }

  void Stop() override {
    stop_called_ = true;
    fake_output_stream_->Stop();
  }

  ~MockAudioOutputStream() override = default;

  bool start_called() { return start_called_; }
  bool stop_called() { return stop_called_; }

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Flush, void());

 private:
  bool start_called_;
  bool stop_called_;
  AudioParameters params_;
  std::unique_ptr<AudioOutputStream> fake_output_stream_;
};

class CallbackExposingMockOutputStream : public AudioOutputStream {
 public:
  CallbackExposingMockOutputStream() = default;

  void Start(AudioSourceCallback* callback) override { callback_ = callback; }

  void Stop() override { callback_.reset(); }

  ~CallbackExposingMockOutputStream() override = default;

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Flush, void());

  std::optional<AudioOutputStream::AudioSourceCallback*> GetCallback() {
    return callback_;
  }

 private:
  std::optional<AudioOutputStream::AudioSourceCallback*> callback_;
};

class MockAudioManager : public AudioManagerBase {
 public:
  MockAudioManager()
      : AudioManagerBase(std::make_unique<TestAudioThread>(),
                         &fake_audio_log_factory_) {}
  ~MockAudioManager() override { Shutdown(); }

  MOCK_METHOD3(MakeAudioOutputStream,
               AudioOutputStream*(const AudioParameters& params,
                                  const std::string& device_id,
                                  const LogCallback& log_callback));
  MOCK_METHOD2(MakeAudioOutputStreamProxy, AudioOutputStream*(
      const AudioParameters& params,
      const std::string& device_id));
  MOCK_METHOD3(MakeAudioInputStream,
               AudioInputStream*(const AudioParameters& params,
                                 const std::string& device_id,
                                 const LogCallback& log_callback));
  MOCK_METHOD0(GetTaskRunner, scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(GetWorkerTaskRunner,
               scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(GetName, const char*());

  MOCK_METHOD2(MakeLinearOutputStream,
               AudioOutputStream*(const AudioParameters& params,
                                  const LogCallback& log_callback));
  MOCK_METHOD3(MakeLowLatencyOutputStream,
               AudioOutputStream*(const AudioParameters& params,
                                  const std::string& device_id,
                                  const LogCallback& log_callback));
  MOCK_METHOD3(MakeLinearInputStream,
               AudioInputStream*(const AudioParameters& params,
                                 const std::string& device_id,
                                 const LogCallback& log_callback));
  MOCK_METHOD3(MakeLowLatencyInputStream,
               AudioInputStream*(const AudioParameters& params,
                                 const std::string& device_id,
                                 const LogCallback& log_callback));

 protected:
  MOCK_METHOD0(HasAudioOutputDevices, bool());
  MOCK_METHOD0(HasAudioInputDevices, bool());
  MOCK_METHOD1(GetAudioInputDeviceNames,
               void(media::AudioDeviceNames* device_name));
  MOCK_METHOD2(GetPreferredOutputStreamParameters, AudioParameters(
      const std::string& device_id, const AudioParameters& params));

 private:
  media::FakeAudioLogFactory fake_audio_log_factory_;
};

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  int OnMoreData(base::TimeDelta /* delay */,
                 base::TimeTicks /* delay_timestamp */,
                 const media::AudioGlitchInfo& glitch_info,
                 AudioBus* dest) override {
    cumulative_glitch_info_ += glitch_info;
    dest->Zero();
    return dest->frames();
  }
  MOCK_METHOD1(OnError, void(ErrorType));

  media::AudioGlitchInfo cumulative_glitch_info() {
    return cumulative_glitch_info_;
  }

 private:
  media::AudioGlitchInfo cumulative_glitch_info_;
};

}  // namespace

namespace media {

class AudioOutputProxyTest : public testing::Test {
 protected:
  void SetUp() override {
    // Use a low sample rate and large buffer size when testing otherwise the
    // FakeAudioOutputStream will keep the message loop busy indefinitely; i.e.,
    // RunUntilIdle() will never terminate.
    params_ = AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                              ChannelLayoutConfig::Stereo(), 8000, 2048);
    InitDispatcher(base::Milliseconds(kTestCloseDelayMs));
  }

  void TearDown() override {
    // This is necessary to free all proxy objects that have been
    // closed by the test.
    base::RunLoop().RunUntilIdle();
  }

  virtual void InitDispatcher(base::TimeDelta close_delay) {
    dispatcher_impl_ = std::make_unique<AudioOutputDispatcherImpl>(
        &manager(), params_, std::string(), close_delay);
  }

  virtual void OnStart() {}

  MockAudioManager& manager() {
    return manager_;
  }

  void WaitForCloseTimer(MockAudioOutputStream* stream) {
    base::RunLoop run_loop;
    EXPECT_CALL(*stream, Close())
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    run_loop.Run();
  }

  void CloseAndWaitForCloseTimer(AudioOutputProxy* proxy,
                                 MockAudioOutputStream* stream) {
    // Close the stream and verify it doesn't happen immediately.
    proxy->Close();
    Mock::VerifyAndClear(stream);

    // Wait for the actual close event to come from the close timer.
    WaitForCloseTimer(stream);
  }

  // Basic Open() and Close() test.
  void OpenAndClose(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));

    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy->Open());
    CloseAndWaitForCloseTimer(proxy, &stream);
  }

  // Creates a stream, and then calls Start() and Stop().
  void StartAndStop(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream, SetVolume(_))
        .Times(1);

    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy->Open());

    proxy->Start(&callback_);
    OnStart();
    proxy->Stop();

    CloseAndWaitForCloseTimer(proxy, &stream);
    EXPECT_TRUE(stream.stop_called());
    EXPECT_TRUE(stream.start_called());
  }

  // Verify that the stream is closed after Stop() is called.
  void CloseAfterStop(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream, SetVolume(_))
        .Times(1);

    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy->Open());

    proxy->Start(&callback_);
    OnStart();
    proxy->Stop();

    // Wait for the close timer to fire after StopStream().
    WaitForCloseTimer(&stream);
    proxy->Close();
    EXPECT_TRUE(stream.stop_called());
    EXPECT_TRUE(stream.start_called());
  }

  // Create two streams, but don't start them.  Only one device must be opened.
  void TwoStreams(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));

    AudioOutputProxy* proxy1 = dispatcher->CreateStreamProxy();
    AudioOutputProxy* proxy2 = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy1->Open());
    EXPECT_TRUE(proxy2->Open());
    proxy1->Close();
    CloseAndWaitForCloseTimer(proxy2, &stream);
    EXPECT_FALSE(stream.stop_called());
    EXPECT_FALSE(stream.start_called());
  }

  // Open() method failed.
  void OpenFailed(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(false));
    EXPECT_CALL(stream, Close())
        .Times(1);

    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    EXPECT_FALSE(proxy->Open());
    proxy->Close();
    EXPECT_FALSE(stream.stop_called());
    EXPECT_FALSE(stream.start_called());
  }

  void CreateAndWait(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));

    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy->Open());

    WaitForCloseTimer(&stream);
    proxy->Close();
    EXPECT_FALSE(stream.stop_called());
    EXPECT_FALSE(stream.start_called());
  }

  void OneStream_TwoPlays(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));

    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream, SetVolume(_))
        .Times(2);

    AudioOutputProxy* proxy1 = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy1->Open());

    proxy1->Start(&callback_);
    OnStart();
    proxy1->Stop();

    // The stream should now be idle and get reused by |proxy2|.
    AudioOutputProxy* proxy2 = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy2->Open());
    proxy2->Start(&callback_);
    OnStart();
    proxy2->Stop();

    proxy1->Close();
    CloseAndWaitForCloseTimer(proxy2, &stream);
    EXPECT_TRUE(stream.stop_called());
    EXPECT_TRUE(stream.start_called());
  }

  void TwoStreams_BothPlaying(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream1(&manager_, params_);
    MockAudioOutputStream stream2(&manager_, params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream1))
        .WillOnce(Return(&stream2));

    EXPECT_CALL(stream1, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream1, SetVolume(_))
        .Times(1);

    EXPECT_CALL(stream2, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream2, SetVolume(_))
        .Times(1);

    AudioOutputProxy* proxy1 = dispatcher->CreateStreamProxy();
    AudioOutputProxy* proxy2 = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy1->Open());
    EXPECT_TRUE(proxy2->Open());

    proxy1->Start(&callback_);
    proxy2->Start(&callback_);
    OnStart();
    proxy1->Stop();
    CloseAndWaitForCloseTimer(proxy1, &stream1);

    proxy2->Stop();
    CloseAndWaitForCloseTimer(proxy2, &stream2);

    EXPECT_TRUE(stream1.stop_called());
    EXPECT_TRUE(stream1.start_called());
    EXPECT_TRUE(stream2.stop_called());
    EXPECT_TRUE(stream2.start_called());
  }

  void StartFailed(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));

    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy->Open());

    WaitForCloseTimer(&stream);

    // |stream| is closed at this point. Start() should reopen it again.
    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .Times(2)
        .WillRepeatedly(Return(reinterpret_cast<AudioOutputStream*>(NULL)));

    EXPECT_CALL(callback_, OnError(_)).Times(2);

    proxy->Start(&callback_);

    // Double Start() in the error case should be allowed since it's possible a
    // callback may not have had time to process the OnError() in between.
    proxy->Stop();
    proxy->Start(&callback_);

    Mock::VerifyAndClear(&callback_);

    proxy->Close();
  }

  void DispatcherDestroyed_BeforeOpen(
      std::unique_ptr<AudioOutputDispatcher> dispatcher) {
    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _)).Times(0);
    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    dispatcher.reset();
    EXPECT_FALSE(proxy->Open());
    proxy->Close();
  }

  void DispatcherDestroyed_BeforeStart(
      std::unique_ptr<AudioOutputDispatcher> dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);
    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open()).WillOnce(Return(true));
    EXPECT_CALL(stream, Close()).Times(1);
    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy->Open());

    EXPECT_CALL(callback_, OnError(_)).Times(1);
    dispatcher.reset();
    proxy->Start(&callback_);
    proxy->Stop();
    proxy->Close();
  }

  void DispatcherDestroyed_BeforeStop(
      std::unique_ptr<AudioOutputDispatcher> dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);
    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open()).WillOnce(Return(true));
    EXPECT_CALL(stream, Close()).Times(1);
    EXPECT_CALL(stream, SetVolume(_)).Times(1);

    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy->Open());
    proxy->Start(&callback_);
    dispatcher.reset();
    proxy->Stop();
    proxy->Close();
  }

  void DispatcherDestroyed_AfterStop(
      std::unique_ptr<AudioOutputDispatcher> dispatcher) {
    MockAudioOutputStream stream(&manager_, params_);
    EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open()).WillOnce(Return(true));
    EXPECT_CALL(stream, Close()).Times(1);
    EXPECT_CALL(stream, SetVolume(_)).Times(1);

    AudioOutputProxy* proxy = dispatcher->CreateStreamProxy();
    EXPECT_TRUE(proxy->Open());
    proxy->Start(&callback_);
    proxy->Stop();
    dispatcher.reset();
    proxy->Close();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockAudioManager manager_;
  std::unique_ptr<AudioOutputDispatcherImpl> dispatcher_impl_;
  MockAudioSourceCallback callback_;
  AudioParameters params_;
};

class AudioOutputResamplerTest : public AudioOutputProxyTest {
 public:
  void InitDispatcher(base::TimeDelta close_delay) override {
    // Use a low sample rate and large buffer size when testing otherwise the
    // FakeAudioOutputStream will keep the message loop busy indefinitely; i.e.,
    // RunUntilIdle() will never terminate.
    resampler_params_ =
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        ChannelLayoutConfig::Stereo(), 16000, 1024);
    resampler_ = std::make_unique<AudioOutputResampler>(
        &manager(), params_, resampler_params_, std::string(), close_delay,
        base::BindRepeating(&RegisterDebugRecording));
  }

  void OnStart() override {
    // Let Start() run for a bit.
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(kStartRunTimeMs));
    run_loop.Run();
  }

 protected:
  AudioParameters resampler_params_;
  std::unique_ptr<AudioOutputResampler> resampler_;
};

TEST_F(AudioOutputProxyTest, CreateAndClose) {
  AudioOutputProxy* proxy = dispatcher_impl_->CreateStreamProxy();
  proxy->Close();
}

TEST_F(AudioOutputResamplerTest, CreateAndClose) {
  AudioOutputProxy* proxy = resampler_->CreateStreamProxy();
  proxy->Close();
}

TEST_F(AudioOutputProxyTest, OpenAndClose) {
  OpenAndClose(dispatcher_impl_.get());
}

TEST_F(AudioOutputResamplerTest, OpenAndClose) {
  OpenAndClose(resampler_.get());
}

// Create a stream, and verify that it is closed after kTestCloseDelayMs.
// if it doesn't start playing.
TEST_F(AudioOutputProxyTest, CreateAndWait) {
  CreateAndWait(dispatcher_impl_.get());
}

// Create a stream, and verify that it is closed after kTestCloseDelayMs.
// if it doesn't start playing.
TEST_F(AudioOutputResamplerTest, CreateAndWait) {
  CreateAndWait(resampler_.get());
}

TEST_F(AudioOutputProxyTest, StartAndStop) {
  StartAndStop(dispatcher_impl_.get());
}

TEST_F(AudioOutputResamplerTest, StartAndStop) {
  StartAndStop(resampler_.get());
}

TEST_F(AudioOutputProxyTest, CloseAfterStop) {
  CloseAfterStop(dispatcher_impl_.get());
}

TEST_F(AudioOutputResamplerTest, CloseAfterStop) {
  CloseAfterStop(resampler_.get());
}

TEST_F(AudioOutputProxyTest, TwoStreams) {
  TwoStreams(dispatcher_impl_.get());
}

TEST_F(AudioOutputResamplerTest, TwoStreams) {
  TwoStreams(resampler_.get());
}

// Two streams: verify that second stream is allocated when the first
// starts playing.
TEST_F(AudioOutputProxyTest, OneStream_TwoPlays) {
  OneStream_TwoPlays(dispatcher_impl_.get());
}

TEST_F(AudioOutputResamplerTest, OneStream_TwoPlays) {
  OneStream_TwoPlays(resampler_.get());
}

// Two streams, both are playing. Dispatcher should not open a third stream.
TEST_F(AudioOutputProxyTest, TwoStreams_BothPlaying) {
  TwoStreams_BothPlaying(dispatcher_impl_.get());
}

TEST_F(AudioOutputResamplerTest, TwoStreams_BothPlaying) {
  TwoStreams_BothPlaying(resampler_.get());
}

TEST_F(AudioOutputProxyTest, OpenFailed) {
  OpenFailed(dispatcher_impl_.get());
}

// Start() method failed.
TEST_F(AudioOutputProxyTest, StartFailed) {
  StartFailed(dispatcher_impl_.get());
}

TEST_F(AudioOutputResamplerTest, StartFailed) {
  StartFailed(resampler_.get());
}

TEST_F(AudioOutputProxyTest, DispatcherDestroyed_BeforeOpen) {
  DispatcherDestroyed_BeforeOpen(std::move(dispatcher_impl_));
}

TEST_F(AudioOutputResamplerTest, DispatcherDestroyed_BeforeOpen) {
  DispatcherDestroyed_BeforeOpen(std::move(resampler_));
}

TEST_F(AudioOutputProxyTest, DispatcherDestroyed_BeforeStart) {
  DispatcherDestroyed_BeforeStart(std::move(dispatcher_impl_));
}

TEST_F(AudioOutputResamplerTest, DispatcherDestroyed_BeforeStart) {
  DispatcherDestroyed_BeforeStart(std::move(resampler_));
}

TEST_F(AudioOutputProxyTest, DispatcherDestroyed_BeforeStop) {
  DispatcherDestroyed_BeforeStop(std::move(dispatcher_impl_));
}

TEST_F(AudioOutputResamplerTest, DispatcherDestroyed_BeforeStop) {
  DispatcherDestroyed_BeforeStop(std::move(resampler_));
}

TEST_F(AudioOutputProxyTest, DispatcherDestroyed_AfterStop) {
  DispatcherDestroyed_AfterStop(std::move(dispatcher_impl_));
}

TEST_F(AudioOutputResamplerTest, DispatcherDestroyed_AfterStop) {
  DispatcherDestroyed_AfterStop(std::move(resampler_));
}

TEST_F(AudioOutputProxyTest, DispatcherDeviceChangeClosesIdleStreams) {
  // Set close delay so long that it triggers a test timeout if relied upon.
  InitDispatcher(base::Seconds(1000));

  MockAudioOutputStream stream(&manager_, params_);

  EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open()).WillOnce(Return(true));

  AudioOutputProxy* proxy = dispatcher_impl_->CreateStreamProxy();
  EXPECT_TRUE(proxy->Open());

  // Close the stream and verify it doesn't happen immediately.
  proxy->Close();
  Mock::VerifyAndClear(&stream);

  // This should trigger a true close on the stream.
  dispatcher_impl_->OnDeviceChange();

  base::RunLoop run_loop;
  EXPECT_CALL(stream, Close())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

// Simulate AudioOutputStream::Create() failure with a low latency stream and
// ensure AudioOutputResampler falls back to the high latency path.
TEST_F(AudioOutputResamplerTest, LowLatencyCreateFailedFallback) {
  MockAudioOutputStream stream(&manager_, params_);
  EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
      .Times(2)
      .WillOnce(Return(static_cast<AudioOutputStream*>(NULL)))
      .WillRepeatedly(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(true));

  AudioOutputProxy* proxy = resampler_->CreateStreamProxy();
  EXPECT_TRUE(proxy->Open());
  CloseAndWaitForCloseTimer(proxy, &stream);
}

// Simulate AudioOutputStream::Open() failure with a low latency stream and
// ensure AudioOutputResampler falls back to the high latency path.
TEST_F(AudioOutputResamplerTest, LowLatencyOpenFailedFallback) {
  MockAudioOutputStream failed_stream(&manager_, params_);
  MockAudioOutputStream okay_stream(&manager_, params_);
  EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
      .Times(2)
      .WillOnce(Return(&failed_stream))
      .WillRepeatedly(Return(&okay_stream));
  EXPECT_CALL(failed_stream, Open())
      .WillOnce(Return(false));
  EXPECT_CALL(failed_stream, Close())
      .Times(1);
  EXPECT_CALL(okay_stream, Open())
      .WillOnce(Return(true));

  AudioOutputProxy* proxy = resampler_->CreateStreamProxy();
  EXPECT_TRUE(proxy->Open());
  CloseAndWaitForCloseTimer(proxy, &okay_stream);
}

// Simulate failures to open both the low latency and the fallback high latency
// stream and ensure AudioOutputResampler falls back to a fake stream.
TEST_F(AudioOutputResamplerTest, HighLatencyFallbackFailed) {
  MockAudioOutputStream okay_stream(&manager_, params_);

// Only Windows has a high latency output driver that is not the same as the low
// latency path.
#if BUILDFLAG(IS_WIN)
  static const int kFallbackCount = 2;
#else
  static const int kFallbackCount = 1;
#endif
  EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
      .Times(kFallbackCount)
      .WillRepeatedly(Return(static_cast<AudioOutputStream*>(NULL)));

  // To prevent shared memory issues the sample rate and buffer size should
  // match the input stream parameters.
  EXPECT_CALL(manager(),
              MakeAudioOutputStream(
                  AllOf(testing::Property(&AudioParameters::format,
                                          AudioParameters::AUDIO_FAKE),
                        testing::Property(&AudioParameters::sample_rate,
                                          params_.sample_rate()),
                        testing::Property(&AudioParameters::frames_per_buffer,
                                          params_.frames_per_buffer())),
                  _, _))
      .Times(1)
      .WillOnce(Return(&okay_stream));
  EXPECT_CALL(okay_stream, Open())
      .WillOnce(Return(true));

  AudioOutputProxy* proxy = resampler_->CreateStreamProxy();
  EXPECT_TRUE(proxy->Open());
  CloseAndWaitForCloseTimer(proxy, &okay_stream);
}

// Simulate failures to open both the low latency, the fallback high latency
// stream, and the fake audio output stream and ensure AudioOutputResampler
// terminates normally.
TEST_F(AudioOutputResamplerTest, AllFallbackFailed) {
// Only Windows has a high latency output driver that is not the same as the low
// latency path.
#if BUILDFLAG(IS_WIN)
  static const int kFallbackCount = 3;
#else
  static const int kFallbackCount = 2;
#endif
  EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
      .Times(kFallbackCount)
      .WillRepeatedly(Return(static_cast<AudioOutputStream*>(NULL)));

  AudioOutputProxy* proxy = resampler_->CreateStreamProxy();
  EXPECT_FALSE(proxy->Open());
  proxy->Close();
}

// Simulate an eventual OpenStream() failure; i.e. successful OpenStream() calls
// eventually followed by one which fails; root cause of http://crbug.com/150619
TEST_F(AudioOutputResamplerTest, LowLatencyOpenEventuallyFails) {
  MockAudioOutputStream stream1(&manager_, params_);
  MockAudioOutputStream stream2(&manager_, params_);

  // Setup the mock such that all three streams are successfully created.
  EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
      .WillOnce(Return(&stream1))
      .WillOnce(Return(&stream2))
      .WillRepeatedly(Return(static_cast<AudioOutputStream*>(NULL)));

  // Stream1 should be able to successfully open and start.
  EXPECT_CALL(stream1, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream1, SetVolume(_))
      .Times(1);

  // Stream2 should also be able to successfully open and start.
  EXPECT_CALL(stream2, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream2, SetVolume(_))
      .Times(1);

  // Open and start the first proxy and stream.
  AudioOutputProxy* proxy1 = resampler_->CreateStreamProxy();
  EXPECT_TRUE(proxy1->Open());
  proxy1->Start(&callback_);
  OnStart();

  // Open and start the second proxy and stream.
  AudioOutputProxy* proxy2 = resampler_->CreateStreamProxy();
  EXPECT_TRUE(proxy2->Open());
  proxy2->Start(&callback_);
  OnStart();

  // Attempt to open the third stream which should fail.
  AudioOutputProxy* proxy3 = resampler_->CreateStreamProxy();
  EXPECT_FALSE(proxy3->Open());
  proxy3->Close();

  // Perform the required Stop()/Close() shutdown dance for each proxy.  Under
  // the hood each proxy should correctly call CloseStream() if OpenStream()
  // succeeded or not.
  proxy2->Stop();
  CloseAndWaitForCloseTimer(proxy2, &stream2);

  proxy1->Stop();
  CloseAndWaitForCloseTimer(proxy1, &stream1);

  EXPECT_TRUE(stream1.stop_called());
  EXPECT_TRUE(stream1.start_called());
  EXPECT_TRUE(stream2.stop_called());
  EXPECT_TRUE(stream2.start_called());
}

// Simulate failures to open both the low latency and the fallback high latency
// stream and ensure AudioOutputResampler falls back to a fake stream.  Ensure
// that after the close delay elapses, opening another stream succeeds with a
// non-fake stream.
TEST_F(AudioOutputResamplerTest, FallbackRecovery) {
  MockAudioOutputStream fake_stream(&manager_, params_);

  // Trigger the fallback mechanism until a fake output stream is created.
#if BUILDFLAG(IS_WIN)
  static const int kFallbackCount = 2;
#else
  static const int kFallbackCount = 1;
#endif
  EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
      .Times(kFallbackCount)
      .WillRepeatedly(Return(static_cast<AudioOutputStream*>(NULL)));
  EXPECT_CALL(manager(),
              MakeAudioOutputStream(
                  AllOf(testing::Property(&AudioParameters::format,
                                          AudioParameters::AUDIO_FAKE),
                        testing::Property(&AudioParameters::sample_rate,
                                          params_.sample_rate()),
                        testing::Property(&AudioParameters::frames_per_buffer,
                                          params_.frames_per_buffer())),
                  _, _))
      .WillOnce(Return(&fake_stream));
  EXPECT_CALL(fake_stream, Open()).WillOnce(Return(true));
  AudioOutputProxy* proxy = resampler_->CreateStreamProxy();
  EXPECT_TRUE(proxy->Open());
  CloseAndWaitForCloseTimer(proxy, &fake_stream);

  // Once all proxies have been closed, AudioOutputResampler will start the
  // reinitialization timer and execute it after the close delay elapses.
  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::Milliseconds(2 * kTestCloseDelayMs));
  run_loop.Run();

  // Verify a non-fake stream can be created.
  MockAudioOutputStream real_stream(&manager_, params_);
  EXPECT_CALL(manager(),
              MakeAudioOutputStream(
                  testing::Property(&AudioParameters::format,
                                    testing::Ne(AudioParameters::AUDIO_FAKE)),
                  _, _))
      .WillOnce(Return(&real_stream));

  // Stream1 should be able to successfully open and start.
  EXPECT_CALL(real_stream, Open()).WillOnce(Return(true));
  proxy = resampler_->CreateStreamProxy();
  EXPECT_TRUE(proxy->Open());
  CloseAndWaitForCloseTimer(proxy, &real_stream);
}

TEST_F(AudioOutputResamplerTest, PropagatesGlitchInfo) {
  CallbackExposingMockOutputStream stream;

  EXPECT_CALL(manager(), MakeAudioOutputStream(_, _, _))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(stream, SetVolume(_)).Times(1);
  AudioOutputProxy* proxy = resampler_->CreateStreamProxy();
  EXPECT_TRUE(proxy->Open());
  proxy->Start(&callback_);

  // Get the callback created by the resampler and send glitch info through it.
  CHECK(stream.GetCallback());
  AudioOutputStream::AudioSourceCallback* inner_callback =
      stream.GetCallback().value();
  media::AudioGlitchInfo glitch_info{.duration = base::Seconds(0.1),
                                     .count = 123};
  auto dest = AudioBus::Create(resampler_params_);

  inner_callback->OnMoreData(base::TimeDelta(), base::TimeTicks(), glitch_info,
                             dest.get());
  EXPECT_EQ(callback_.cumulative_glitch_info(), glitch_info);
  inner_callback->OnMoreData(base::TimeDelta(), base::TimeTicks(), {},
                             dest.get());
  EXPECT_EQ(callback_.cumulative_glitch_info(), glitch_info);

  proxy->Stop();
  proxy->Close();
  Mock::VerifyAndClearExpectations(&stream);
  base::RunLoop run_loop;
  EXPECT_CALL(stream, Close())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

}  // namespace media
