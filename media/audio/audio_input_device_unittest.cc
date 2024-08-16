// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_device.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/audio_glitch_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::CancelableSyncSocket;
using base::SyncSocket;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace media {

namespace {

const size_t kMemorySegmentCount = 10u;

class MockAudioInputIPC : public AudioInputIPC {
 public:
  MockAudioInputIPC() = default;
  ~MockAudioInputIPC() override = default;

  MOCK_METHOD4(CreateStream,
               void(AudioInputIPCDelegate* delegate,
                    const AudioParameters& params,
                    bool automatic_gain_control,
                    uint32_t total_segments));
  MOCK_METHOD0(RecordStream, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(SetOutputDeviceForAec, void(const std::string&));
  MOCK_METHOD0(CloseStream, void());
};

class MockCaptureCallback : public AudioCapturerSource::CaptureCallback {
 public:
  MockCaptureCallback() = default;
  ~MockCaptureCallback() override = default;

  MOCK_METHOD0(OnCaptureStarted, void());
  MOCK_METHOD5(Capture,
               void(const AudioBus* audio_source,
                    base::TimeTicks audio_capture_time,
                    const AudioGlitchInfo& glitch_info,
                    double volume,
                    bool key_pressed));

  MOCK_METHOD2(OnCaptureError,
               void(AudioCapturerSource::ErrorCode code,
                    const std::string& message));
  MOCK_METHOD1(OnCaptureMuted, void(bool is_muted));
};

// Verifies that the capture time and glitch info passed to Capture() are
// correct.
class AssertingCaptureCallback : public AudioCapturerSource::CaptureCallback {
 public:
  AssertingCaptureCallback() = default;
  ~AssertingCaptureCallback() override = default;

  MOCK_METHOD2(VerifyCapture,
               void(base::TimeTicks audio_capture_time,
                    const AudioGlitchInfo& glitch_info));

  void Capture(const AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               const AudioGlitchInfo& glitch_info,
               double volume,
               bool key_pressed) override {
    VerifyCapture(audio_capture_time, glitch_info);
    capture_called_event_.Signal();
  }

  void WaitForCapture() { capture_called_event_.Wait(); }

  MOCK_METHOD0(OnCaptureStarted, void());
  MOCK_METHOD2(OnCaptureError,
               void(AudioCapturerSource::ErrorCode code,
                    const std::string& message));
  MOCK_METHOD1(OnCaptureMuted, void(bool is_muted));

 private:
  base::WaitableEvent capture_called_event_;
};

}  // namespace.

class AudioInputDeviceTest
    : public ::testing::TestWithParam<AudioInputDevice::DeadStreamDetection> {
 protected:
  void CreateInputDevice() {
    AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                           ChannelLayoutConfig::Stereo(), 48000, 480);

    const uint32_t memory_size =
        ComputeAudioInputBufferSize(params, kMemorySegmentCount);

    shared_memory_ = base::ReadOnlySharedMemoryRegion::Create(memory_size);
    ASSERT_TRUE(shared_memory_.IsValid());
    memset(shared_memory_.mapping.memory(), 0xff, memory_size);

    ASSERT_TRUE(
        CancelableSyncSocket::CreatePair(&browser_socket_, &renderer_socket_));

    MockAudioInputIPC* input_ipc = new MockAudioInputIPC();

    device_ = base::MakeRefCounted<AudioInputDevice>(
        base::WrapUnique(input_ipc), AudioInputDevice::Purpose::kUserInput,
        AudioInputDeviceTest::GetParam());

    capture_callback_.emplace();
    device_->Initialize(params, &capture_callback_.value());

    EXPECT_CALL(*input_ipc, CreateStream(_, _, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
          auto duplicated_shared_memory_region =
              shared_memory_.region.Duplicate();
          CHECK(duplicated_shared_memory_region.IsValid());
          static_cast<AudioInputIPCDelegate*>(device_.get())
              ->OnStreamCreated(std::move(duplicated_shared_memory_region),
                                renderer_socket_.Take(), false);
        }));
    EXPECT_CALL(*input_ipc, RecordStream());
    EXPECT_CALL(*capture_callback_, OnCaptureStarted());
    EXPECT_CALL(*input_ipc, CloseStream());

    uint8_t* ptr = static_cast<uint8_t*>(shared_memory_.mapping.memory());
    AudioInputBuffer* buffer = reinterpret_cast<AudioInputBuffer*>(ptr);
    buffer->params.id = 0;
    buffer->params.capture_time_us =
        (capture_time_ - base::TimeTicks()).InMicroseconds();
    buffer->params.glitch_duration_us = glitch_info_.duration.InMicroseconds();
    buffer->params.glitch_count = glitch_info_.count;
    buffer->params.key_pressed = false;
  }

  base::MappedReadOnlyRegion shared_memory_;
  CancelableSyncSocket browser_socket_;
  CancelableSyncSocket renderer_socket_;
  std::optional<AssertingCaptureCallback> capture_callback_;
  scoped_refptr<AudioInputDevice> device_;
  const base::TimeTicks capture_time_ =
      base::TimeTicks() + base::Microseconds(123);
  const AudioGlitchInfo glitch_info_{.duration = base::Microseconds(20000),
                                     .count = 2};
};

