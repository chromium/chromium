// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/limits.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/output_device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"

namespace content {

namespace {

using ::media::limits::kMaxWebAudioBufferSize;
using ::media::limits::kMinWebAudioBufferSize;
using ::testing::_;
using ::testing::InSequence;

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
      std::optional<float> context_sample_rate,
      media::AudioRendererSink::RenderCallback* callback,
      CreateSilentSinkCallback silent_sink_callback)
      : RendererWebAudioDeviceImpl(
            sink_descriptor,
            layout_config,
            latency_hint,
            context_sample_rate,
            callback,
            base::BindOnce(&MockGetOutputDeviceParameters),
            std::move(silent_sink_callback)) {}
};
class RendererWebAudioDeviceImplConstructorParamTest
    : public RendererWebAudioDeviceImpl {
 public:
  RendererWebAudioDeviceImplConstructorParamTest(
      const blink::WebAudioSinkDescriptor& sink_descriptor,
      media::ChannelLayoutConfig layout_config,
      const blink::WebAudioLatencyHint& latency_hint,
      std::optional<float> context_sample_rate,
      media::AudioRendererSink::RenderCallback* callback,
      CreateSilentSinkCallback silent_sink_callback,
      base::RepeatingCallback<
          media::AudioParameters(const blink::LocalFrameToken&,
                                 const std::string&)> device_params_cb);
};

RendererWebAudioDeviceImplConstructorParamTest::
    RendererWebAudioDeviceImplConstructorParamTest(
        const blink::WebAudioSinkDescriptor& sink_descriptor,
        media::ChannelLayoutConfig layout_config,
        const blink::WebAudioLatencyHint& latency_hint,
        std::optional<float> context_sample_rate,
        media::AudioRendererSink::RenderCallback* callback,
        CreateSilentSinkCallback silent_sink_callback,
        base::RepeatingCallback<
            media::AudioParameters(const blink::LocalFrameToken&,
                                   const std::string&)> device_params_cb)
    : RendererWebAudioDeviceImpl(sink_descriptor,
                                 layout_config,
                                 latency_hint,
                                 context_sample_rate,
                                 callback,
                                 std::move(device_params_cb),
                                 std::move(silent_sink_callback)) {}

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

  scoped_refptr<media::AudioRendererSink> CreateMockSilentSink(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
    return mock_audio_renderer_sink_;
  }

 protected:
  RendererWebAudioDeviceImplTest() {
    mock_audio_renderer_sink_ = base::MakeRefCounted<MockAudioRendererSink>();
  }


  void SetupDevice(blink::WebAudioLatencyHint latencyHint) {
    blink::WebAudioSinkDescriptor sink_descriptor(
        blink::WebString::FromUTF8(std::string()), kFrameToken);
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, media::ChannelLayoutConfig::Mono(), latencyHint,
        context_sample_rate_, this,
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
        context_sample_rate_, this,
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
        context_sample_rate_, this,
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
  std::optional<float> context_sample_rate_;
};

class RendererWebAudioDeviceImplBufferSizeTest : public ::testing::Test {
 protected:
  static constexpr int kHardwareSampleRate48k = 48000;
  static constexpr int kHardwareBufferSize48k = 480;
  static constexpr int kHardwareSampleRate44k = 44100;
  static constexpr int kHardwareBufferSize44k = 441;
  base::test::ScopedFeatureList feature_list_;
};

// When the kWebAudioRemoveAudioDestinationResampler
// feature is disabled, the GetOutputBufferSize method returns the default
// hardware buffer size, regardless of the context sample rate.
TEST_F(RendererWebAudioDeviceImplBufferSizeTest,
       InteractiveLatency_FeatureDisabled_UsesDefaultBufferSize) {
  feature_list_.InitAndDisableFeature(
      blink::features::kWebAudioRemoveAudioDestinationResampler);
  blink::WebAudioLatencyHint latency_hint(
      blink::WebAudioLatencyHint::kCategoryInteractive);
  media::AudioParameters hardware_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kHardwareSampleRate48k,
      kHardwareBufferSize48k);

  // When feature is disabled, we ensure the context_sample_rate is using
  // default hardware sample rate before calling
  // `RendererWebAudioDeviceImpl::GetOutputBufferSize`.
  int context_sample_rate = kHardwareSampleRate48k;
  int output_buffer_size = RendererWebAudioDeviceImpl::GetOutputBufferSize(
      latency_hint, context_sample_rate, hardware_params);
  EXPECT_EQ(output_buffer_size, 480);
}

