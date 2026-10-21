// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_features.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/limits.h"
#include "media/base/output_device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  MockAudioRendererSink() = default;
  void Initialize(const media::AudioParameters& params,
                  media::AudioRendererSink::RenderCallback* callback) override {
    callback_ = callback;
    initialize_params_.push_back(params);
  }
  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, Pause, (PauseReason reason), (override));
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
  std::vector<media::AudioParameters> initialize_params_;

 private:
  ~MockAudioRendererSink() override = default;
};

constexpr int kHardwareSampleRate = 44100;
constexpr int kHardwareBufferSize = 128;
const blink::LocalFrameToken kFrameToken;

media::OutputDeviceInfo GetHealthyDevice() {
  return media::OutputDeviceInfo(
      std::string(), media::OUTPUT_DEVICE_STATUS_OK,
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Stereo(),
                             kHardwareSampleRate, kHardwareBufferSize));
}

media::OutputDeviceInfo GetErrorDevice() {
  return media::OutputDeviceInfo(
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
}

constexpr int kDeviceSampleRate = 48000;
constexpr int kDeviceBufferSize = 512;
constexpr int kAlternateDeviceBufferSize = 480;

enum class FirstDeviceQuery { kHealthy, kTimedOut, kNotAuthorized, kInvalid };
enum class SecondDeviceQuery { kOk, kOkAlternateBuffer, kTimedOut };

media::AudioParameters GetDeviceParams(int frames_per_buffer) {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kDeviceSampleRate, frames_per_buffer);
}

media::OutputDeviceInfo GetDeviceInfo(FirstDeviceQuery query) {
  switch (query) {
    case FirstDeviceQuery::kHealthy:
      return media::OutputDeviceInfo(std::string(),
                                     media::OUTPUT_DEVICE_STATUS_OK,
                                     GetDeviceParams(kDeviceBufferSize));
    case FirstDeviceQuery::kTimedOut:
      return media::OutputDeviceInfo(
          std::string(), media::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT,
          media::AudioParameters::UnavailableDeviceParams());
    case FirstDeviceQuery::kNotAuthorized:
      // AudioOutputDevice reports UnavailableDeviceParams() for any non-OK
      // status.
      return media::OutputDeviceInfo(
          std::string(), media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
          media::AudioParameters::UnavailableDeviceParams());
    case FirstDeviceQuery::kInvalid:
      return media::OutputDeviceInfo(std::string(),
                                     media::OUTPUT_DEVICE_STATUS_OK,
                                     media::AudioParameters());
  }
  NOTREACHED();
}

media::OutputDeviceInfo GetDeviceInfo(SecondDeviceQuery query) {
  switch (query) {
    case SecondDeviceQuery::kOk:
      return media::OutputDeviceInfo(std::string(),
                                     media::OUTPUT_DEVICE_STATUS_OK,
                                     GetDeviceParams(kDeviceBufferSize));
    case SecondDeviceQuery::kOkAlternateBuffer:
      return media::OutputDeviceInfo(
          std::string(), media::OUTPUT_DEVICE_STATUS_OK,
          GetDeviceParams(kAlternateDeviceBufferSize));
    case SecondDeviceQuery::kTimedOut:
      return media::OutputDeviceInfo(
          std::string(), media::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT,
          media::AudioParameters::UnavailableDeviceParams());
  }
  NOTREACHED();
}

struct InitTestParam {
  using TupleT =
      std::tuple<FirstDeviceQuery,
                 SecondDeviceQuery,
                 std::optional<float>,
                 blink::WebAudioLatencyHint::AudioContextLatencyCategory,
                 bool>;

  explicit InitTestParam(const TupleT& t)
      : first_query(std::get<0>(t)),
        second_query(std::get<1>(t)),
        context_sample_rate(std::get<2>(t)),
        latency_category(std::get<3>(t)),
        remove_resampler(std::get<4>(t)) {}

  FirstDeviceQuery first_query;
  SecondDeviceQuery second_query;
  std::optional<float> context_sample_rate;
  blink::WebAudioLatencyHint::AudioContextLatencyCategory latency_category;
  bool remove_resampler;
};

