// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_input.h"

#include <stddef.h>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/base/audio_latency.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_pool.h"

using testing::_;

namespace blink {

constexpr int kSampleRate = 48000;
constexpr int kBufferSize = 8192;
constexpr media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
constexpr char kDefaultDeviceId[] = "default";
constexpr char kAnotherDeviceId[] = "another";
constexpr char kUnauthorizedDeviceId[] = "unauthorized";
constexpr char kNonexistentDeviceId[] = "nonexistent";

class AudioRendererMixerInputTest : public testing::Test,
                                    public AudioRendererMixerPool {
 public:
  AudioRendererMixerInputTest() {
    audio_parameters_ = media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LINEAR,
        media::ChannelLayoutConfig::FromLayout<kChannelLayout>(), kSampleRate,
        kBufferSize);

    CreateMixerInput(kDefaultDeviceId);
    fake_callback_ =
        std::make_unique<media::FakeAudioRenderCallback>(0, kSampleRate);
    audio_bus_ = media::AudioBus::Create(audio_parameters_);
  }

  AudioRendererMixerInputTest(const AudioRendererMixerInputTest&) = delete;
  AudioRendererMixerInputTest& operator=(const AudioRendererMixerInputTest&) =
      delete;

  void CreateMixerInput(const std::string& device_id) {
    mixer_input_ = base::MakeRefCounted<AudioRendererMixerInput>(
        this, LocalFrameToken(), FrameToken(), device_id,
        media::AudioLatency::Type::kPlayback);
    mixer_input_->GetOutputDeviceInfoAsync(base::DoNothing());
    task_environment_.RunUntilIdle();
  }

  AudioRendererMixer* GetMixer(
      const FrameToken&,
      const media::AudioParameters& params,
      media::AudioLatency::Type,
      const media::OutputDeviceInfo& sink_info,
      scoped_refptr<media::AudioRendererSink> sink) override {
    EXPECT_TRUE(params.IsValid());
    size_t idx = (sink_info.device_id() == kDefaultDeviceId) ? 0 : 1;
    if (!mixers_[idx]) {
      EXPECT_CALL(*reinterpret_cast<media::MockAudioRendererSink*>(sink.get()),
                  Start());

      mixers_[idx] = std::make_unique<AudioRendererMixer>(audio_parameters_,
                                                          std::move(sink));
    }
    EXPECT_CALL(*this, ReturnMixer(mixers_[idx].get()));
    return mixers_[idx].get();
  }

  double ProvideInput() {
    return mixer_input_->ProvideInput(audio_bus_.get(), 0, {});
  }

  scoped_refptr<media::AudioRendererSink> GetSink(
      const LocalFrameToken&,
      std::string_view device_id) override {
    media::OutputDeviceStatus status = media::OUTPUT_DEVICE_STATUS_OK;
    if (device_id == kNonexistentDeviceId) {
      status = media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND;
    } else if (device_id == kUnauthorizedDeviceId) {
      status = media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED;
    }
    auto sink = base::MakeRefCounted<media::MockAudioRendererSink>(
        std::string(device_id), status);
    EXPECT_CALL(*sink, Stop());
    return sink;
  }

  MOCK_METHOD1(ReturnMixer, void(AudioRendererMixer*));

  MOCK_METHOD1(SwitchCallbackCalled, void(media::OutputDeviceStatus));
  MOCK_METHOD1(OnDeviceInfoReceived, void(media::OutputDeviceInfo));

  void SwitchCallback(base::RunLoop* loop, media::OutputDeviceStatus result) {
    SwitchCallbackCalled(result);
    loop->Quit();
  }

  AudioRendererMixer* GetInputMixer() { return mixer_input_->mixer_; }
  media::MockAudioRendererSink* GetMockSink() const {
    return reinterpret_cast<media::MockAudioRendererSink*>(
        mixer_input_->sink_.get());
  }

 protected:
  ~AudioRendererMixerInputTest() override = default;

  base::test::SingleThreadTaskEnvironment task_environment_;
  media::AudioParameters audio_parameters_;
  std::unique_ptr<AudioRendererMixer> mixers_[2];
  scoped_refptr<AudioRendererMixerInput> mixer_input_;
  std::unique_ptr<media::FakeAudioRenderCallback> fake_callback_;
  std::unique_ptr<media::AudioBus> audio_bus_;
};

// Test that getting and setting the volume work as expected.  The volume is
// returned from ProvideInput() only when playing.
TEST_F(AudioRendererMixerInputTest, GetSetVolume) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  mixer_input_->Play();

  // Starting volume should be 1.0.
  EXPECT_DOUBLE_EQ(ProvideInput(), 1);

  const double kVolume = 0.5;
  EXPECT_TRUE(mixer_input_->SetVolume(kVolume));
  EXPECT_DOUBLE_EQ(ProvideInput(), kVolume);

  mixer_input_->Stop();
}