// When the kWebAudioRemoveAudioDestinationResampler
// feature is enabled and the context sample rate matches the hardware sample
// rate, the GetOutputBufferSize method returns the default hardware buffer
// size.
TEST_F(RendererWebAudioDeviceImplBufferSizeTest,
       InteractiveLatency_SameSampleRate_ReturnsDefaultBufferSize) {
  feature_list_.InitAndEnableFeature(
      blink::features::kWebAudioRemoveAudioDestinationResampler);
  blink::WebAudioLatencyHint latency_hint(
      blink::WebAudioLatencyHint::kCategoryInteractive);
  media::AudioParameters hardware_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kHardwareSampleRate48k,
      kHardwareBufferSize48k);

  int context_sample_rate = 48000;
  int output_buffer_size = RendererWebAudioDeviceImpl::GetOutputBufferSize(
      latency_hint, context_sample_rate, hardware_params);
  EXPECT_EQ(output_buffer_size, 480);
}

// When the kWebAudioRemoveAudioDestinationResampler
// feature is enabled and the context sample rate is significantly lower than
// the hardware sample rate, the GetOutputBufferSize method returns the minimum
// allowed buffer size (kMinWebAudioBufferSize). This ensures that the scaled
// buffer size is capped at the minimum.
TEST_F(RendererWebAudioDeviceImplBufferSizeTest,
       InteractiveLatency_LowSampleRate_CapsAtMinBufferSize) {
  feature_list_.InitAndEnableFeature(
      blink::features::kWebAudioRemoveAudioDestinationResampler);
  blink::WebAudioLatencyHint latency_hint(
      blink::WebAudioLatencyHint::kCategoryInteractive);
  media::AudioParameters hardware_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kHardwareSampleRate48k,
      kHardwareBufferSize48k);

  int context_sample_rate = 8000;
  int output_buffer_size = RendererWebAudioDeviceImpl::GetOutputBufferSize(
      latency_hint, context_sample_rate, hardware_params);
  EXPECT_EQ(output_buffer_size, kMinWebAudioBufferSize);
}

// When the kWebAudioRemoveAudioDestinationResampler
// feature is enabled and the context sample rate is higher than the hardware
// sample rate, the GetOutputBufferSize method correctly scales the buffer size.
TEST_F(RendererWebAudioDeviceImplBufferSizeTest,
       InteractiveLatency_HighSampleRate_ScalesBufferSize) {
  feature_list_.InitAndEnableFeature(
      blink::features::kWebAudioRemoveAudioDestinationResampler);
  blink::WebAudioLatencyHint latency_hint(
      blink::WebAudioLatencyHint::kCategoryInteractive);
  media::AudioParameters hardware_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kHardwareSampleRate48k,
      kHardwareBufferSize48k);

  int context_sample_rate = 768000;
  int output_buffer_size = RendererWebAudioDeviceImpl::GetOutputBufferSize(
      latency_hint, context_sample_rate, hardware_params);
  EXPECT_EQ(output_buffer_size, 7680);
}

// When the kWebAudioRemoveAudioDestinationResampler feature is enabled and the
// context sample rate is slightly higher than the hardware sample rate, the
// GetOutputBufferSize method correctly scales the buffer size, demonstrating
// accurate rounding behavior.
TEST_F(RendererWebAudioDeviceImplBufferSizeTest,
       InteractiveLatency_CloseSampleRate_ScalesBufferSize) {
  feature_list_.InitAndEnableFeature(
      blink::features::kWebAudioRemoveAudioDestinationResampler);
  blink::WebAudioLatencyHint latency_hint(
      blink::WebAudioLatencyHint::kCategoryInteractive);
  media::AudioParameters hardware_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kHardwareSampleRate48k,
      kHardwareBufferSize48k);

  int context_sample_rate = 48001;
  int output_buffer_size = RendererWebAudioDeviceImpl::GetOutputBufferSize(
      latency_hint, context_sample_rate, hardware_params);
  EXPECT_EQ(output_buffer_size, 481);
}

