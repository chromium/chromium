// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_node.h"

#include <algorithm>
#include <memory>

#include "base/containers/span.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/public/platform/web_audio_source_provider_impl.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_context_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_handler.h"
#include "third_party/blink/renderer/modules/webaudio/testing/fake_audio_thread.h"
#include "third_party/blink/renderer/modules/webaudio/testing/mock_web_audio_device.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/media/web_audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

class AudioContextTestPlatform : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      const WebAudioSinkDescriptor& sink_descriptor,
      unsigned number_of_output_channels,
      const WebAudioLatencyHint& latency_hint,
      std::optional<float> context_sample_rate,
      media::AudioRendererSink::RenderCallback*) override {
    return std::make_unique<MockWebAudioDevice>(AudioHardwareSampleRate(),
                                                AudioHardwareBufferSize());
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 128; }
};

class ConstantAudioRenderCallback
    : public media::AudioRendererSink::RenderCallback {
 public:
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* dest) override {
    for (auto channel : dest->AllChannels()) {
      std::ranges::fill(channel, 0.5f);
    }
    return dest->frames();
  }
  void OnRenderError() override {}
};

class StubWebAudioSourceProviderClient : public WebAudioSourceProviderClient {
 public:
  void SetFormat(uint32_t number_of_channels, float sample_rate) override {}
};

constexpr unsigned kInitialNumberOfChannels = 2;
constexpr float kInitialSampleRate = 44100.0f;
constexpr unsigned kNewNumberOfChannels = 1;
constexpr float kNewSampleRate = 48000.0f;

}  // namespace

class MediaElementAudioSourceNodeTest : public testing::Test {
 protected:
  void SetUp() override {
    page_ = std::make_unique<DummyPageHolder>();
    AudioContextOptions* options = AudioContextOptions::Create();
    context_ = AudioContext::Create(page_->GetFrame().DomWindow(), options,
                                    ASSERT_NO_EXCEPTION);
    media_element_ =
        MakeGarbageCollected<HTMLAudioElement>(page_->GetDocument());
    node_ = MediaElementAudioSourceNode::Create(*context_, *media_element_,
                                                ASSERT_NO_EXCEPTION);
  }

  scoped_refptr<WebAudioSourceProviderImpl> CreatePlayingProvider(
      unsigned channels,
      float sample_rate) {
    auto provider =
        base::MakeRefCounted<WebAudioSourceProviderImpl>(nullptr, nullptr);
    provider->SetClient(&provider_client_);
    provider->Initialize(
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LINEAR,
                               media::ChannelLayoutConfig::Guess(channels),
                               static_cast<int>(sample_rate), 128),
        &render_callback_);
    provider->Start();
    provider->Play();
    return provider;
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<AudioContextTestPlatform> platform_;
  ConstantAudioRenderCallback render_callback_;
  StubWebAudioSourceProviderClient provider_client_;
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<AudioContext> context_;
  Persistent<HTMLAudioElement> media_element_;
  Persistent<MediaElementAudioSourceNode> node_;
};