// Test Start()/Play()/Pause()/Stop()/playing() all work as expected.  Also
// implicitly tests that AddMixerInput() and RemoveMixerInput() work without
// crashing; functional tests for these methods are in AudioRendererMixerTest.
TEST_F(AudioRendererMixerInputTest, StartPlayPauseStopPlaying) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  mixer_input_->Play();
  EXPECT_DOUBLE_EQ(ProvideInput(), 1);
  mixer_input_->Pause();
  mixer_input_->Play();
  EXPECT_DOUBLE_EQ(ProvideInput(), 1);
  mixer_input_->Stop();
}

// Test that Stop() can be called before Initialize() and Start().
TEST_F(AudioRendererMixerInputTest, StopBeforeInitializeOrStart) {
  mixer_input_->Stop();

  // Verify Stop() works without Initialize() or Start().
  CreateMixerInput(kDefaultDeviceId);
  mixer_input_->Stop();
}

// Test that Start() can be called after Stop().
TEST_F(AudioRendererMixerInputTest, StartAfterStop) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Stop();

  mixer_input_->GetOutputDeviceInfoAsync(base::DoNothing());
  task_environment_.RunUntilIdle();

  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  mixer_input_->Stop();
}

// Test that Initialize() can be called again after Stop().
TEST_F(AudioRendererMixerInputTest, InitializeAfterStop) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  mixer_input_->Stop();

  mixer_input_->GetOutputDeviceInfoAsync(base::DoNothing());
  task_environment_.RunUntilIdle();
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Stop();
}

// Test SwitchOutputDevice().
TEST_F(AudioRendererMixerInputTest, SwitchOutputDevice) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  const std::string kDeviceId("mock-device-id");
  EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
  AudioRendererMixer* old_mixer = GetInputMixer();
  EXPECT_EQ(old_mixer, mixers_[0].get());
  base::RunLoop run_loop;
  mixer_input_->SwitchOutputDevice(
      kDeviceId, base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                                base::Unretained(this), &run_loop));
  run_loop.Run();
  AudioRendererMixer* new_mixer = GetInputMixer();
  EXPECT_EQ(new_mixer, mixers_[1].get());
  EXPECT_NE(old_mixer, new_mixer);
  mixer_input_->Stop();
}

// Test SwitchOutputDevice() to the same device as the current (default) device
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceToSameDevice) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
  AudioRendererMixer* old_mixer = GetInputMixer();
  base::RunLoop run_loop;
  mixer_input_->SwitchOutputDevice(
      kDefaultDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();
  AudioRendererMixer* new_mixer = GetInputMixer();
  EXPECT_EQ(old_mixer, new_mixer);
  mixer_input_->Stop();
}

// Test SwitchOutputDevice() to the new device
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceToAnotherDevice) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
  AudioRendererMixer* old_mixer = GetInputMixer();
  base::RunLoop run_loop;
  mixer_input_->SwitchOutputDevice(
      kAnotherDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();
  AudioRendererMixer* new_mixer = GetInputMixer();
  EXPECT_NE(old_mixer, new_mixer);
  mixer_input_->Stop();
}

// Test that SwitchOutputDevice() to a nonexistent device fails.
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceToNonexistentDevice) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  EXPECT_CALL(
      *this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND));
  base::RunLoop run_loop;
  mixer_input_->SwitchOutputDevice(
      kNonexistentDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();
  mixer_input_->Stop();
}

// Test that SwitchOutputDevice() to an unauthorized device fails.
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceToUnauthorizedDevice) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  EXPECT_CALL(*this, SwitchCallbackCalled(
                         media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED));
  base::RunLoop run_loop;
  mixer_input_->SwitchOutputDevice(
      kUnauthorizedDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();
  mixer_input_->Stop();
}

// Test that calling SwitchOutputDevice() before Start() succeeds.
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceBeforeStart) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  base::RunLoop run_loop;
  EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
  mixer_input_->SwitchOutputDevice(
      kAnotherDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                     base::Unretained(this), &run_loop));
  mixer_input_->Start();
  run_loop.Run();
  mixer_input_->Stop();
}

// Test that calling SwitchOutputDevice() succeeds even if Start() is never
// called.
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceWithoutStart) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  base::RunLoop run_loop;
  EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
  mixer_input_->SwitchOutputDevice(
      kAnotherDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();
  mixer_input_->Stop();
}

// Test that calling SwitchOutputDevice() works after calling Stop(), and that
// restarting works after the call to SwitchOutputDevice().
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceAfterStopBeforeRestart) {
  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  mixer_input_->Stop();

  base::RunLoop run_loop;
  EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
  mixer_input_->SwitchOutputDevice(
      kAnotherDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();

  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  mixer_input_->Stop();
}

// Test that calling SwitchOutputDevice() works before calling Initialize(),
// and that initialization and restart work after the call to
// SwitchOutputDevice().
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceBeforeInitialize) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
  mixer_input_->SwitchOutputDevice(
      kAnotherDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();

  mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  mixer_input_->Start();
  mixer_input_->Stop();
}

