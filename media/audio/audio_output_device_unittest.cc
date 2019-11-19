// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_device.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/single_thread_task_runner.h"
#include "base/sync_socket.h"
#include "base/task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/audio/audio_sync_reader.h"
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
constexpr float kAudioData = 0.618;
constexpr base::TimeDelta kAuthTimeout =
    base::TimeDelta::FromMilliseconds(10000);
constexpr base::TimeDelta kDelay = base::TimeDelta::FromMicroseconds(123);
constexpr int kFramesSkipped = 456;
constexpr int kFrames = 789;
constexpr int kBitstreamFrames = 101;
constexpr size_t kBitstreamDataSize = 512;

class MockRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  MockRenderCallback() = default;
  ~MockRenderCallback() override = default;

  MOCK_METHOD4(Render,
               int(base::TimeDelta delay,
                   base::TimeTicks timestamp,
                   int prior_frames_skipped,
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
  MOCK_METHOD3(
      CreateStream,
      void(AudioOutputIPCDelegate* delegate,
           const AudioParameters& params,
           const base::Optional<base::UnguessableToken>& processing_id));
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
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AudioParameters default_audio_parameters_;
  StrictMock<MockRenderCallback> callback_;
  MockAudioOutputIPC* audio_output_ipc_;  // owned by audio_device_
  scoped_refptr<AudioOutputDevice> audio_device_;
  OutputDeviceStatus device_status_;

 private:
  int CalculateMemorySize();

  UnsafeSharedMemoryRegion shared_memory_region_;
  WritableSharedMemoryMapping shared_memory_mapping_;
  CancelableSyncSocket browser_socket_;
  CancelableSyncSocket renderer_socket_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDeviceTest);
};

AudioOutputDeviceTest::AudioOutputDeviceTest()
    : device_status_(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL) {
  default_audio_parameters_.Reset(AudioParameters::AUDIO_PCM_LINEAR,
                                  CHANNEL_LAYOUT_STEREO, 48000, 1024);
  SetDevice(kDefaultDeviceId);
}

AudioOutputDeviceTest::~AudioOutputDeviceTest() {
  audio_device_ = nullptr;
}

void AudioOutputDeviceTest::CreateDevice(const std::string& device_id,
                                         base::TimeDelta timeout) {
  // Make sure the previous device is properly cleaned up.
  if (audio_device_)
    StopAudioDevice();

  audio_output_ipc_ = new NiceMock<MockAudioOutputIPC>();
  audio_device_ = new AudioOutputDevice(
      base::WrapUnique(audio_output_ipc_), task_env_.GetMainThreadTaskRunner(),
      AudioSinkParameters(base::UnguessableToken(), device_id), timeout);
}

