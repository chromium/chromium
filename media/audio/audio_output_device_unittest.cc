// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_device.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::CancelableSyncSocket;
using base::UnsafeSharedMemoryRegion;
using base::WritableSharedMemoryMapping;
using base::SyncSocket;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::WithArg;
using testing::StrictMock;
using testing::NiceMock;
using testing::NotNull;
using testing::Mock;

namespace media {

namespace {

constexpr char kDefaultDeviceId[] = "";
constexpr char kNonDefaultDeviceId[] = "valid-nondefault-device-id";
constexpr char kUnauthorizedDeviceId[] = "unauthorized-device-id";
constexpr base::TimeDelta kAuthTimeout = base::Milliseconds(10000);

class MockRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  MockRenderCallback() = default;
  ~MockRenderCallback() override = default;

  MOCK_METHOD4(Render,
               int(base::TimeDelta delay,
                   base::TimeTicks timestamp,
                   const AudioGlitchInfo& glitch_info,
                   AudioBus* dest));
  MOCK_METHOD0(OnRenderError, void());
};

class MockAudioOutputIPC : public AudioOutputIPC {
 public:
  MockAudioOutputIPC() = default;
  ~MockAudioOutputIPC() override = default;

  MOCK_METHOD3(RequestDeviceAuthorization,
               void(AudioOutputIPCDelegate* delegate,
                    const base::UnguessableToken& session_id,
                    const std::string& device_id));
  MOCK_METHOD2(CreateStream,
               void(AudioOutputIPCDelegate* delegate,
                    const AudioParameters& params));
  MOCK_METHOD0(PlayStream, void());
  MOCK_METHOD0(PauseStream, void());
  MOCK_METHOD0(FlushStream, void());
  MOCK_METHOD0(CloseStream, void());
  MOCK_METHOD1(SetVolume, void(double volume));
};

}  // namespace.

class AudioOutputDeviceTest : public testing::Test {
 public:
  AudioOutputDeviceTest();

  AudioOutputDeviceTest(const AudioOutputDeviceTest&) = delete;
  AudioOutputDeviceTest& operator=(const AudioOutputDeviceTest&) = delete;

  ~AudioOutputDeviceTest() override;

  void ReceiveAuthorization(OutputDeviceStatus device_status);
  void StartAudioDevice();
  void CallOnStreamCreated();
  void StopAudioDevice();
  void FlushAudioDevice();
  void CreateDevice(const std::string& device_id,
                    base::TimeDelta timeout = kAuthTimeout);
  void SetDevice(const std::string& device_id);

  MOCK_METHOD1(OnDeviceInfoReceived, void(OutputDeviceInfo));

 protected:
  void Render();
  void CloseBrowserSocket();

  MockAudioOutputIPC* audio_output_ipc() {
    return static_cast<MockAudioOutputIPC*>(audio_device_->GetIpcForTesting());
  }

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AudioParameters default_audio_parameters_;
  StrictMock<MockRenderCallback> callback_;
  OutputDeviceStatus device_status_;

 private:
  // These may need to outlive `audio_device_`.
  UnsafeSharedMemoryRegion shared_memory_region_;
  WritableSharedMemoryMapping shared_memory_mapping_;
  CancelableSyncSocket browser_socket_;
  CancelableSyncSocket renderer_socket_;
  uint32_t counter_ = 0;

 protected:
  scoped_refptr<AudioOutputDevice> audio_device_;
};

AudioOutputDeviceTest::AudioOutputDeviceTest()
    : device_status_(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL) {
  default_audio_parameters_.Reset(AudioParameters::AUDIO_PCM_LINEAR,
                                  ChannelLayoutConfig::Stereo(), 48000, 1024);
  SetDevice(kDefaultDeviceId);
}

AudioOutputDeviceTest::~AudioOutputDeviceTest() = default;

