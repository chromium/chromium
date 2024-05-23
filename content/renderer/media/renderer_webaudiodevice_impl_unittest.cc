// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/limits.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/output_device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"

using ::testing::_;
using ::testing::InSequence;

namespace content {

namespace {

class MockAudioRendererSink : public media::AudioRendererSink {
 public:
  explicit MockAudioRendererSink() = default;
  void Initialize(const media::AudioParameters& params,
                  media::AudioRendererSink::RenderCallback* callback) override {
    callback_ = callback;
  }
  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Play, (), (override));
  MOCK_METHOD(void, Flush, (), (override));
  MOCK_METHOD(bool, SetVolume, (double volume), (override));
  MOCK_METHOD(media::OutputDeviceInfo, GetOutputDeviceInfo, (), (override));
  MOCK_METHOD(void,
              GetOutputDeviceInfoAsync,
              (OutputDeviceInfoCB info_cb),
              (override));
  MOCK_METHOD(bool, IsOptimizedForHardwareParameters, (), (override));
  MOCK_METHOD(bool, CurrentThreadIsRenderingThread, (), (override));

  raw_ptr<media::AudioRendererSink::RenderCallback, DanglingUntriaged>
      callback_ = nullptr;

 private:
  ~MockAudioRendererSink() override = default;
};

constexpr int kHardwareSampleRate = 44100;
constexpr int kHardwareBufferSize = 128;
const blink::LocalFrameToken kFrameToken;
const media::OutputDeviceInfo kHealthyDevice(
    media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
const media::OutputDeviceInfo kErrorDevice(
    media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);

media::AudioParameters MockGetOutputDeviceParameters(
    const blink::LocalFrameToken& frame_token,
    const std::string& device_id) {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kHardwareSampleRate, kHardwareBufferSize);
}

class RendererWebAudioDeviceImplUnderTest : public RendererWebAudioDeviceImpl {
 public:
  RendererWebAudioDeviceImplUnderTest(
      const blink::WebAudioSinkDescriptor& sink_descriptor,
      media::ChannelLayoutConfig layout_config,
      const blink::WebAudioLatencyHint& latency_hint,
      media::AudioRendererSink::RenderCallback* callback,
      CreateSilentSinkCallback silent_sink_callback)
      : RendererWebAudioDeviceImpl(
            sink_descriptor,
            layout_config,
            latency_hint,
            callback,
            base::BindOnce(&MockGetOutputDeviceParameters),
            std::move(silent_sink_callback)) {}
};

}  // namespace