TEST_F(MediaElementAudioSourceNodeTest, OnCurrentSrcChangedClearsCachedFormat) {
  MediaElementAudioSourceHandler& handler =
      node_->GetMediaElementAudioSourceHandler();

  EXPECT_DOUBLE_EQ(handler.SourceSampleRateForTesting(), 0.0);
  EXPECT_EQ(handler.SourceNumberOfChannelsForTesting(), 0u);
  EXPECT_FALSE(handler.IsOriginTaintedForTesting());

  // Passing (0, 0) on a freshly initialized handler must still be treated as
  // an invalid format and clear any attached provider.
  handler.SetAudioSourceProviderForTesting(
      base::MakeRefCounted<WebAudioSourceProviderImpl>(nullptr, nullptr));
  ASSERT_TRUE(handler.HasAudioSourceProviderForTesting());
  node_->SetFormat(0, 0.0f);
  EXPECT_FALSE(handler.HasAudioSourceProviderForTesting());
  EXPECT_DOUBLE_EQ(handler.SourceSampleRateForTesting(), 0.0);
  EXPECT_EQ(handler.SourceNumberOfChannelsForTesting(), 0u);

  // Set initial format as if a 2-channel 44.1kHz audio source was loaded.
  handler.SetAudioSourceProviderForTesting(
      base::MakeRefCounted<WebAudioSourceProviderImpl>(nullptr, nullptr));
  node_->SetFormat(kInitialNumberOfChannels, kInitialSampleRate);
  EXPECT_TRUE(handler.HasAudioSourceProviderForTesting());
  EXPECT_DOUBLE_EQ(handler.SourceSampleRateForTesting(), kInitialSampleRate);
  EXPECT_EQ(handler.SourceNumberOfChannelsForTesting(),
            kInitialNumberOfChannels);
  EXPECT_FALSE(handler.IsOriginTaintedForTesting());

  // Triggering source change should clear cached format and default to tainted
  // until SetFormat() validates the new source.
  node_->OnCurrentSrcChanged(KURL("https://example.com/new-source.wav"));
  EXPECT_FALSE(handler.HasAudioSourceProviderForTesting());
  EXPECT_DOUBLE_EQ(handler.SourceSampleRateForTesting(), 0.0);
  EXPECT_EQ(handler.SourceNumberOfChannelsForTesting(), 0u);
  EXPECT_TRUE(handler.IsOriginTaintedForTesting());

  // Verify that Process() outputs silence on the audio thread when format is
  // cleared.
  FakeAudioThread audio_thread(ThreadType::kRealtimeAudioWorkletThread);
  audio_thread.RunOnAudioThreadWithContext(
      context_.Get(),
      CrossThreadBindOnce(
          [](MediaElementAudioSourceHandler* handler) {
            AudioBus* output_bus = handler->Output(0).Bus();
            ASSERT_TRUE(output_bus);
            output_bus->Zero();
            for (unsigned ch = 0; ch < output_bus->NumberOfChannels(); ++ch) {
              base::span<float> channel_span =
                  output_bus->Channel(ch)->MutableSpan();
              std::ranges::fill(channel_span, 1.0f);
            }
            handler->Process(128);
            for (unsigned ch = 0; ch < output_bus->NumberOfChannels(); ++ch) {
              base::span<const float> channel_span =
                  output_bus->Channel(ch)->Span();
              for (float sample : channel_span) {
                EXPECT_EQ(sample, 0.0f);
              }
            }
          },
          CrossThreadUnretained(&handler)));

  // Set new format for the new source.
  handler.SetAudioSourceProviderForTesting(
      base::MakeRefCounted<WebAudioSourceProviderImpl>(nullptr, nullptr));
  node_->SetFormat(kNewNumberOfChannels, kNewSampleRate);
  EXPECT_TRUE(handler.HasAudioSourceProviderForTesting());
  EXPECT_DOUBLE_EQ(handler.SourceSampleRateForTesting(), kNewSampleRate);
  EXPECT_EQ(handler.SourceNumberOfChannelsForTesting(), kNewNumberOfChannels);
  EXPECT_FALSE(handler.IsOriginTaintedForTesting());

  // An invalid format must reset both cached format and provider.
  node_->SetFormat(0, kNewSampleRate);
  EXPECT_FALSE(handler.HasAudioSourceProviderForTesting());
  EXPECT_DOUBLE_EQ(handler.SourceSampleRateForTesting(), 0.0);
  EXPECT_EQ(handler.SourceNumberOfChannelsForTesting(), 0u);
}

TEST_F(MediaElementAudioSourceNodeTest, WouldTaintOriginWithoutPlayer) {
  MediaElementAudioSourceHandler& handler =
      node_->GetMediaElementAudioSourceHandler();
  // When there is no WebMediaPlayer (e.g. before media load initializes a
  // player), the element does not taint origin by default. This aligns with
  // HTMLMediaElement::IsMediaDataCorsSameOrigin().
  EXPECT_FALSE(handler.WouldTaintOriginForTesting());
}