void AudioOutputDeviceTest::CreateDevice(const std::string& device_id,
                                         base::TimeDelta timeout) {
  // Make sure the previous device is properly cleaned up.
  if (audio_device_)
    StopAudioDevice();

  audio_device_ = new AudioOutputDevice(
      std::make_unique<NiceMock<MockAudioOutputIPC>>(),
      task_env_.GetMainThreadTaskRunner(),
      AudioSinkParameters(base::UnguessableToken(), device_id), timeout);
}

void AudioOutputDeviceTest::SetDevice(const std::string& device_id) {
  CreateDevice(device_id);
  EXPECT_CALL(*audio_output_ipc(),
              RequestDeviceAuthorization(audio_device_.get(),
                                         base::UnguessableToken(), device_id));
  audio_device_->RequestDeviceAuthorization();
  task_env_.FastForwardBy(base::TimeDelta());

  // Simulate response from browser
  OutputDeviceStatus device_status =
      (device_id == kUnauthorizedDeviceId)
          ? OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED
          : OUTPUT_DEVICE_STATUS_OK;
  ReceiveAuthorization(device_status);

  audio_device_->Initialize(default_audio_parameters_,
                            &callback_);
}

void AudioOutputDeviceTest::ReceiveAuthorization(OutputDeviceStatus status) {
  device_status_ = status;
  if (device_status_ != OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc(), CloseStream());

  audio_device_->OnDeviceAuthorized(device_status_, default_audio_parameters_,
                                    kDefaultDeviceId);
  task_env_.FastForwardBy(base::TimeDelta());
}

void AudioOutputDeviceTest::StartAudioDevice() {
  if (device_status_ == OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc(), CreateStream(audio_device_.get(), _));
  else
    EXPECT_CALL(callback_, OnRenderError());

  audio_device_->Start();
  task_env_.FastForwardBy(base::TimeDelta());
}

void AudioOutputDeviceTest::CallOnStreamCreated() {
  const uint32_t kMemorySize =
      ComputeAudioOutputBufferSize(default_audio_parameters_);

  shared_memory_region_ = base::UnsafeSharedMemoryRegion::Create(kMemorySize);
  ASSERT_TRUE(shared_memory_region_.IsValid());
  shared_memory_mapping_ = shared_memory_region_.Map();
  ASSERT_TRUE(shared_memory_mapping_.IsValid());
  memset(shared_memory_mapping_.memory(), 0xff, kMemorySize);

  ASSERT_TRUE(CancelableSyncSocket::CreatePair(&browser_socket_,
                                               &renderer_socket_));

  // Create duplicates of the handles we pass to AudioOutputDevice since
  // ownership will be transferred and AudioOutputDevice is responsible for
  // freeing.
  base::UnsafeSharedMemoryRegion duplicated_memory_region =
      shared_memory_region_.Duplicate();
  ASSERT_TRUE(duplicated_memory_region.IsValid());

  audio_device_->OnStreamCreated(std::move(duplicated_memory_region),
                                 renderer_socket_.Take(),
                                 /*playing_automatically*/ false);
  task_env_.FastForwardBy(base::TimeDelta());
}

void AudioOutputDeviceTest::StopAudioDevice() {
  if (device_status_ == OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc(), CloseStream());

  audio_device_->Stop();
  task_env_.FastForwardBy(base::TimeDelta());
}

void AudioOutputDeviceTest::FlushAudioDevice() {
  if (device_status_ == OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc(), FlushStream());

  audio_device_->Flush();
  task_env_.FastForwardBy(base::TimeDelta());
}

void AudioOutputDeviceTest::Render() {
  browser_socket_.Send(base::byte_span_from_ref(counter_));
  ++counter_;
}

void AudioOutputDeviceTest::CloseBrowserSocket() {
  browser_socket_.Close();
}

TEST_F(AudioOutputDeviceTest, Initialize) {
  // Tests that the object can be constructed, initialized and destructed
  // without having ever been started.
  StopAudioDevice();
}

// Calls Start() followed by an immediate Stop() and check for the basic message
// filter messages being sent in that case.
TEST_F(AudioOutputDeviceTest, StartStop) {
  StartAudioDevice();
  StopAudioDevice();
}