std::string GetInitTestParamName(
    const testing::TestParamInfo<InitTestParam>& info) {
  const InitTestParam& param = info.param;
  std::string first;
  switch (param.first_query) {
    case FirstDeviceQuery::kHealthy:
      first = "Healthy";
      break;
    case FirstDeviceQuery::kTimedOut:
      first = "TimedOut";
      break;
    case FirstDeviceQuery::kNotAuthorized:
      first = "NotAuthorized";
      break;
    case FirstDeviceQuery::kInvalid:
      first = "InvalidParams";
      break;
  }
  std::string second;
  switch (param.second_query) {
    case SecondDeviceQuery::kOk:
      second = base::StringPrintf("ThenOk%d", kDeviceBufferSize);
      break;
    case SecondDeviceQuery::kOkAlternateBuffer:
      second = base::StringPrintf("ThenOk%d", kAlternateDeviceBufferSize);
      break;
    case SecondDeviceQuery::kTimedOut:
      second = "ThenTimedOut";
      break;
  }
  const std::string context_rate =
      param.context_sample_rate
          ? base::StringPrintf("ContextRate%d",
                               static_cast<int>(*param.context_sample_rate))
          : "NoContextRate";
  const char* latency =
      param.latency_category == blink::WebAudioLatencyHint::kCategoryInteractive
          ? "Interactive"
          : "Balanced";
  const char* resampler =
      param.remove_resampler ? "RemoveResamplerOn" : "RemoveResamplerOff";
  return base::StrCat(
      {first, "_", second, "_", context_rate, "_", latency, "_", resampler});
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
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            std::move(silent_sink_callback),
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()) {}
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

  scoped_refptr<media::AudioRendererSink> CreateMockSilentSink(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
    return mock_audio_renderer_sink_;
  }

 protected:
  RendererWebAudioDeviceImplTest() {
    mock_audio_renderer_sink_ = base::MakeRefCounted<MockAudioRendererSink>();
    ON_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo())
        .WillByDefault(testing::Return(GetHealthyDevice()));
  }

  void SetupDevice(blink::WebAudioLatencyHint latency_hint) {
    blink::WebAudioSinkDescriptor sink_descriptor(
        blink::WebString::FromUtf8(std::string()), kFrameToken);
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, media::ChannelLayoutConfig::Mono(), latency_hint,
        context_sample_rate_, this,
        base::BindRepeating(
            &RendererWebAudioDeviceImplTest::CreateMockSilentSink,
            // Guaranteed to be valid because `this` owns `webaudio_device_` and
            // so will outlive it.
            base::Unretained(this)));
  }

  void SetupDevice(media::ChannelLayoutConfig layout_config) {
    blink::WebAudioSinkDescriptor sink_descriptor(
        blink::WebString::FromUtf8(std::string()), kFrameToken);
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, layout_config,
        blink::WebAudioLatencyHint(
            blink::WebAudioLatencyHint::kCategoryInteractive),
        context_sample_rate_, this,
        base::BindRepeating(
            &RendererWebAudioDeviceImplTest::CreateMockSilentSink,
            // Guaranteed to be valid because `this` owns `webaudio_device_` and
            // so will outlive it.
            base::Unretained(this)));
  }

  void SetupDevice(blink::WebAudioSinkDescriptor sink_descriptor) {
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, media::ChannelLayoutConfig::Mono(),
        blink::WebAudioLatencyHint(
            blink::WebAudioLatencyHint::kCategoryInteractive),
        context_sample_rate_, this,
        base::BindRepeating(
            &RendererWebAudioDeviceImplTest::CreateMockSilentSink,
            // Guaranteed to be valid because `this` owns `webaudio_device_` and
            // so will outlive it.
            base::Unretained(this)));
  }

  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType render_token,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) override {
    return mock_audio_renderer_sink_;
  }

  void TearDown() override { webaudio_device_.reset(); }

  struct InitStep {
    const char* name;
    int frames_per_buffer;
    double sample_rate;
  };

  void RunInitSequence(
      const blink::WebAudioSinkDescriptor& sink_descriptor,
      blink::WebAudioLatencyHint::AudioContextLatencyCategory latency) {
    webaudio_device_ = std::make_unique<RendererWebAudioDeviceImplUnderTest>(
        sink_descriptor, media::ChannelLayoutConfig::Mono(),
        blink::WebAudioLatencyHint(latency), context_sample_rate_, this,
        base::BindRepeating(
            &RendererWebAudioDeviceImplTest::CreateMockSilentSink,
            // Guaranteed to be valid because `this` owns `webaudio_device_` and
            // so will outlive it.
            base::Unretained(this)));
    construction_frames_per_buffer_ = webaudio_device_->FramesPerBuffer();
    construction_sample_rate_ = webaudio_device_->SampleRate();

    auto record_step = [this](const char* name) {
      init_steps_.push_back({name, webaudio_device_->FramesPerBuffer(),
                             webaudio_device_->SampleRate()});
    };
    webaudio_device_->MaybeCreateSinkAndGetStatus();
    record_step("MaybeCreateSinkAndGetStatus");
    webaudio_device_->Start();
    record_step("Start");
    webaudio_device_->Stop();
    record_step("Stop");
    webaudio_device_->Start();
    record_step("Restart");
    webaudio_device_->Stop();
    record_step("FinalStop");
  }

  std::unique_ptr<RendererWebAudioDeviceImpl> webaudio_device_;
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<MockAudioRendererSink> mock_audio_renderer_sink_;
  std::optional<float> context_sample_rate_;
  int construction_frames_per_buffer_ = 0;
  double construction_sample_rate_ = 0;
  std::vector<InitStep> init_steps_;
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
      features::kWebAudioRemoveAudioDestinationResampler);
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
      features::kWebAudioRemoveAudioDestinationResampler);
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
      features::kWebAudioRemoveAudioDestinationResampler);
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
      features::kWebAudioRemoveAudioDestinationResampler);
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
      features::kWebAudioRemoveAudioDestinationResampler);
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
      features::kWebAudioRemoveAudioDestinationResampler);
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
      features::kWebAudioRemoveAudioDestinationResampler);
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