// When the kWebAudioRemoveAudioDestinationResampler feature is enabled and the
// context sample rate is slightly higher than the hardware sample rate, with
// different hardware parameters, the GetOutputBufferSize method correctly
// scales the buffer size, demonstrating general rounding and scaling behavior
// with various hardware configurations.
TEST_F(RendererWebAudioDeviceImplBufferSizeTest,
       InteractiveLatency_CloseSampleRate2_ScalesBufferSize) {
  feature_list_.InitAndEnableFeature(
      blink::features::kWebAudioRemoveAudioDestinationResampler);
  blink::WebAudioLatencyHint latency_hint(
      blink::WebAudioLatencyHint::kCategoryInteractive);
  media::AudioParameters hardware_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kHardwareSampleRate44k,
      kHardwareBufferSize44k);

  int context_sample_rate = 48000;
  int output_buffer_size = RendererWebAudioDeviceImpl::GetOutputBufferSize(
      latency_hint, context_sample_rate, hardware_params);
  EXPECT_EQ(output_buffer_size, 481);
}

// When the kWebAudioRemoveAudioDestinationResampler
// feature is enabled and the context sample rate is extremely high (potentially
// unsupported), the GetOutputBufferSize method caps the scaled buffer size at
// the maximum allowed buffer size (kMaxWebAudioBufferSize).
TEST_F(RendererWebAudioDeviceImplBufferSizeTest,
       InteractiveLatency_VeryHighSampleRate_CapsAtMaxBufferSize) {
  feature_list_.InitAndEnableFeature(
      blink::features::kWebAudioRemoveAudioDestinationResampler);
  blink::WebAudioLatencyHint latency_hint(
      blink::WebAudioLatencyHint::kCategoryInteractive);
  media::AudioParameters hardware_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kHardwareSampleRate48k,
      kHardwareBufferSize48k);

  int context_sample_rate = 999000;
  int output_buffer_size = RendererWebAudioDeviceImpl::GetOutputBufferSize(
      latency_hint, context_sample_rate, hardware_params);
  EXPECT_EQ(output_buffer_size, kMaxWebAudioBufferSize);
}