// AudioOutputDevice supports multiple start/stop sequences.
TEST_F(AudioOutputDeviceTest, StartStopStartStop) {
  StartAudioDevice();
  StopAudioDevice();
  StartAudioDevice();
  StopAudioDevice();
}

// Simulate receiving OnStreamCreated() prior to processing ShutDownOnIOThread()
// on the IO loop.
TEST_F(AudioOutputDeviceTest, StopBeforeRender) {
  StartAudioDevice();

  // Call Stop() but don't run the IO loop yet.
  audio_device_->Stop();

  // Expect us to shutdown IPC but not to render anything despite the stream
  // getting created.
  EXPECT_CALL(*audio_output_ipc(), CloseStream());
  CallOnStreamCreated();
}

TEST_F(AudioOutputDeviceTest, NoErrorForNormalShutdown) {
  StartAudioDevice();
  CallOnStreamCreated();

  base::RunLoop run_loop;
  EXPECT_CALL(callback_, Render(_, _, _, _))
      .WillOnce(DoAll(base::test::RunClosure(run_loop.QuitWhenIdleClosure()),
                      Return(0)))
      .WillRepeatedly(Return(0));

  EXPECT_CALL(callback_, OnRenderError()).Times(0);

  Render();
  run_loop.Run();

  StopAudioDevice();
}

TEST_F(AudioOutputDeviceTest, ErrorFiredForSocketClose) {
  StartAudioDevice();
  CallOnStreamCreated();

  // Lock used to ensure Render() completes before CloseBrowserSocket() starts.
  base::Lock send_lock_;

  base::RunLoop run_loop;
  EXPECT_CALL(callback_, Render(_, _, _, _))
      .WillOnce(DoAll(base::test::RunClosure(base::BindLambdaForTesting([&]() {
                        base::AutoLock lock(send_lock_);
                        CloseBrowserSocket();
                      })),
                      Return(0)))
      .WillRepeatedly(Return(0));

  EXPECT_CALL(callback_, OnRenderError())
      .WillOnce(base::test::RunClosure(run_loop.QuitWhenIdleClosure()));

  {
    base::AutoLock lock(send_lock_);
    Render();
  }
  run_loop.Run();

  StopAudioDevice();
}

// Multiple start/stop with nondefault device
TEST_F(AudioOutputDeviceTest, NonDefaultStartStopStartStop) {
  SetDevice(kNonDefaultDeviceId);
  StartAudioDevice();
  StopAudioDevice();

  EXPECT_CALL(*audio_output_ipc(),
              RequestDeviceAuthorization(audio_device_.get(),
                                         base::UnguessableToken(), _));
  StartAudioDevice();
  // Simulate reply from browser
  ReceiveAuthorization(OUTPUT_DEVICE_STATUS_OK);

  StopAudioDevice();
}

TEST_F(AudioOutputDeviceTest, UnauthorizedDevice) {
  SetDevice(kUnauthorizedDeviceId);
  StartAudioDevice();
  StopAudioDevice();
}

TEST_F(AudioOutputDeviceTest,
       StartUnauthorizedDeviceAndStopBeforeErrorFires_NoError) {
  SetDevice(kUnauthorizedDeviceId);
  audio_device_->Start();
  // Don't run the runloop. We stop before |audio_device| gets the
  // authorization error, so it's not allowed to dereference |callback_|.
  EXPECT_CALL(callback_, OnRenderError()).Times(0);
  StopAudioDevice();
}

TEST_F(AudioOutputDeviceTest, AuthorizationFailsBeforeInitialize_NoError) {
  // Clear audio device set by fixture.
  StopAudioDevice();
  audio_device_ = new AudioOutputDevice(
      std::make_unique<NiceMock<MockAudioOutputIPC>>(),
      task_env_.GetMainThreadTaskRunner(),
      AudioSinkParameters(base::UnguessableToken(), kDefaultDeviceId),
      kAuthTimeout);
  EXPECT_CALL(
      *audio_output_ipc(),
      RequestDeviceAuthorization(audio_device_.get(), base::UnguessableToken(),
                                 kDefaultDeviceId));

  audio_device_->RequestDeviceAuthorization();
  audio_device_->Initialize(default_audio_parameters_, &callback_);
  task_env_.FastForwardBy(base::TimeDelta());
  audio_device_->Stop();

  // We've stopped, so accessing |callback_| isn't ok.
  EXPECT_CALL(callback_, OnRenderError()).Times(0);
  audio_device_->OnDeviceAuthorized(OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
                                    default_audio_parameters_,
                                    kDefaultDeviceId);
  task_env_.FastForwardBy(base::TimeDelta());
}

