// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/limits.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/media/audio/web_audio_device_factory.h"

using testing::_;

namespace content {

namespace {

const int kHardwareSampleRate = 44100;
const int kHardwareBufferSize = 128;
const blink::LocalFrameToken kFrameToken;

blink::LocalFrameToken MockFrameTokenFromCurrentContext() {
  return kFrameToken;
}

media::AudioParameters MockGetOutputDeviceParameters(
    const blink::LocalFrameToken& frame_token,
    const base::UnguessableToken& session_id,
    const std::string& device_id) {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO,
                                kHardwareSampleRate, kHardwareBufferSize);
}

class RendererWebAudioDeviceImplUnderTest : public RendererWebAudioDeviceImpl {
 public:
  RendererWebAudioDeviceImplUnderTest(
      media::ChannelLayout layout,
      int channels,
      const blink::WebAudioLatencyHint& latency_hint,
      blink::WebAudioDevice::RenderCallback* callback,
      const base::UnguessableToken& session_id)
      : RendererWebAudioDeviceImpl(
            layout,
            channels,
            latency_hint,
            callback,
            session_id,
            base::BindOnce(&MockGetOutputDeviceParameters),
            base::BindOnce(&MockFrameTokenFromCurrentContext)) {}
};

}  // namespace

class RendererWebAudioDeviceImplTest
    : public blink::WebAudioDevice::RenderCallback,
      public blink::WebAudioDeviceFactory,
      public testing::Test {
 protected:
  RendererWebAudioDeviceImplTest() {}

  void SetupDevice(blink::WebAudioLatencyHint latencyHint) {
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        media::CHANNEL_LAYOUT_MONO, 1, latencyHint, this,
        base::UnguessableToken());
    webaudio_device_->SetSuspenderTaskRunnerForTesting(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  void SetupDevice(media::ChannelLayout layout, int channels) {
    webaudio_device_.reset(new RendererWebAudioDeviceImplUnderTest(
        layout, channels,
        blink::WebAudioLatencyHint(
            blink::WebAudioLatencyHint::kCategoryInteractive),
        this, base::UnguessableToken()));
    webaudio_device_->SetSuspenderTaskRunnerForTesting(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  MOCK_METHOD2(CreateAudioCapturerSource,
               scoped_refptr<media::AudioCapturerSource>(
                   const blink::LocalFrameToken&,
                   const media::AudioSourceParameters&));
  MOCK_METHOD3(
      CreateFinalAudioRendererSink,
      scoped_refptr<media::AudioRendererSink>(const blink::LocalFrameToken&,
                                              const media::AudioSinkParameters&,
                                              base::TimeDelta));
  MOCK_METHOD3(CreateSwitchableAudioRendererSink,
               scoped_refptr<media::SwitchableAudioRendererSink>(
                   blink::WebAudioDeviceSourceType,
                   const blink::LocalFrameToken&,
                   const media::AudioSinkParameters&));

  scoped_refptr<media::AudioRendererSink> CreateAudioRendererSink(
      blink::WebAudioDeviceSourceType render_token,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) override {
    scoped_refptr<media::MockAudioRendererSink> mock_sink =
        new media::MockAudioRendererSink(
            params.device_id, media::OUTPUT_DEVICE_STATUS_OK,
            MockGetOutputDeviceParameters(frame_token, params.session_id,
                                          params.device_id));

    EXPECT_CALL(*mock_sink.get(), Start());
    EXPECT_CALL(*mock_sink.get(), Play());
    EXPECT_CALL(*mock_sink.get(), Stop());

    return mock_sink;
  }

  void TearDown() override { webaudio_device_.reset(); }

  std::unique_ptr<RendererWebAudioDeviceImpl> webaudio_device_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(RendererWebAudioDeviceImplTest, ChannelLayout) {
  for (int ch = 1; ch < static_cast<int>(media::limits::kMaxChannels); ++ch) {
    SCOPED_TRACE(base::StringPrintf("ch == %d", ch));

    media::ChannelLayout layout = media::GuessChannelLayout(ch);
    if (layout == media::CHANNEL_LAYOUT_UNSUPPORTED)
      layout = media::CHANNEL_LAYOUT_DISCRETE;

    SetupDevice(layout, ch);
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

}  // namespace content