TEST_F(RendererWebAudioDeviceImplTest, ChannelLayout) {
  for (int ch = 1; ch < static_cast<int>(media::limits::kMaxChannels); ++ch) {
    SCOPED_TRACE(base::StringPrintf("ch == %d", ch));

    media::ChannelLayout layout = media::GuessChannelLayout(ch);
    if (layout == media::CHANNEL_LAYOUT_UNSUPPORTED) {
      layout = media::CHANNEL_LAYOUT_DISCRETE;
    }

    SetupDevice({layout, ch});
    media::AudioParameters sink_params =
        webaudio_device_->get_sink_params_for_testing();
    EXPECT_TRUE(sink_params.IsValid());
    EXPECT_EQ(layout, sink_params.channel_layout());
    EXPECT_EQ(ch, sink_params.channels());
  }
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

TEST_F(RendererWebAudioDeviceImplTest, ValidDeviceParameters) {
  // Setup a scenario where the device parameters callback returns valid
  // parameters.
  SetupDevice(media::ChannelLayoutConfig::Stereo());

  EXPECT_EQ(webaudio_device_->GetOriginalSinkParamsForTesting().sample_rate(),
            kHardwareSampleRate);
  EXPECT_EQ(
      webaudio_device_->GetOriginalSinkParamsForTesting().frames_per_buffer(),
      kHardwareBufferSize);
  EXPECT_TRUE(webaudio_device_->GetOriginalSinkParamsForTesting().IsValid());
}

TEST_F(RendererWebAudioDeviceImplTest,
       HandleInvalidOriginalSinkParamsInConstructor) {
  media::AudioParameters default_params;
  EXPECT_FALSE(default_params.IsValid());
  // Test for handling invalid original sink parameters in constructor.
  auto mock_device_params_cb =
      base::BindRepeating([](const blink::LocalFrameToken&,
                             const std::string&) -> media::AudioParameters {
        return media::AudioParameters();
      });
  blink::WebAudioSinkDescriptor sink_descriptor(
      blink::WebString::FromUTF8(std::string()), kFrameToken);

  RendererWebAudioDeviceImplConstructorParamTest device_under_test(
      sink_descriptor, media::ChannelLayoutConfig::Stereo(),
      blink::WebAudioLatencyHint(
          blink::WebAudioLatencyHint::kCategoryInteractive),
      std::nullopt, this,
      base::BindRepeating(&RendererWebAudioDeviceImplTest::CreateMockSilentSink,
                          base::Unretained(this)),
      mock_device_params_cb);

  const media::AudioParameters& params =
      device_under_test.GetOriginalSinkParamsForTesting();
  EXPECT_EQ(params.format(), media::AudioParameters::AUDIO_FAKE);
  EXPECT_EQ(params.sample_rate(), 48000);
  EXPECT_EQ(params.frames_per_buffer(), 480);
  EXPECT_TRUE(params.IsValid());
}

class RendererWebAudioDeviceImplLatencyAndSampleRateTest
    : public RendererWebAudioDeviceImplTest,
      public testing::WithParamInterface<
          std::tuple<blink::WebAudioLatencyHint::AudioContextLatencyCategory,
                     int>> {
 protected:
  void SetUp() override {
    if (std::get<0>(GetParam()) == blink::WebAudioLatencyHint::kCategoryExact) {
      // Simulate a 10ms exact latency.
      test_latency_hint_ = blink::WebAudioLatencyHint(/*seconds=*/0.01);
    } else {
      test_latency_hint_ = blink::WebAudioLatencyHint(std::get<0>(GetParam()));
    }

    int sample_rate = std::get<1>(GetParam());
    // sample_rate == 0 means no context_sample_rate is specified.
    if (sample_rate != 0) {
      context_sample_rate_ = static_cast<float>(sample_rate);
    }

    feature_list_.InitAndEnableFeature(
        blink::features::kWebAudioRemoveAudioDestinationResampler);
  }

 protected:
  blink::WebAudioLatencyHint test_latency_hint_{
      blink::WebAudioLatencyHint::kCategoryInteractive};

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(RendererWebAudioDeviceImplLatencyAndSampleRateTest,
       TestLatencyHintValues) {
  int context_sample_rate = context_sample_rate_.value_or(kHardwareSampleRate);
  media::AudioParameters hardware_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kHardwareSampleRate,
      kHardwareBufferSize);
  int expected_buffer_size = RendererWebAudioDeviceImpl::GetOutputBufferSize(
      test_latency_hint_, context_sample_rate, hardware_params);

  SetupDevice(test_latency_hint_);

  EXPECT_EQ(webaudio_device_->SampleRate(), context_sample_rate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), expected_buffer_size);

  webaudio_device_->Start();
  EXPECT_EQ(webaudio_device_->SampleRate(), context_sample_rate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), expected_buffer_size);

  webaudio_device_->Stop();
  EXPECT_EQ(webaudio_device_->SampleRate(), context_sample_rate);
  EXPECT_EQ(webaudio_device_->FramesPerBuffer(), expected_buffer_size);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RendererWebAudioDeviceImplLatencyAndSampleRateTest,
    testing::Combine(

        testing::Values(blink::WebAudioLatencyHint::kCategoryInteractive,
                        blink::WebAudioLatencyHint::kCategoryBalanced,
                        blink::WebAudioLatencyHint::kCategoryPlayback,
                        blink::WebAudioLatencyHint::kCategoryExact),
        // User provided sample rate; 0 means no sample rate provided.
        testing::Values(0, 16000, 44100, 48000, 96000)));

}  // namespace content