// When the kWebAudioRemoveAudioDestinationResampler feature is enabled and the
// hardware reports a native minimum buffer size that exceeds Web Audio's
// maximum limit (kMaxWebAudioBufferSize), the GetOutputBufferSize method safely
// caps the computed buffer size at kMaxWebAudioBufferSize.
TEST_F(RendererWebAudioDeviceImplBufferSizeTest,
       ExactLatency_HighHardwareBufferSize_CapsAtMaxBufferSize) {
  feature_list_.InitAndEnableFeature(
      features::kWebAudioRemoveAudioDestinationResampler);
  // Request a 50ms exact latency hint.
  blink::WebAudioLatencyHint latency_hint(0.05);

  // Simulate a hardware device with a minimum buffer size exceeding
  // kMaxWebAudioBufferSize (8192).
  media::AudioParameters hardware_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kHardwareSampleRate48k,
      kHardwareBufferSize48k);
  media::AudioParameters::HardwareCapabilities capabilities;
  capabilities.min_frames_per_buffer = 16384;
  capabilities.max_frames_per_buffer = 16384;
  hardware_params.set_hardware_capabilities(capabilities);

  int context_sample_rate = kHardwareSampleRate48k;
  int output_buffer_size = RendererWebAudioDeviceImpl::GetOutputBufferSize(
      latency_hint, context_sample_rate, hardware_params);

  // The output buffer size should be safely capped at
  // kMaxWebAudioBufferSize (8192) instead of crashing or returning 0.
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
        webaudio_device_->GetSinkParamsForTesting();
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

    EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(1);
    EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(1);
  }

  SetupDevice(media::ChannelLayoutConfig::Stereo());

  // `sink_` should be created after OUTPUT_DEVICE_STATUS_OK status return from
  // `CreateAndGetSinkStatus` call.
  EXPECT_NE(webaudio_device_->GetSinkForTesting(), nullptr);
  media::OutputDeviceStatus status =
      webaudio_device_->MaybeCreateSinkAndGetStatus();
  EXPECT_NE(webaudio_device_->GetSinkForTesting(), nullptr);

  // Healthy device should return OUTPUT_DEVICE_STATUS_OK.
  EXPECT_EQ(status, media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
  webaudio_device_->Start();
  webaudio_device_->Stop();
}

TEST_F(RendererWebAudioDeviceImplTest,
       CreateSinkAndGetDeviceStatus_ErrorDevice) {
  EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo)
      .WillRepeatedly(testing::Return(GetErrorDevice()));
  EXPECT_CALL(*mock_audio_renderer_sink_, Start).Times(0);
  EXPECT_CALL(*mock_audio_renderer_sink_, Play).Times(0);
  // Stop() is necessary before destruction per AudioRendererSink contract.
  EXPECT_CALL(*mock_audio_renderer_sink_, Stop).Times(testing::AtLeast(1));

  SetupDevice(media::ChannelLayoutConfig::Stereo());

  // `sink_` should remain as nullptr after
  // OUTPUT_DEVICE_STATUS_ERROR_INTERNAL status return from constructor /
  // `MaybeCreateSinkAndGetStatus` call.
  EXPECT_EQ(webaudio_device_->GetSinkForTesting(), nullptr);
  media::OutputDeviceStatus status =
      webaudio_device_->MaybeCreateSinkAndGetStatus();
  EXPECT_EQ(webaudio_device_->GetSinkForTesting(), nullptr);

  // Error device should return OUTPUT_DEVICE_STATUS_ERROR_INTERNAL.
  EXPECT_EQ(status,
            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
}

