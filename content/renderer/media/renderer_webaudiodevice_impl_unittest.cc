// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <memory>

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
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"

using testing::_;

namespace content {

namespace {

constexpr int kHardwareSampleRate = 44100;
constexpr int kHardwareBufferSize = 128;
const blink::LocalFrameToken kFrameToken;

media::AudioParameters MockGetOutputDeviceParameters(
    const blink::LocalFrameToken& frame_token,
    const base::UnguessableToken& session_id,
    const std::string& device_id) {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kHardwareSampleRate, kHardwareBufferSize);
}

class RendererWebAudioDeviceImplUnderTest : public RendererWebAudioDeviceImpl {
 public:
  RendererWebAudioDeviceImplUnderTest(
      const blink::WebAudioSinkDescriptor& sink_descriptor,
      media::ChannelLayout layout,
      int number_of_output_channels,
      const blink::WebAudioLatencyHint& latency_hint,
      blink::WebAudioDevice::RenderCallback* callback,
      const base::UnguessableToken& session_id)
      : RendererWebAudioDeviceImpl(
            sink_descriptor,
            layout,
            number_of_output_channels,
            latency_hint,
            callback,
            session_id,
            base::BindOnce(&MockGetOutputDeviceParameters)) {}
};

}  // namespace

class RendererWebAudioDeviceImplTest
    : public blink::WebAudioDevice::RenderCallback,
      public blink::AudioDeviceFactory,
      public testing::Test {
 public:
  void Render(const blink::WebVector<float*>& destination_data,
              uint32_t number_of_frames,
              double delay,
              double delay_timestamp,
              size_t prior_frames_skipped) override {
    called_render_ = true;
  }

 protected:
  RendererWebAudioDeviceImplTest() {}

  void SetupDevice(blink::WebAudioLatencyHint latencyHint) {
    blink::WebAudioSinkDescriptor sink_descriptor(
        blink::WebString::FromUTF8(std::string()), kFrameToken);
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, media::CHANNEL_LAYOUT_MONO, 1, latencyHint, this,
        base::UnguessableToken());
    webaudio_device_->SetSilentSinkTaskRunnerForTesting(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  void SetupDevice(media::ChannelLayout layout, int channels) {
    blink::WebAudioSinkDescriptor sink_descriptor(
        blink::WebString::FromUTF8(std::string()), kFrameToken);
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, layout, channels,
        blink::WebAudioLatencyHint(
            blink::WebAudioLatencyHint::kCategoryInteractive),
        this, base::UnguessableToken());
    webaudio_device_->SetSilentSinkTaskRunnerForTesting(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  void SetupDevice(blink::WebAudioSinkDescriptor sink_descriptor) {
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, media::CHANNEL_LAYOUT_MONO, 1,
        blink::WebAudioLatencyHint(
            blink::WebAudioLatencyHint::kCategoryInteractive),
        this, base::UnguessableToken());
    webaudio_device_->SetSilentSinkTaskRunnerForTesting(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
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
  bool called_render_ = false;
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

TEST_F(RendererWebAudioDeviceImplTest, TestSilent) {
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(1, kHardwareBufferSize);

  // The WebAudioSinkDescriptor constructor with frame token will construct a
  // silent sink.
  blink::WebAudioSinkDescriptor sink_descriptor(kFrameToken);
  SetupDevice(sink_descriptor);

  // Test public interface.
  EXPECT_EQ(called_render_, false);
  webaudio_device_->Start();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(called_render_, true);
  webaudio_device_->SampleRate();
  webaudio_device_->FramesPerBuffer();
  webaudio_device_->SetDetectSilence(true);
  webaudio_device_->SetDetectSilence(false);
  webaudio_device_->Pause();
  webaudio_device_->Resume();
  webaudio_device_->Stop();

  // Test repeated calls.
  webaudio_device_->Start();
  webaudio_device_->Start();
  webaudio_device_->Pause();
  webaudio_device_->Pause();
  webaudio_device_->Resume();
  webaudio_device_->Resume();
  webaudio_device_->Stop();
  webaudio_device_->Stop();

  task_environment_.RunUntilIdle();
}

}  // namespace content
