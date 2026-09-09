// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_node.h"

#include <algorithm>
#include <memory>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
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

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<AudioContextTestPlatform> platform_;
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

  // Set initial format as if a 2-channel 44.1kHz audio source was loaded.
  node_->SetFormat(kInitialNumberOfChannels, kInitialSampleRate);
  EXPECT_DOUBLE_EQ(handler.SourceSampleRateForTesting(), kInitialSampleRate);
  EXPECT_EQ(handler.SourceNumberOfChannelsForTesting(),
            kInitialNumberOfChannels);
  EXPECT_FALSE(handler.IsOriginTaintedForTesting());

  // Triggering source change should clear cached format and default to tainted
  // until SetFormat() validates the new source.
  node_->OnCurrentSrcChanged(KURL("https://example.com/new-source.wav"));
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
  node_->SetFormat(kNewNumberOfChannels, kNewSampleRate);
  EXPECT_DOUBLE_EQ(handler.SourceSampleRateForTesting(), kNewSampleRate);
  EXPECT_EQ(handler.SourceNumberOfChannelsForTesting(), kNewNumberOfChannels);
  EXPECT_FALSE(handler.IsOriginTaintedForTesting());
}

TEST_F(MediaElementAudioSourceNodeTest, WouldTaintOriginWithoutPlayer) {
  MediaElementAudioSourceHandler& handler =
      node_->GetMediaElementAudioSourceHandler();
  // When there is no WebMediaPlayer (e.g. before media load initializes a
  // player), the element does not taint origin by default. This aligns with
  // HTMLMediaElement::IsMediaDataCorsSameOrigin().
  EXPECT_FALSE(handler.WouldTaintOriginForTesting());
}

}  // namespace blink