TEST_F(RendererWebAudioDeviceImplTest,
       MaybeCreateSinkAndGetStatus_ErrorDevice) {
  SetupDevice(media::ChannelLayoutConfig::Stereo());
  EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo())
      .WillOnce(testing::Return(GetErrorDevice()));
  media::OutputDeviceStatus status =
      webaudio_device_->MaybeCreateSinkAndGetStatus();
  EXPECT_EQ(status,
            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
  EXPECT_EQ(webaudio_device_->GetSinkForTesting(), nullptr);
}

TEST_F(RendererWebAudioDeviceImplTest,
       SilentSink_InitSequenceNeverQueriesOutputDevice) {
  EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo).Times(0);

  RunInitSequence(blink::WebAudioSinkDescriptor(kFrameToken),
                  blink::WebAudioLatencyHint::kCategoryInteractive);
}

TEST_F(RendererWebAudioDeviceImplTest,
       SilentSink_ParamsFixedAfterConstruction) {
  RunInitSequence(blink::WebAudioSinkDescriptor(kFrameToken),
                  blink::WebAudioLatencyHint::kCategoryInteractive);

  for (const InitStep& step : init_steps_) {
    SCOPED_TRACE(step.name);
    EXPECT_EQ(step.frames_per_buffer, construction_frames_per_buffer_);
    EXPECT_EQ(step.sample_rate, construction_sample_rate_);
  }
}

TEST_F(RendererWebAudioDeviceImplTest,
       SilentSink_SinkInitializedWithConstructionParams) {
  RunInitSequence(blink::WebAudioSinkDescriptor(kFrameToken),
                  blink::WebAudioLatencyHint::kCategoryInteractive);

  ASSERT_FALSE(mock_audio_renderer_sink_->initialize_params_.empty());
  for (const media::AudioParameters& params :
       mock_audio_renderer_sink_->initialize_params_) {
    EXPECT_EQ(params.frames_per_buffer(), construction_frames_per_buffer_);
    EXPECT_EQ(params.sample_rate(), construction_sample_rate_);
  }
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
  EXPECT_NE(webaudio_device_->GetSinkForTesting(), nullptr);
  media::OutputDeviceStatus status =
      webaudio_device_->MaybeCreateSinkAndGetStatus();
  EXPECT_NE(webaudio_device_->GetSinkForTesting(), nullptr);

  // Silent sink should return OUTPUT_DEVICE_STATUS_OK.
  EXPECT_EQ(status, media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
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
  media::AudioParameters invalid_params;
  EXPECT_FALSE(invalid_params.IsValid());

  // Test for handling invalid original sink parameters in constructor.
  EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo())
      .WillOnce(testing::Return(media::OutputDeviceInfo(
          std::string(), media::OUTPUT_DEVICE_STATUS_OK, invalid_params)));

  SetupDevice(media::ChannelLayoutConfig::Stereo());

  const media::AudioParameters& params =
      webaudio_device_->GetOriginalSinkParamsForTesting();
  EXPECT_EQ(params.format(), media::AudioParameters::AUDIO_FAKE);
  EXPECT_EQ(params.sample_rate(), 48000);
  EXPECT_EQ(params.frames_per_buffer(), 480);
  EXPECT_TRUE(params.IsValid());
}

TEST_F(RendererWebAudioDeviceImplTest,
       AuthorizedDeviceRecordsHardwareBufferSize) {
  base::HistogramTester histogram_tester;
  SetupDevice(media::ChannelLayoutConfig::Stereo());

  histogram_tester.ExpectUniqueSample(
      "WebAudio.AudioDestination.HardwareBufferSize2", kHardwareBufferSize, 1);
}

TEST_F(RendererWebAudioDeviceImplTest,
       SinkRecreationAfterStopRecordsNoAdditionalHardwareBufferSize) {
  base::HistogramTester histogram_tester;
  SetupDevice(media::ChannelLayoutConfig::Stereo());

  webaudio_device_->Stop();
  EXPECT_EQ(webaudio_device_->MaybeCreateSinkAndGetStatus(),
            media::OUTPUT_DEVICE_STATUS_OK);

  histogram_tester.ExpectUniqueSample(
      "WebAudio.AudioDestination.HardwareBufferSize2", kHardwareBufferSize, 1);
}

