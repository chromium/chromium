// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/audio/fake_cras_audio_client.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/cras/audio_manager_cras.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// cras_util.h defines custom min/max macros which break compilation, so ensure
// it's not included until last.  #if avoids presubmit errors.
#if defined(USE_CRAS)
#include "media/audio/cras/cras_input.h"
#endif

using testing::_;
using testing::AtLeast;
using testing::Ge;
using testing::InvokeWithoutArgs;
using testing::StrictMock;

namespace media {

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD3(OnData, void(const AudioBus*, base::TimeTicks, double));
  MOCK_METHOD0(OnError, void());
};

class MockAudioManagerCrasInput : public AudioManagerCras {
 public:
  MockAudioManagerCrasInput()
      : AudioManagerCras(std::make_unique<TestAudioThread>(),
                         &fake_audio_log_factory_) {}

  // We need to override this function in order to skip checking the number
  // of active output streams. It is because the number of active streams
  // is managed inside MakeAudioInputStream, and we don't use
  // MakeAudioInputStream to create the stream in the tests.
  void ReleaseInputStream(AudioInputStream* stream) override {
    DCHECK(stream);
    delete stream;
  }

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
};

class CrasInputStreamTest : public testing::Test {
 protected:
  CrasInputStreamTest() {
    chromeos::CrasAudioClient::InitializeFake();
    chromeos::CrasAudioHandler::InitializeForTesting();
    mock_manager_.reset(new StrictMock<MockAudioManagerCrasInput>());
    base::RunLoop().RunUntilIdle();
  }

  ~CrasInputStreamTest() override {
    mock_manager_->Shutdown();
    chromeos::CrasAudioHandler::Shutdown();
    chromeos::CrasAudioClient::Shutdown();
  }

  CrasInputStream* CreateStream(ChannelLayout layout) {
    return CreateStream(layout, kTestFramesPerPacket);
  }

  CrasInputStream* CreateStream(ChannelLayout layout,
                                int32_t samples_per_packet) {
    return CreateStream(layout, samples_per_packet,
                        AudioDeviceDescription::kDefaultDeviceId);
  }

  CrasInputStream* CreateStream(ChannelLayout layout,
                                int32_t samples_per_packet,
                                const std::string& device_id) {
    AudioParameters params(kTestFormat,
                           layout,
                           kTestSampleRate,
                           samples_per_packet);
    return new CrasInputStream(params, mock_manager_.get(), device_id);
  }

  void CaptureSomeFrames(const AudioParameters &params,
                         unsigned int duration_ms) {
    CrasInputStream* test_stream = new CrasInputStream(
        params, mock_manager_.get(), AudioDeviceDescription::kDefaultDeviceId);

    ASSERT_TRUE(test_stream->Open());

    // Allow 8 frames variance for SRC in the callback.  Different numbers of
    // samples can be provided when doing non-integer SRC.  For example
    // converting from 192k to 44.1k is a ratio of 4.35 to 1.
    MockAudioInputCallback mock_callback;
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);

    EXPECT_CALL(mock_callback, OnData(_, _, _))
        .WillOnce(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal));

    test_stream->Start(&mock_callback);

    // Wait for samples to be captured.
    EXPECT_TRUE(event.TimedWait(TestTimeouts::action_timeout()));

    test_stream->Stop();
    test_stream->Close();
  }

  static const unsigned int kTestCaptureDurationMs;
  static const ChannelLayout kTestChannelLayout;
  static const AudioParameters::Format kTestFormat;
  static const uint32_t kTestFramesPerPacket;
  static const int kTestSampleRate;

  base::TestMessageLoop message_loop_;
  std::unique_ptr<StrictMock<MockAudioManagerCrasInput>> mock_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrasInputStreamTest);
};

const unsigned int CrasInputStreamTest::kTestCaptureDurationMs = 250;
const ChannelLayout CrasInputStreamTest::kTestChannelLayout =
    CHANNEL_LAYOUT_STEREO;
const AudioParameters::Format CrasInputStreamTest::kTestFormat =
    AudioParameters::AUDIO_PCM_LINEAR;
const uint32_t CrasInputStreamTest::kTestFramesPerPacket = 1000;
const int CrasInputStreamTest::kTestSampleRate = 44100;

TEST_F(CrasInputStreamTest, OpenMono) {
  CrasInputStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  EXPECT_TRUE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, OpenStereo) {
  CrasInputStream* test_stream = CreateStream(CHANNEL_LAYOUT_STEREO);
  EXPECT_TRUE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, BadSampleRate) {
  AudioParameters bad_rate_params(kTestFormat,
                                  kTestChannelLayout,
                                  0,
                                  kTestFramesPerPacket);
  CrasInputStream* test_stream =
      new CrasInputStream(bad_rate_params, mock_manager_.get(),
                          AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_FALSE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, SetGetVolume) {
  CrasInputStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  EXPECT_TRUE(test_stream->Open());

  double max_volume = test_stream->GetMaxVolume();
  EXPECT_GE(max_volume, 1.0);

  test_stream->SetVolume(max_volume / 2);

  double new_volume = test_stream->GetVolume();

  EXPECT_GE(new_volume, 0.0);
  EXPECT_LE(new_volume, max_volume);

  test_stream->Close();
}

TEST_F(CrasInputStreamTest, CaptureFrames) {
  const unsigned int rates[] =
      {8000, 16000, 22050, 32000, 44100, 48000, 96000, 192000};

  for (unsigned int i = 0; i < ARRAY_SIZE(rates); i++) {
    SCOPED_TRACE(testing::Message() << "Mono " << rates[i] << "Hz");
    AudioParameters params_mono(kTestFormat,
                                CHANNEL_LAYOUT_MONO,
                                rates[i],
                                kTestFramesPerPacket);
    CaptureSomeFrames(params_mono, kTestCaptureDurationMs);
  }

  for (unsigned int i = 0; i < ARRAY_SIZE(rates); i++) {
    SCOPED_TRACE(testing::Message() << "Stereo " << rates[i] << "Hz");
    AudioParameters params_stereo(kTestFormat,
                                  CHANNEL_LAYOUT_STEREO,
                                  rates[i],
                                  kTestFramesPerPacket);
    CaptureSomeFrames(params_stereo, kTestCaptureDurationMs);
  }
}

TEST_F(CrasInputStreamTest, CaptureLoopback) {
  CrasInputStream* test_stream =
      CreateStream(CHANNEL_LAYOUT_STEREO, kTestFramesPerPacket,
                   AudioDeviceDescription::kLoopbackInputDeviceId);
  EXPECT_TRUE(test_stream->Open());
  test_stream->Close();
}

}  // namespace media