TEST_F(AudioOutputDeviceTest, AuthorizationTimedOut) {
  CreateDevice(kNonDefaultDeviceId);
  EXPECT_CALL(
      *audio_output_ipc(),
      RequestDeviceAuthorization(audio_device_.get(), base::UnguessableToken(),
                                 kNonDefaultDeviceId));
  EXPECT_CALL(*audio_output_ipc(), CloseStream());

  // Request authorization; no reply from the browser.
  audio_device_->RequestDeviceAuthorization();

  // Advance time until we hit the timeout.
  task_env_.FastForwardUntilNoTasksRemain();

  audio_device_->Stop();
  task_env_.FastForwardBy(base::TimeDelta());
}

TEST_F(AudioOutputDeviceTest, GetOutputDeviceInfoAsync_Error) {
  CreateDevice(kUnauthorizedDeviceId, base::TimeDelta());
  EXPECT_CALL(
      *audio_output_ipc(),
      RequestDeviceAuthorization(audio_device_.get(), base::UnguessableToken(),
                                 kUnauthorizedDeviceId));
  audio_device_->RequestDeviceAuthorization();
  audio_device_->GetOutputDeviceInfoAsync(base::BindOnce(
      &AudioOutputDeviceTest::OnDeviceInfoReceived, base::Unretained(this)));
  task_env_.FastForwardBy(base::TimeDelta());

  OutputDeviceInfo info;
  constexpr auto kExpectedStatus = OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED;
  EXPECT_CALL(*this, OnDeviceInfoReceived(_))
      .WillOnce(testing::SaveArg<0>(&info));
  ReceiveAuthorization(kExpectedStatus);

  task_env_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(kExpectedStatus, info.device_status());
  EXPECT_EQ(kUnauthorizedDeviceId, info.device_id());
  EXPECT_TRUE(
      AudioParameters::UnavailableDeviceParams().Equals(info.output_params()));

  audio_device_->Stop();
  task_env_.FastForwardBy(base::TimeDelta());
}

TEST_F(AudioOutputDeviceTest, GetOutputDeviceInfoAsync_Okay) {
  CreateDevice(kDefaultDeviceId, base::TimeDelta());
  EXPECT_CALL(
      *audio_output_ipc(),
      RequestDeviceAuthorization(audio_device_.get(), base::UnguessableToken(),
                                 kDefaultDeviceId));
  audio_device_->RequestDeviceAuthorization();
  audio_device_->GetOutputDeviceInfoAsync(base::BindOnce(
      &AudioOutputDeviceTest::OnDeviceInfoReceived, base::Unretained(this)));
  task_env_.FastForwardBy(base::TimeDelta());

  OutputDeviceInfo info;
  constexpr auto kExpectedStatus = OUTPUT_DEVICE_STATUS_OK;
  EXPECT_CALL(*this, OnDeviceInfoReceived(_))
      .WillOnce(testing::SaveArg<0>(&info));
  ReceiveAuthorization(kExpectedStatus);

  task_env_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(kExpectedStatus, info.device_status());
  EXPECT_EQ(kDefaultDeviceId, info.device_id());
  EXPECT_TRUE(default_audio_parameters_.Equals(info.output_params()));

  audio_device_->Stop();
  task_env_.FastForwardBy(base::TimeDelta());
}

TEST_F(AudioOutputDeviceTest, StreamIsFlushed) {
  StartAudioDevice();
  FlushAudioDevice();
  StopAudioDevice();
}

}  // namespace media.
