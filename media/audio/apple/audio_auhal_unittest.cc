// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_message_loop.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;

// TODO(crogers): Most of these tests can be made platform agnostic.
// http://crbug.com/223242

namespace media {

ACTION(ZeroBuffer) {
  arg3->Zero();
}

ACTION_P3(MaybeSignalEvent, counter, signal_at_count, event) {
  if (++(*counter) == signal_at_count)
    event->Signal();
}

class AUHALStreamTest : public testing::Test {
 public:
  AUHALStreamTest()
      : message_loop_(base::MessagePumpType::UI),
        manager_(AudioManager::CreateForTesting(
            std::make_unique<TestAudioThread>())),
        manager_device_info_(manager_.get()) {
    // Wait for the AudioManager to finish any initialization on the audio loop.
    base::RunLoop().RunUntilIdle();
  }

  AUHALStreamTest(const AUHALStreamTest&) = delete;
  AUHALStreamTest& operator=(const AUHALStreamTest&) = delete;

  ~AUHALStreamTest() override { manager_->Shutdown(); }

  AudioOutputStream* Create() {
    std::string default_device_id =
        manager_device_info_.GetDefaultOutputDeviceID();
    AudioParameters default_params =
        manager_device_info_.GetOutputStreamParameters(default_device_id);
    return manager_->MakeAudioOutputStream(
        default_params, "",
        base::BindRepeating(&AUHALStreamTest::OnLogMessage,
                            base::Unretained(this)));
  }

  bool OutputDevicesAvailable() {
    return manager_device_info_.HasAudioOutputDevices();
  }

  void OnLogMessage(const std::string& message) { log_message_ = message; }

 protected:
  base::TestMessageLoop message_loop_;
  std::unique_ptr<AudioManager> manager_;
  AudioDeviceInfoAccessorForTests manager_device_info_;
  MockAudioSourceCallback source_;
  std::string log_message_;
};

TEST_F(AUHALStreamTest, HardwareSampleRate) {
  ABORT_AUDIO_TEST_IF_NOT(OutputDevicesAvailable());
  std::string default_device_id =
      manager_device_info_.GetDefaultOutputDeviceID();
  const AudioParameters preferred_params =
      manager_device_info_.GetOutputStreamParameters(default_device_id);
  EXPECT_GE(preferred_params.sample_rate(), 16000);
  EXPECT_LE(preferred_params.sample_rate(), 192000);
}

TEST_F(AUHALStreamTest, CreateClose) {
  ABORT_AUDIO_TEST_IF_NOT(OutputDevicesAvailable());
  Create()->Close();
}

TEST_F(AUHALStreamTest, CreateOpenClose) {
  ABORT_AUDIO_TEST_IF_NOT(OutputDevicesAvailable());
  AudioOutputStream* stream = Create();
  EXPECT_TRUE(stream->Open());
  stream->Close();
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/40283968): audio output unit startup fails with partition
// alloc.
#define MAYBE_CreateOpenStartStopClose DISABLED_CreateOpenStartStopClose
#else
#define MAYBE_CreateOpenStartStopClose CreateOpenStartStopClose
#endif
TEST_F(AUHALStreamTest, MAYBE_CreateOpenStartStopClose) {
  ABORT_AUDIO_TEST_IF_NOT(OutputDevicesAvailable());

  AudioOutputStream* stream = Create();
  EXPECT_TRUE(stream->Open());

  // Wait for the first two data callback from the OS.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  int callback_counter = 0;
  const int number_of_callbacks = 2;
  EXPECT_CALL(source_, OnMoreData(_, _, _, _))
      .Times(number_of_callbacks)
      .WillRepeatedly(DoAll(
          ZeroBuffer(),
          MaybeSignalEvent(&callback_counter, number_of_callbacks, &event),
          Return(0)));
  EXPECT_CALL(source_, OnError(_)).Times(0);
  stream->Start(&source_);
  event.Wait();

  stream->Stop();
  stream->Close();

  EXPECT_FALSE(log_message_.empty());
}

}  // namespace media