class RendererWebAudioDeviceImplTest
    : public media::AudioRendererSink::RenderCallback,
      public blink::AudioDeviceFactory,
      public testing::Test {
 public:
  MOCK_METHOD(int,
              Render,
              (base::TimeDelta delay,
               base::TimeTicks delay_timestamp,
               const media::AudioGlitchInfo& glitch_info,
               media::AudioBus* dest),
              (override));

  void OnRenderError() override {}

 protected:
  RendererWebAudioDeviceImplTest() {
    mock_audio_renderer_sink_ = base::MakeRefCounted<MockAudioRendererSink>();
  }

  scoped_refptr<media::AudioRendererSink> CreateMockSilentSink(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
    return mock_audio_renderer_sink_;
  }

  void SetupDevice(blink::WebAudioLatencyHint latencyHint) {
    blink::WebAudioSinkDescriptor sink_descriptor(
        blink::WebString::FromUTF8(std::string()), kFrameToken);
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, media::ChannelLayoutConfig::Mono(), latencyHint, this,
        base::BindRepeating(
            &RendererWebAudioDeviceImplTest::CreateMockSilentSink,
            // Guaranteed to be valid because |this| owns |webaudio_device_| and
            // so will outlive it.
            base::Unretained(this)));
    webaudio_device_->SetSilentSinkTaskRunnerForTesting(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  void SetupDevice(media::ChannelLayoutConfig layout_config) {
    blink::WebAudioSinkDescriptor sink_descriptor(
        blink::WebString::FromUTF8(std::string()), kFrameToken);
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, layout_config,
        blink::WebAudioLatencyHint(
            blink::WebAudioLatencyHint::kCategoryInteractive),
        this,
        base::BindRepeating(
            &RendererWebAudioDeviceImplTest::CreateMockSilentSink,
            // Guaranteed to be valid because |this| owns |webaudio_device_| and
            // so will outlive it.
            base::Unretained(this)));
    webaudio_device_->SetSilentSinkTaskRunnerForTesting(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  void SetupDevice(blink::WebAudioSinkDescriptor sink_descriptor) {
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, media::ChannelLayoutConfig::Mono(),
        blink::WebAudioLatencyHint(
            blink::WebAudioLatencyHint::kCategoryInteractive),
        this,
        base::BindRepeating(
            &RendererWebAudioDeviceImplTest::CreateMockSilentSink,
            // Guaranteed to be valid because |this| owns |webaudio_device_| and
            // so will outlive it.
            base::Unretained(this)));
    webaudio_device_->SetSilentSinkTaskRunnerForTesting(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType render_token,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) override {
    return mock_audio_renderer_sink_;
  }

  void TearDown() override { webaudio_device_.reset(); }

  std::unique_ptr<RendererWebAudioDeviceImpl> webaudio_device_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<MockAudioRendererSink> mock_audio_renderer_sink_;
};

TEST_F(RendererWebAudioDeviceImplTest, ChannelLayout) {
  for (int ch = 1; ch < static_cast<int>(media::limits::kMaxChannels); ++ch) {
    SCOPED_TRACE(base::StringPrintf("ch == %d", ch));

    media::ChannelLayout layout = media::GuessChannelLayout(ch);
    if (layout == media::CHANNEL_LAYOUT_UNSUPPORTED)
      layout = media::CHANNEL_LAYOUT_DISCRETE;

    SetupDevice({layout, ch});
    media::AudioParameters sink_params =
        webaudio_device_->get_sink_params_for_testing();
    EXPECT_TRUE(sink_params.IsValid());
    EXPECT_EQ(layout, sink_params.channel_layout());
    EXPECT_EQ(ch, sink_params.channels());
  }
}

TEST_F(RendererWebAudioDeviceImplTest, TestLatencyHintValues) {
  blink::WebAudioLatencyHint interactiveLatencyHint(
      blink::WebAudioLatencyHint::kCategoryInteractive);
  int interactiveBufferSize =
      media::AudioLatency::GetInteractiveBufferSize(kHardwareBufferSize);
  SetupDevice(interactiveLatencyHint);

  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), interactiveBufferSize);

  webaudio_device_->Start();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), interactiveBufferSize);

  webaudio_device_->Stop();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), interactiveBufferSize);

  webaudio_device_->Start();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), interactiveBufferSize);

  webaudio_device_->Stop();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), interactiveBufferSize);

  blink::WebAudioLatencyHint balancedLatencyHint(
      blink::WebAudioLatencyHint::kCategoryBalanced);
  int balancedBufferSize = media::AudioLatency::GetRtcBufferSize(
      kHardwareSampleRate, kHardwareBufferSize);
  SetupDevice(balancedLatencyHint);

  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), balancedBufferSize);

  webaudio_device_->Start();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), balancedBufferSize);

  webaudio_device_->Stop();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), balancedBufferSize);

  webaudio_device_->Start();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), balancedBufferSize);

  webaudio_device_->Stop();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), balancedBufferSize);

  blink::WebAudioLatencyHint playbackLatencyHint(
      blink::WebAudioLatencyHint::kCategoryPlayback);
  int playbackBufferSize = media::AudioLatency::GetHighLatencyBufferSize(
      kHardwareSampleRate, kHardwareBufferSize);
  SetupDevice(playbackLatencyHint);

  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), playbackBufferSize);

  webaudio_device_->Start();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), playbackBufferSize);

  webaudio_device_->Stop();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), playbackBufferSize);

  webaudio_device_->Start();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), playbackBufferSize);

  webaudio_device_->Stop();
  EXPECT_EQ(webaudio_device_->SampleRate(), kHardwareSampleRate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), playbackBufferSize);

  EXPECT_GE(playbackBufferSize, balancedBufferSize);
  EXPECT_GE(balancedBufferSize, interactiveBufferSize);
}

TEST_F(RendererWebAudioDeviceImplTest, NullSink_RenderWorks) {
  {
    InSequence s;

    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*this, Render).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  // The WebAudioSinkDescriptor constructor with frame token will construct a
  // silent sink.
  SetupDevice(blink::WebAudioSinkDescriptor(kFrameToken));
  webaudio_device_->Start();
  mock_audio_renderer_sink_->callback_->Render(
      base::TimeDelta::Min(), base::TimeTicks::Now(), {},
      media::AudioBus::Create(1, kHardwareBufferSize).get());
  webaudio_device_->Stop();
}

TEST_F(RendererWebAudioDeviceImplTest, NullSink_PauseResumeWorks) {
  {
    InSequence s;

    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Pause).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  // The WebAudioSinkDescriptor constructor with frame token will construct a
  // silent sink.
  SetupDevice(blink::WebAudioSinkDescriptor(kFrameToken));
  webaudio_device_->Start();
  webaudio_device_->Pause();
  webaudio_device_->Resume();
  webaudio_device_->Stop();
}

TEST_F(RendererWebAudioDeviceImplTest,
       NullSink_StartRenderStopStartRenderStopWorks) {
  {
    InSequence s;

    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*this, Render).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*this, Render).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  // The WebAudioSinkDescriptor constructor with frame token will construct a
  // silent sink.
  SetupDevice(blink::WebAudioSinkDescriptor(kFrameToken));
  webaudio_device_->Start();
  mock_audio_renderer_sink_->callback_->Render(
      base::TimeDelta::Min(), base::TimeTicks::Now(), {},
      media::AudioBus::Create(1, kHardwareBufferSize).get());
  webaudio_device_->Stop();
  webaudio_device_->Start();
  mock_audio_renderer_sink_->callback_->Render(
      base::TimeDelta::Min(), base::TimeTicks::Now(), {},
      media::AudioBus::Create(1, kHardwareBufferSize).get());
  webaudio_device_->Stop();
}