TEST_F(RendererWebAudioDeviceImplTest,
       TimedOutAuthorizationRecordsNoHardwareBufferSize) {
  base::HistogramTester histogram_tester;
  // A timed-out authorization reports valid placeholder parameters.
  EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo)
      .WillRepeatedly(testing::Return(media::OutputDeviceInfo(
          std::string(), media::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT,
          media::AudioParameters::UnavailableDeviceParams())));

  SetupDevice(media::ChannelLayoutConfig::Stereo());

  histogram_tester.ExpectTotalCount(
      "WebAudio.AudioDestination.HardwareBufferSize2", 0);
}

TEST_F(RendererWebAudioDeviceImplTest,
       InvalidHardwareParamsRecordNoHardwareBufferSize) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo)
      .WillRepeatedly(testing::Return(
          media::OutputDeviceInfo(std::string(), media::OUTPUT_DEVICE_STATUS_OK,
                                  media::AudioParameters())));

  SetupDevice(media::ChannelLayoutConfig::Stereo());

  histogram_tester.ExpectTotalCount(
      "WebAudio.AudioDestination.HardwareBufferSize2", 0);
}

TEST_F(RendererWebAudioDeviceImplTest, SilentSinkRecordsNoHardwareBufferSize) {
  base::HistogramTester histogram_tester;
  // The `WebAudioSinkDescriptor` constructor with only a frame token constructs
  // a silent sink.
  SetupDevice(blink::WebAudioSinkDescriptor(kFrameToken));

  histogram_tester.ExpectTotalCount(
      "WebAudio.AudioDestination.HardwareBufferSize2", 0);
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
        features::kWebAudioRemoveAudioDestinationResampler);
  }

 protected:
  blink::WebAudioLatencyHint test_latency_hint_{
      blink::WebAudioLatencyHint::kCategoryInteractive};
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

class RendererWebAudioDeviceImplInitTest
    : public RendererWebAudioDeviceImplTest,
      public testing::WithParamInterface<InitTestParam> {
 protected:
  void SetUp() override {
    const InitTestParam& param = GetParam();
    feature_list_.InitWithFeatureState(
        features::kWebAudioRemoveAudioDestinationResampler,
        param.remove_resampler);
    context_sample_rate_ = param.context_sample_rate;
    EXPECT_CALL(*mock_audio_renderer_sink_, GetOutputDeviceInfo())
        .WillOnce(testing::Return(GetDeviceInfo(param.first_query)))
        .WillRepeatedly(testing::Return(GetDeviceInfo(param.second_query)));
  }

  void RunAudibleInitSequence() {
    RunInitSequence(blink::WebAudioSinkDescriptor(
                        blink::WebString::FromUtf8(std::string()), kFrameToken),
                    GetParam().latency_category);
  }
};

TEST_P(RendererWebAudioDeviceImplInitTest, ParamsFixedAfterConstruction) {
  RunAudibleInitSequence();

  for (const InitStep& step : init_steps_) {
    SCOPED_TRACE(step.name);
    EXPECT_EQ(step.frames_per_buffer, construction_frames_per_buffer_);
    EXPECT_EQ(step.sample_rate, construction_sample_rate_);
  }
}

TEST_P(RendererWebAudioDeviceImplInitTest,
       SinkInitializedWithConstructionParams) {
  RunAudibleInitSequence();

  ASSERT_FALSE(mock_audio_renderer_sink_->initialize_params_.empty());
  for (const media::AudioParameters& params :
       mock_audio_renderer_sink_->initialize_params_) {
    EXPECT_EQ(params.frames_per_buffer(), construction_frames_per_buffer_);
    EXPECT_EQ(params.sample_rate(), construction_sample_rate_);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RendererWebAudioDeviceImplInitTest,
    testing::ConvertGenerator<InitTestParam::TupleT>(testing::Combine(
        testing::Values(FirstDeviceQuery::kHealthy,
                        FirstDeviceQuery::kTimedOut,
                        FirstDeviceQuery::kNotAuthorized,
                        FirstDeviceQuery::kInvalid),
        testing::Values(SecondDeviceQuery::kOkAlternateBuffer,
                        SecondDeviceQuery::kOk,
                        SecondDeviceQuery::kTimedOut),
        testing::Values(std::optional<float>(),
                        std::optional<float>(kDeviceSampleRate)),
        testing::Values(blink::WebAudioLatencyHint::kCategoryInteractive,
                        blink::WebAudioLatencyHint::kCategoryBalanced),
        testing::Bool())),
    GetInitTestParamName);

}  // namespace content