// Regular construction.
TEST_P(AudioInputDeviceTest, Noop) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  MockAudioInputIPC* input_ipc = new MockAudioInputIPC();
  scoped_refptr<AudioInputDevice> device(new AudioInputDevice(
      base::WrapUnique(input_ipc), AudioInputDevice::Purpose::kUserInput,
      AudioInputDeviceTest::GetParam()));
}

ACTION_P(ReportStateChange, device) {
  static_cast<AudioInputIPCDelegate*>(device)->OnError(
      AudioCapturerSource::ErrorCode::kUnknown);
}

// Verify that we get an OnCaptureError() callback if CreateStream fails.
TEST_P(AudioInputDeviceTest, FailToCreateStream) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Stereo(), 48000, 480);

  MockCaptureCallback callback;
  MockAudioInputIPC* input_ipc = new MockAudioInputIPC();
  scoped_refptr<AudioInputDevice> device(new AudioInputDevice(
      base::WrapUnique(input_ipc), AudioInputDevice::Purpose::kUserInput,
      AudioInputDeviceTest::GetParam()));
  device->Initialize(params, &callback);
  EXPECT_CALL(*input_ipc, CreateStream(_, _, _, _))
      .WillOnce(ReportStateChange(device.get()));
  EXPECT_CALL(callback,
              OnCaptureError(AudioCapturerSource::ErrorCode::kUnknown, _));
  EXPECT_CALL(*input_ipc, CloseStream());
  device->Start();
  device->Stop();
}

TEST_P(AudioInputDeviceTest, CreateStream) {
  base::test::TaskEnvironment ste;
  CreateInputDevice();

  device_->Start();
  device_->Stop();
}

TEST_P(AudioInputDeviceTest, CaptureCallback) {
  base::test::TaskEnvironment ste;
  CreateInputDevice();

  uint32_t buffer_index = 0;
  browser_socket_.Send(base::byte_span_from_ref(buffer_index));

  EXPECT_CALL(*capture_callback_, OnCaptureError(_, _)).Times(0);
  EXPECT_CALL(*capture_callback_, VerifyCapture(capture_time_, glitch_info_));

  device_->Start();
  ste.RunUntilIdle();

  // The capture occurs on another thread, wait for it.
  capture_callback_->WaitForCapture();

  device_->Stop();
}

TEST_P(AudioInputDeviceTest, CaptureCallbackSocketError) {
  base::test::TaskEnvironment ste;
  CreateInputDevice();

  uint32_t buffer_index = 0;
  browser_socket_.Send(base::byte_span_from_ref(buffer_index));

  EXPECT_CALL(*capture_callback_,
              OnCaptureError(AudioCapturerSource::ErrorCode::kSocketError, _))
      .WillOnce(base::test::RunClosure(ste.QuitClosure()));

  device_->Start();
  ste.RunUntilIdle();

  // The capture occurs on another thread, wait for it.
  capture_callback_->WaitForCapture();

  browser_socket_.Close();
  ste.RunUntilQuit();

  device_->Stop();
}

INSTANTIATE_TEST_SUITE_P(
    AudioInputDeviceGroup,
    AudioInputDeviceTest,
    ::testing::Values(AudioInputDevice::DeadStreamDetection::kDisabled,
                      AudioInputDevice::DeadStreamDetection::kEnabled));

}  // namespace media.