TEST_F(RendererWebAudioDeviceImplTest, NullSink_RepeatedStartWorks) {
  {
    InSequence s;

    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  // The WebAudioSinkDescriptor constructor with frame token will construct a
  // silent sink.
  SetupDevice(blink::WebAudioSinkDescriptor(kFrameToken));
  webaudio_device_->Start();
  webaudio_device_->Start();
  webaudio_device_->Stop();
}

TEST_F(RendererWebAudioDeviceImplTest, NullSink_RepeatedPauseWorks) {
  {
    InSequence s;

    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Pause).Times(2);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  // The WebAudioSinkDescriptor constructor with frame token will construct a
  // silent sink.
  SetupDevice(blink::WebAudioSinkDescriptor(kFrameToken));
  webaudio_device_->Start();
  webaudio_device_->Pause();
  webaudio_device_->Pause();
  webaudio_device_->Stop();
}

TEST_F(RendererWebAudioDeviceImplTest, NullSink_RepeatedResumeWorks) {
  {
    InSequence s;

    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Pause).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(2);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  // The WebAudioSinkDescriptor constructor with frame token will construct a
  // silent sink.
  SetupDevice(blink::WebAudioSinkDescriptor(kFrameToken));
  webaudio_device_->Start();
  webaudio_device_->Pause();
  webaudio_device_->Resume();
  webaudio_device_->Resume();
  webaudio_device_->Stop();
}

TEST_F(RendererWebAudioDeviceImplTest, NullSink_RepeatedStopWorks) {
  {
    InSequence s;

    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  // The WebAudioSinkDescriptor constructor with frame token will construct a
  // silent sink.
  SetupDevice(blink::WebAudioSinkDescriptor(kFrameToken));
  webaudio_device_->Start();
  webaudio_device_->Stop();
  webaudio_device_->Stop();
}

TEST_F(RendererWebAudioDeviceImplTest,
       CreateSinkAndGetDeviceStatus_HealthyDevice) {
  {
    InSequence s;

    EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo)
        .Times(1)
        .WillOnce(testing::Return(kHealthyDevice));
    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  SetupDevice(media::ChannelLayoutConfig::Stereo());

  // `sink_` should be created after OUTPUT_DEVICE_STATUS_OK status return from
  // `CreateAndGetSinkStatus` call.
  EXPECT_EQ(webaudio_device_->sink_, nullptr);
  media::OutputDeviceStatus status =
      webaudio_device_->MaybeCreateSinkAndGetStatus();
  EXPECT_NE(webaudio_device_->sink_, nullptr);

  // Healthy device should return OUTPUT_DEVICE_STATUS_OK.
  EXPECT_EQ(status, media ::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
  webaudio_device_->Start();
  webaudio_device_->Stop();
}

TEST_F(RendererWebAudioDeviceImplTest,
       CreateSinkAndGetDeviceStatus_ErrorDevice) {
  {
    InSequence s;

    EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo)
        .Times(1)
        .WillOnce(testing::Return(kErrorDevice));
    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(0);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(0);
    // Stop() is necessary before destruction per AudioRendererSink contract.
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  SetupDevice(media::ChannelLayoutConfig::Stereo());

  // `sink_` should be remain as nullptr after
  // OUTPUT_DEVICE_STATUS_ERROR_INTERNAL status return from
  // `CreateAndGetSinkStatus` call.
  EXPECT_EQ(webaudio_device_->sink_, nullptr);
  media::OutputDeviceStatus status =
      webaudio_device_->MaybeCreateSinkAndGetStatus();
  EXPECT_EQ(webaudio_device_->sink_, nullptr);

  // Error device should return OUTPUT_DEVICE_STATUS_ERROR_INTERNAL.
  EXPECT_EQ(status,
            media ::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
}

TEST_F(RendererWebAudioDeviceImplTest,
       CreateSinkAndGetDeviceStatus_SilentSink) {
  {
    InSequence s;

    // Silent sink shouldn't invoke `GetOutputDeviceInfo`.
    EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo).Times(0);
    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  // The WebAudioSinkDescriptor constructor with frame token will construct a
  // silent sink.
  SetupDevice(blink::WebAudioSinkDescriptor(kFrameToken));

  // `sink_` should be created after OUTPUT_DEVICE_STATUS_OK status return from
  // `CreateAndGetSinkStatus` call.
  EXPECT_EQ(webaudio_device_->sink_, nullptr);
  media::OutputDeviceStatus status =
      webaudio_device_->MaybeCreateSinkAndGetStatus();
  EXPECT_NE(webaudio_device_->sink_, nullptr);

  // Silent sink should return OUTPUT_DEVICE_STATUS_OK.
  EXPECT_EQ(status, media ::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
  webaudio_device_->Start();
  webaudio_device_->Stop();
}
}  // namespace content