TEST_F(MediaElementAudioSourceNodeTest, HasPendingActivity) {
  task_environment_.RunUntilIdle();
  ASSERT_EQ(context_->ContextState(), V8AudioContextState::Enum::kRunning);

  // Initially, the media element is paused and unconnected.
  EXPECT_TRUE(media_element_->paused());
  EXPECT_FALSE(node_->HasPendingActivity());

  // Connect to the destination node. When paused, HasPendingActivity() remains
  // false so that an inactive source does not keep itself alive.
  node_->connect(context_->destinationNode(), 0, 0, ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(node_->HasPendingActivity());

  // Once playing, the connected source has pending activity to stream audio.
  media_element_->Play();
  EXPECT_FALSE(media_element_->paused());
  EXPECT_TRUE(node_->HasPendingActivity());

  // When paused again, pending activity drops to false.
  media_element_->pause();
  EXPECT_TRUE(media_element_->paused());
  EXPECT_FALSE(node_->HasPendingActivity());

  // Playing while disconnected also has no pending activity.
  media_element_->Play();
  EXPECT_FALSE(media_element_->paused());
  node_->disconnect();
  EXPECT_FALSE(node_->HasPendingActivity());
}

TEST_F(MediaElementAudioSourceNodeTest,
       ProcessAndResamplerAfterMediaElementCollected) {
  scoped_refptr<MediaElementAudioSourceHandler> handler =
      &node_->GetMediaElementAudioSourceHandler();

  // Attach an active playing WebAudioSourceProviderImpl and configure a
  // resampled format (48kHz source on a 44.1kHz context) so
  // multi_channel_resampler_ actively pulls non-zero frames before GC.
  scoped_refptr<WebAudioSourceProviderImpl> provider =
      CreatePlayingProvider(kInitialNumberOfChannels, kNewSampleRate);
  handler->SetAudioSourceProviderForTesting(provider);
  node_->SetFormat(kInitialNumberOfChannels, kNewSampleRate);
  ASSERT_TRUE(handler->HasAudioSourceProviderForTesting());

  FakeAudioThread audio_thread(ThreadType::kRealtimeAudioWorkletThread);
  audio_thread.RunOnAudioThreadWithContext(
      context_.Get(),
      CrossThreadBindOnce(
          [](MediaElementAudioSourceHandler* handler) {
            AudioBus* output_bus = handler->Output(0).Bus();
            ASSERT_TRUE(output_bus);
            output_bus->Zero();
            handler->Process(128);
            for (unsigned ch = 0; ch < output_bus->NumberOfChannels(); ++ch) {
              EXPECT_TRUE(std::ranges::any_of(
                  output_bus->Channel(ch)->Span(),
                  [](float sample) { return sample > 0.0f; }));
            }

            output_bus->Zero();
            handler->ProvideResamplerInputForTesting(0, output_bus);
            for (unsigned ch = 0; ch < output_bus->NumberOfChannels(); ++ch) {
              for (float sample : output_bus->Channel(ch)->Span()) {
                EXPECT_FLOAT_EQ(sample, 0.5f);
              }
            }
          },
          CrossThreadUnretained(handler.get())));

  // Drop strong references to the node and media element and run GC while the
  // handler remains alive (e.g. as an in-flight/orphan audio handler).
  node_ = nullptr;
  media_element_ = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(handler->HasAudioSourceProviderForTesting());
  EXPECT_TRUE(handler->WouldTaintOriginForTesting());

  // SetFormat() after Dispose() must be a safe no-op and leave cached format
  // values unchanged.
  handler->SetFormat(kNewNumberOfChannels, kInitialSampleRate);
  EXPECT_EQ(handler->SourceNumberOfChannelsForTesting(),
            kInitialNumberOfChannels);
  EXPECT_DOUBLE_EQ(handler->SourceSampleRateForTesting(), kNewSampleRate);

  // Both Process() and ProvideResamplerInput() must safely output silence when
  // the audio source provider has been cleared during Dispose().
  audio_thread.RunOnAudioThreadWithContext(
      context_.Get(),
      CrossThreadBindOnce(
          [](MediaElementAudioSourceHandler* handler) {
            AudioBus* output_bus = handler->Output(0).Bus();
            ASSERT_TRUE(output_bus);
            auto expect_silence = [&](auto render_fn) {
              for (unsigned ch = 0; ch < output_bus->NumberOfChannels(); ++ch) {
                std::ranges::fill(output_bus->Channel(ch)->MutableSpan(), 1.0f);
              }
              render_fn();
              for (unsigned ch = 0; ch < output_bus->NumberOfChannels(); ++ch) {
                for (float sample : output_bus->Channel(ch)->Span()) {
                  EXPECT_EQ(sample, 0.0f);
                }
              }
            };

            expect_silence([&]() { handler->Process(128); });
            expect_silence([&]() {
              handler->ProvideResamplerInputForTesting(0, output_bus);
            });
          },
          CrossThreadUnretained(handler.get())));

  provider->Stop();
  provider->SetClient(nullptr);
}

}  // namespace blink
