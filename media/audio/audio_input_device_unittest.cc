// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_device.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/sync_socket.h"
#include "base/test/task_environment.h"
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
  MOCK_METHOD4(Capture,
               void(const AudioBus* audio_source,
                    base::TimeTicks audio_capture_time,
                    double volume,
                    bool key_pressed));

  MOCK_METHOD1(OnCaptureError, void(const std::string& message));
  MOCK_METHOD1(OnCaptureMuted, void(bool is_muted));
};

}  // namespace.

// Regular construction.
TEST(AudioInputDeviceTest, Noop) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  MockAudioInputIPC* input_ipc = new MockAudioInputIPC();
  scoped_refptr<AudioInputDevice> device(new AudioInputDevice(
      base::WrapUnique(input_ipc), AudioInputDevice::Purpose::kUserInput));
}

ACTION_P(ReportStateChange, device) {
  static_cast<AudioInputIPCDelegate*>(device)->OnError();
}

// Verify that we get an OnCaptureError() callback if CreateStream fails.
TEST(AudioInputDeviceTest, FailToCreateStream) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, 48000, 480);

  MockCaptureCallback callback;
  MockAudioInputIPC* input_ipc = new MockAudioInputIPC();
  scoped_refptr<AudioInputDevice> device(new AudioInputDevice(
      base::WrapUnique(input_ipc), AudioInputDevice::Purpose::kUserInput));
  device->Initialize(params, &callback);
  EXPECT_CALL(*input_ipc, CreateStream(_, _, _, _))
      .WillOnce(ReportStateChange(device.get()));
  EXPECT_CALL(callback, OnCaptureError(_));
  device->Start();
  device->Stop();
}

TEST(AudioInputDeviceTest, CreateStream) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, 48000, 480);
  base::MappedReadOnlyRegion shared_memory;
  CancelableSyncSocket browser_socket;
  CancelableSyncSocket renderer_socket;

  const uint32_t memory_size =
      media::ComputeAudioInputBufferSize(params, kMemorySegmentCount);

  shared_memory = base::ReadOnlySharedMemoryRegion::Create(memory_size);
  ASSERT_TRUE(shared_memory.IsValid());
  memset(shared_memory.mapping.memory(), 0xff, memory_size);

  ASSERT_TRUE(
      CancelableSyncSocket::CreatePair(&browser_socket, &renderer_socket));
  SyncSocket::TransitDescriptor audio_device_socket_descriptor;
  ASSERT_TRUE(renderer_socket.PrepareTransitDescriptor(
      base::GetCurrentProcessHandle(), &audio_device_socket_descriptor));
  base::ReadOnlySharedMemoryRegion duplicated_shared_memory_region =
      shared_memory.region.Duplicate();
  ASSERT_TRUE(duplicated_shared_memory_region.IsValid());

  base::test::TaskEnvironment ste;
  MockCaptureCallback callback;
  MockAudioInputIPC* input_ipc = new MockAudioInputIPC();
  scoped_refptr<AudioInputDevice> device(new AudioInputDevice(
      base::WrapUnique(input_ipc), AudioInputDevice::Purpose::kUserInput));
  device->Initialize(params, &callback);

  EXPECT_CALL(*input_ipc, CreateStream(_, _, _, _))
      .WillOnce(InvokeWithoutArgs([&]() {
        static_cast<AudioInputIPCDelegate*>(device.get())
            ->OnStreamCreated(
                std::move(duplicated_shared_memory_region),
                SyncSocket::UnwrapHandle(audio_device_socket_descriptor),
                false);
      }));
  EXPECT_CALL(*input_ipc, RecordStream());

  EXPECT_CALL(callback, OnCaptureStarted());
  device->Start();
  EXPECT_CALL(*input_ipc, CloseStream());
  device->Stop();
}

}  // namespace media.
