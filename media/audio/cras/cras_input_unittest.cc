// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/cras/audio_manager_cras.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// cras_util.h defines custom min/max macros which break compilation, so ensure
// it's not included until last.  #if avoids presubmit errors.
#if BUILDFLAG(USE_CRAS)
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
  MOCK_METHOD4(OnData,
               void(const AudioBus*,
                    base::TimeTicks,
                    double,
                    const AudioGlitchInfo& glitch_info));
  MOCK_METHOD0(OnError, void());
};

class MockAudioManagerCrasInput : public AudioManagerCrasBase {
 public:
  MockAudioManagerCrasInput()
      : AudioManagerCrasBase(std::make_unique<TestAudioThread>(),
                             &fake_audio_log_factory_) {}

  MOCK_METHOD1(RegisterSystemAecDumpSource, void(AecdumpRecordingSource*));

  MOCK_METHOD1(DeregisterSystemAecDumpSource, void(AecdumpRecordingSource*));

  bool HasAudioOutputDevices() { return true; }
  bool HasAudioInputDevices() { return true; }
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) {
    return AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                           ChannelLayoutConfig::Stereo(), 44100, 1000);
  }
  bool IsDefault(const std::string& device_id, bool is_input) override {
    return true;
  }
  enum CRAS_CLIENT_TYPE GetClientType() { return CRAS_CLIENT_TYPE_LACROS; }

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
    mock_manager_.reset(new StrictMock<MockAudioManagerCrasInput>());
    base::RunLoop().RunUntilIdle();
  }

  CrasInputStreamTest(const CrasInputStreamTest&) = delete;
  CrasInputStreamTest& operator=(const CrasInputStreamTest&) = delete;

  ~CrasInputStreamTest() override { mock_manager_->Shutdown(); }

  CrasInputStream* CreateStream(ChannelLayoutConfig layout) {
    return CreateStream(layout, kTestFramesPerPacket);
  }

  CrasInputStream* CreateStream(ChannelLayoutConfig layout,
                                int32_t samples_per_packet) {
    return CreateStream(layout, samples_per_packet,
                        AudioDeviceDescription::kDefaultDeviceId);
  }

  CrasInputStream* CreateStream(ChannelLayoutConfig layout,
                                int32_t samples_per_packet,
                                const std::string& device_id) {
    AudioParameters params(kTestFormat, layout, kTestSampleRate,
                           samples_per_packet);
    return new CrasInputStream(params, mock_manager_.get(), device_id,
                               AudioManager::LogCallback());
  }

  void CaptureSomeFrames(const AudioParameters& params,
                         unsigned int duration_ms) {
    CrasInputStream* test_stream = new CrasInputStream(
        params, mock_manager_.get(), AudioDeviceDescription::kDefaultDeviceId,
        AudioManager::LogCallback());

    EXPECT_CALL(*mock_manager_.get(), RegisterSystemAecDumpSource(_));
    EXPECT_CALL(*mock_manager_.get(), DeregisterSystemAecDumpSource(_));

    EXPECT_EQ(test_stream->Open(), AudioInputStream::OpenOutcome::kSuccess);

    // Allow 8 frames variance for SRC in the callback.  Different numbers of
    // samples can be provided when doing non-integer SRC.  For example
    // converting from 192k to 44.1k is a ratio of 4.35 to 1.
    MockAudioInputCallback mock_callback;
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);

    EXPECT_CALL(mock_callback, OnData(_, _, _, _))
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
};

const unsigned int CrasInputStreamTest::kTestCaptureDurationMs = 250;
constexpr ChannelLayout CrasInputStreamTest::kTestChannelLayout =
    CHANNEL_LAYOUT_STEREO;
const AudioParameters::Format CrasInputStreamTest::kTestFormat =
    AudioParameters::AUDIO_PCM_LINEAR;
const uint32_t CrasInputStreamTest::kTestFramesPerPacket = 1000;
const int CrasInputStreamTest::kTestSampleRate = 44100;

TEST_F(CrasInputStreamTest, OpenMono) {
  CrasInputStream* test_stream = CreateStream(ChannelLayoutConfig::Mono());
  EXPECT_EQ(test_stream->Open(), AudioInputStream::OpenOutcome::kSuccess);
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, OpenStereo) {
  CrasInputStream* test_stream = CreateStream(ChannelLayoutConfig::Stereo());
  EXPECT_EQ(test_stream->Open(), AudioInputStream::OpenOutcome::kSuccess);
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, BadSampleRate) {
  AudioParameters bad_rate_params(
      kTestFormat, ChannelLayoutConfig::FromLayout<kTestChannelLayout>(), 0,
      kTestFramesPerPacket);
  CrasInputStream* test_stream = new CrasInputStream(
      bad_rate_params, mock_manager_.get(),
      AudioDeviceDescription::kDefaultDeviceId, AudioManager::LogCallback());
  EXPECT_EQ(test_stream->Open(), AudioInputStream::OpenOutcome::kFailed);
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, SetGetVolume) {
  CrasInputStream* test_stream = CreateStream(ChannelLayoutConfig::Mono());
  EXPECT_EQ(test_stream->Open(), AudioInputStream::OpenOutcome::kSuccess);

  double max_volume = test_stream->GetMaxVolume();
  EXPECT_GE(max_volume, 1.0);

  test_stream->SetVolume(max_volume / 2);

  double new_volume = test_stream->GetVolume();

  EXPECT_GE(new_volume, 0.0);
  EXPECT_LE(new_volume, max_volume);

  test_stream->Close();
}

TEST_F(CrasInputStreamTest, CaptureFrames) {
  const unsigned int rates[] = {8000,  16000, 22050, 32000,
                                44100, 48000, 96000, 192000};

  for (unsigned int i = 0; i < ARRAY_SIZE(rates); i++) {
    SCOPED_TRACE(testing::Message() << "Mono " << rates[i] << "Hz");
    AudioParameters params_mono(kTestFormat, ChannelLayoutConfig::Mono(),
                                rates[i], kTestFramesPerPacket);
    CaptureSomeFrames(params_mono, kTestCaptureDurationMs);
  }

  for (unsigned int i = 0; i < ARRAY_SIZE(rates); i++) {
    SCOPED_TRACE(testing::Message() << "Stereo " << rates[i] << "Hz");
    AudioParameters params_stereo(kTestFormat, ChannelLayoutConfig::Stereo(),
                                  rates[i], kTestFramesPerPacket);
    CaptureSomeFrames(params_stereo, kTestCaptureDurationMs);
  }
}

TEST_F(CrasInputStreamTest, CaptureLoopback) {
  CrasInputStream* test_stream =
      CreateStream(ChannelLayoutConfig::Stereo(), kTestFramesPerPacket,
                   AudioDeviceDescription::kLoopbackInputDeviceId);
  EXPECT_EQ(test_stream->Open(), AudioInputStream::OpenOutcome::kSuccess);
  test_stream->Close();
}

}  // namespace media