// Test that calling SwitchOutputDevice() before
// GetOutputDeviceInfoAsync() works correctly.
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceBeforeGODIA) {
  mixer_input_->Stop();
  mixer_input_ = base::MakeRefCounted<AudioRendererMixerInput>(
      this, LocalFrameToken(), FrameToken(), kDefaultDeviceId,
      media::AudioLatency::Type::kPlayback);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
  mixer_input_->SwitchOutputDevice(
      kAnotherDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();
  mixer_input_->Stop();
}

// Test that calling SwitchOutputDevice() during an ongoing
// GetOutputDeviceInfoAsync() call works correctly.
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceDuringGODIA) {
  mixer_input_->Stop();
  mixer_input_ = base::MakeRefCounted<AudioRendererMixerInput>(
      this, LocalFrameToken(), FrameToken(), kDefaultDeviceId,
      media::AudioLatency::Type::kPlayback);

  mixer_input_->GetOutputDeviceInfoAsync(
      base::BindOnce(&AudioRendererMixerInputTest::OnDeviceInfoReceived,
                     base::Unretained(this)));
  mixer_input_->SwitchOutputDevice(
      kAnotherDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallbackCalled,
                     base::Unretained(this)));
  {
    // Verify that first the GODIA call returns, then the SwitchOutputDevice().
    testing::InSequence sequence_required;
    media::OutputDeviceInfo info;
    constexpr auto kExpectedStatus = media::OUTPUT_DEVICE_STATUS_OK;
    EXPECT_CALL(*this, OnDeviceInfoReceived(_))
        .WillOnce(testing::SaveArg<0>(&info));
    EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
    task_environment_.RunUntilIdle();
    EXPECT_EQ(kExpectedStatus, info.device_status());
    EXPECT_EQ(kDefaultDeviceId, info.device_id());
  }

  mixer_input_->Stop();
}

// Test that calling GetOutputDeviceInfoAsync() during an ongoing
// SwitchOutputDevice() call works correctly.
TEST_F(AudioRendererMixerInputTest, GODIADuringSwitchOutputDevice) {
  mixer_input_->Stop();
  mixer_input_ = base::MakeRefCounted<AudioRendererMixerInput>(
      this, LocalFrameToken(), FrameToken(), kDefaultDeviceId,
      media::AudioLatency::Type::kPlayback);

  mixer_input_->SwitchOutputDevice(
      kAnotherDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallbackCalled,
                     base::Unretained(this)));
  mixer_input_->GetOutputDeviceInfoAsync(
      base::BindOnce(&AudioRendererMixerInputTest::OnDeviceInfoReceived,
                     base::Unretained(this)));

  {
    // Verify that first the SwitchOutputDevice call returns, then the GODIA().
    testing::InSequence sequence_required;
    EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
    media::OutputDeviceInfo info;
    constexpr auto kExpectedStatus = media::OUTPUT_DEVICE_STATUS_OK;
    EXPECT_CALL(*this, OnDeviceInfoReceived(_))
        .WillOnce(testing::SaveArg<0>(&info));
    task_environment_.RunUntilIdle();
    EXPECT_EQ(kExpectedStatus, info.device_status());
    EXPECT_EQ(kAnotherDeviceId, info.device_id());
  }

  mixer_input_->Stop();
}

// Test that calling GetOutputDeviceInfoAsync() during an ongoing
// SwitchOutputDevice() call which eventually fails works correctly.
TEST_F(AudioRendererMixerInputTest, GODIADuringSwitchOutputDeviceWhichFails) {
  mixer_input_->Stop();
  mixer_input_ = base::MakeRefCounted<AudioRendererMixerInput>(
      this, LocalFrameToken(), FrameToken(), kDefaultDeviceId,
      media::AudioLatency::Type::kPlayback);

  mixer_input_->SwitchOutputDevice(
      kNonexistentDeviceId,
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallbackCalled,
                     base::Unretained(this)));
  mixer_input_->GetOutputDeviceInfoAsync(
      base::BindOnce(&AudioRendererMixerInputTest::OnDeviceInfoReceived,
                     base::Unretained(this)));

  {
    // Verify that first the SwitchOutputDevice call returns, then the GODIA().
    testing::InSequence sequence_required;
    EXPECT_CALL(*this, SwitchCallbackCalled(
                           media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND));
    media::OutputDeviceInfo info;
    constexpr auto kExpectedStatus = media::OUTPUT_DEVICE_STATUS_OK;
    EXPECT_CALL(*this, OnDeviceInfoReceived(_))
        .WillOnce(testing::SaveArg<0>(&info));
    task_environment_.RunUntilIdle();
    EXPECT_EQ(kExpectedStatus, info.device_status());
    EXPECT_EQ(kDefaultDeviceId, info.device_id());
  }

  mixer_input_->Stop();
}

// Test that calling SwitchOutputDevice() with an empty device id does nothing
// when we're already on the default device.
TEST_F(AudioRendererMixerInputTest, SwitchOutputDeviceEmptyDeviceId) {
  EXPECT_CALL(*this, SwitchCallbackCalled(media::OUTPUT_DEVICE_STATUS_OK));
  mixer_input_->SwitchOutputDevice(
      std::string(),
      base::BindOnce(&AudioRendererMixerInputTest::SwitchCallbackCalled,
                     base::Unretained(this)));

  // No RunUntilIdle() since switch should immediately return success.
  testing::Mock::VerifyAndClear(this);

  mixer_input_->Stop();
}

}  // namespace blink