void AudioOutputDeviceTest::SetDevice(const std::string& device_id) {
  CreateDevice(device_id);
  EXPECT_CALL(*audio_output_ipc_,
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
    EXPECT_CALL(*audio_output_ipc_, CloseStream());

  audio_device_->OnDeviceAuthorized(device_status_, default_audio_parameters_,
                                    kDefaultDeviceId);
  task_env_.FastForwardBy(base::TimeDelta());
}

void AudioOutputDeviceTest::StartAudioDevice() {
  if (device_status_ == OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc_, CreateStream(audio_device_.get(), _, _));
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
  SyncSocket::TransitDescriptor audio_device_socket_descriptor;
  ASSERT_TRUE(renderer_socket_.PrepareTransitDescriptor(
      base::GetCurrentProcessHandle(), &audio_device_socket_descriptor));
  base::UnsafeSharedMemoryRegion duplicated_memory_region =
      shared_memory_region_.Duplicate();
  ASSERT_TRUE(duplicated_memory_region.IsValid());

  audio_device_->OnStreamCreated(
      std::move(duplicated_memory_region),
      SyncSocket::UnwrapHandle(audio_device_socket_descriptor),
      /*playing_automatically*/ false);
  task_env_.FastForwardBy(base::TimeDelta());
}

void AudioOutputDeviceTest::StopAudioDevice() {
  if (device_status_ == OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc_, CloseStream());

  audio_device_->Stop();
  task_env_.FastForwardBy(base::TimeDelta());
}

void AudioOutputDeviceTest::FlushAudioDevice() {
  if (device_status_ == OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc_, FlushStream());

  audio_device_->Flush();
  task_env_.FastForwardBy(base::TimeDelta());
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
  EXPECT_CALL(*audio_output_ipc_, CloseStream());
  CallOnStreamCreated();
}

// Multiple start/stop with nondefault device
TEST_F(AudioOutputDeviceTest, NonDefaultStartStopStartStop) {
  SetDevice(kNonDefaultDeviceId);
  StartAudioDevice();
  StopAudioDevice();

  EXPECT_CALL(*audio_output_ipc_,
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
  audio_output_ipc_ = new NiceMock<MockAudioOutputIPC>();
  audio_device_ = new AudioOutputDevice(
      base::WrapUnique(audio_output_ipc_), task_env_.GetMainThreadTaskRunner(),
      AudioSinkParameters(base::UnguessableToken(), kDefaultDeviceId),
      kAuthTimeout);
  EXPECT_CALL(
      *audio_output_ipc_,
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
      *audio_output_ipc_,
      RequestDeviceAuthorization(audio_device_.get(), base::UnguessableToken(),
                                 kNonDefaultDeviceId));
  EXPECT_CALL(*audio_output_ipc_, CloseStream());

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
      *audio_output_ipc_,
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
      *audio_output_ipc_,
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

namespace {

// This struct collects useful stuff without doing anything magical. It is used
// below, where the test fixture is too inflexible.
struct TestEnvironment {
  explicit TestEnvironment(const AudioParameters& params) {
    const uint32_t memory_size = ComputeAudioOutputBufferSize(params);
    auto shared_memory_region =
        base::UnsafeSharedMemoryRegion::Create(memory_size);
    auto shared_memory_mapping = shared_memory_region.Map();
    CHECK(shared_memory_region.IsValid());
    CHECK(shared_memory_mapping.IsValid());
    auto browser_socket = std::make_unique<base::CancelableSyncSocket>();
    CHECK(CancelableSyncSocket::CreatePair(browser_socket.get(),
                                           &renderer_socket));
    reader = std::make_unique<AudioSyncReader>(
        /*log callback*/ base::DoNothing(), params,
        std::move(shared_memory_region), std::move(shared_memory_mapping),
        std::move(browser_socket));
    time_stamp = base::TimeTicks::Now();

#if defined(OS_FUCHSIA)
    // TODO(https://crbug.com/838367): Fuchsia bots use nested virtualization,
    // which can result in unusually long scheduling delays, so allow a longer
    // timeout.
    reader->set_max_wait_timeout_for_test(
        base::TimeDelta::FromMilliseconds(250));
#endif
  }

  base::CancelableSyncSocket renderer_socket;
  StrictMock<MockRenderCallback> callback;
  std::unique_ptr<AudioSyncReader> reader;
  base::TimeTicks time_stamp;
};

}  // namespace

#if defined(ADDRESS_SANITIZER)
// TODO(crbug.com/903696): Flaky, at least on CrOS ASAN.
#define MAYBE_VerifyDataFlow DISABLED_VerifyDataFlow
#else
#define MAYBE_VerifyDataFlow VerifyDataFlow
#endif
TEST_F(AudioOutputDeviceTest, MAYBE_VerifyDataFlow) {
  // The test fixture isn't used in this test, but we still have to clean up
  // after it.
  StopAudioDevice();

  auto params = AudioParameters::UnavailableDeviceParams();
  params.set_frames_per_buffer(kFrames);
  ASSERT_EQ(2, params.channels());
  TestEnvironment env(params);
  auto* ipc = new MockAudioOutputIPC();  // owned by |audio_device|.
  auto audio_device = base::MakeRefCounted<AudioOutputDevice>(
      base::WrapUnique(ipc), task_env_.GetMainThreadTaskRunner(),
      AudioSinkParameters(base::UnguessableToken(), kDefaultDeviceId),
      kAuthTimeout);

  // Start a stream.
  audio_device->RequestDeviceAuthorization();
  audio_device->Initialize(params, &env.callback);
  audio_device->Start();
  EXPECT_CALL(*ipc, RequestDeviceAuthorization(audio_device.get(),
                                               base::UnguessableToken(),
                                               kDefaultDeviceId));
  EXPECT_CALL(*ipc, CreateStream(audio_device.get(), _, _));
  EXPECT_CALL(*ipc, PlayStream());
  task_env_.RunUntilIdle();
  Mock::VerifyAndClear(ipc);
  audio_device->OnDeviceAuthorized(OUTPUT_DEVICE_STATUS_OK, params,
                                   kDefaultDeviceId);
  audio_device->OnStreamCreated(env.reader->TakeSharedMemoryRegion(),
                                env.renderer_socket.Release(),
                                /*playing_automatically*/ false);

  task_env_.RunUntilIdle();
  // At this point, the callback thread should be running. Send some data over
  // and verify that it's propagated to |env.callback|. Do it a few times.
  auto test_bus = AudioBus::Create(params);
  for (int i = 0; i < 10; ++i) {
    test_bus->Zero();
    EXPECT_CALL(env.callback,
                Render(kDelay, env.time_stamp, kFramesSkipped, NotNull()))
        .WillOnce(WithArg<3>(Invoke([](AudioBus* renderer_bus) -> int {
          // Place some test data in the bus so that we can check that it was
          // copied to the browser side.
          std::fill_n(renderer_bus->channel(0), renderer_bus->frames(),
                      kAudioData);
          std::fill_n(renderer_bus->channel(1), renderer_bus->frames(),
                      kAudioData);
          return renderer_bus->frames();
        })));
    env.reader->RequestMoreData(kDelay, env.time_stamp, kFramesSkipped);
    env.reader->Read(test_bus.get());

    Mock::VerifyAndClear(&env.callback);
    for (int i = 0; i < kFrames; ++i) {
      EXPECT_EQ(kAudioData, test_bus->channel(0)[i]);
      EXPECT_EQ(kAudioData, test_bus->channel(1)[i]);
    }
  }

  audio_device->Stop();
  EXPECT_CALL(*ipc, CloseStream());
  task_env_.RunUntilIdle();
}

TEST_F(AudioOutputDeviceTest, CreateNondefaultDevice) {
  // The test fixture isn't used in this test, but we still have to clean up
  // after it.
  StopAudioDevice();

  auto params = AudioParameters::UnavailableDeviceParams();
  params.set_frames_per_buffer(kFrames);
  ASSERT_EQ(2, params.channels());
  TestEnvironment env(params);
  auto* ipc = new MockAudioOutputIPC();  // owned by |audio_device|.
  auto audio_device = base::MakeRefCounted<AudioOutputDevice>(
      base::WrapUnique(ipc), task_env_.GetMainThreadTaskRunner(),
      AudioSinkParameters(base::UnguessableToken(), kNonDefaultDeviceId),
      kAuthTimeout);

  audio_device->RequestDeviceAuthorization();
  audio_device->Initialize(params, &env.callback);
  audio_device->Start();
  EXPECT_CALL(*ipc, RequestDeviceAuthorization(audio_device.get(),
                                               base::UnguessableToken(),
                                               kNonDefaultDeviceId));
  EXPECT_CALL(*ipc, CreateStream(audio_device.get(), _, _));
  EXPECT_CALL(*ipc, PlayStream());
  task_env_.RunUntilIdle();
  Mock::VerifyAndClear(ipc);
  audio_device->OnDeviceAuthorized(OUTPUT_DEVICE_STATUS_OK, params,
                                   kNonDefaultDeviceId);
  audio_device->OnStreamCreated(env.reader->TakeSharedMemoryRegion(),
                                env.renderer_socket.Release(),
                                /*playing_automatically*/ false);

  audio_device->Stop();
  EXPECT_CALL(*ipc, CloseStream());
  task_env_.RunUntilIdle();
}

TEST_F(AudioOutputDeviceTest, CreateBitStreamStream) {
  // The test fixture isn't used in this test, but we still have to clean up
  // after it.
  StopAudioDevice();

  const int kAudioParameterFrames = 4321;
  AudioParameters params(AudioParameters::AUDIO_BITSTREAM_EAC3,
                         CHANNEL_LAYOUT_STEREO, 48000, kAudioParameterFrames);

  TestEnvironment env(params);
  auto* ipc = new MockAudioOutputIPC();  // owned by |audio_device|.
  auto audio_device = base::MakeRefCounted<AudioOutputDevice>(
      base::WrapUnique(ipc), task_env_.GetMainThreadTaskRunner(),
      AudioSinkParameters(base::UnguessableToken(), kNonDefaultDeviceId),
      kAuthTimeout);

  // Start a stream.
  audio_device->RequestDeviceAuthorization();
  audio_device->Initialize(params, &env.callback);
  audio_device->Start();
  EXPECT_CALL(*ipc, RequestDeviceAuthorization(audio_device.get(),
                                               base::UnguessableToken(),
                                               kNonDefaultDeviceId));
  EXPECT_CALL(*ipc, CreateStream(audio_device.get(), _, _));
  EXPECT_CALL(*ipc, PlayStream());
  task_env_.RunUntilIdle();
  Mock::VerifyAndClear(ipc);
  audio_device->OnDeviceAuthorized(OUTPUT_DEVICE_STATUS_OK, params,
                                   kNonDefaultDeviceId);
  audio_device->OnStreamCreated(env.reader->TakeSharedMemoryRegion(),
                                env.renderer_socket.Release(),
                                /*playing_automatically*/ false);

  task_env_.RunUntilIdle();
  // At this point, the callback thread should be running. Send some data over
  // and verify that it's propagated to |env.callback|. Do it a few times.
  auto test_bus = AudioBus::Create(params);
  for (int i = 0; i < 10; ++i) {
    test_bus->Zero();
    EXPECT_CALL(env.callback,
                Render(kDelay, env.time_stamp, kFramesSkipped, NotNull()))
        .WillOnce(WithArg<3>(Invoke([](AudioBus* renderer_bus) -> int {
          EXPECT_TRUE(renderer_bus->is_bitstream_format());
          // Place some test data in the bus so that we can check that it was
          // copied to the browser side.
          std::fill_n(renderer_bus->channel(0),
                      kBitstreamDataSize / sizeof(float), kAudioData);
          renderer_bus->SetBitstreamFrames(kBitstreamFrames);
          renderer_bus->SetBitstreamDataSize(kBitstreamDataSize);
          return renderer_bus->frames();
        })));
    env.reader->RequestMoreData(kDelay, env.time_stamp, kFramesSkipped);
    env.reader->Read(test_bus.get());

    Mock::VerifyAndClear(&env.callback);
    EXPECT_TRUE(test_bus->is_bitstream_format());
    EXPECT_EQ(kBitstreamFrames, test_bus->GetBitstreamFrames());
    EXPECT_EQ(kBitstreamDataSize, test_bus->GetBitstreamDataSize());
    for (size_t i = 0; i < kBitstreamDataSize / sizeof(float); ++i) {
      // Note: if all of these fail, the bots will behave strangely due to the
      // large amount of text output. Assert is used to avoid this.
      ASSERT_EQ(kAudioData, test_bus->channel(0)[i]);
    }
  }

  audio_device->Stop();
  EXPECT_CALL(*ipc, CloseStream());
  task_env_.RunUntilIdle();
}

}  // namespace media.
