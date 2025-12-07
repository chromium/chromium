// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_handler.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/output_device_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_context_options.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/testing/mock_web_audio_device.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/mediastream/webaudio_destination_consumer.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using ::testing::_;
using ::testing::StrictMock;

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
    return std::make_unique<MockWebAudioDevice>(
        AudioHardwareSampleRate(), AudioHardwareBufferSize());
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 128; }
};

class MockWebAudioDestinationConsumer : public WebAudioDestinationConsumer {
 public:
  MOCK_METHOD2(SetFormat, void(int, float));
  MOCK_METHOD2(ConsumeAudio, void(const Vector<const float*>&, int));
};

}  // namespace

class MediaStreamAudioDestinationHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    auto page = std::make_unique<DummyPageHolder>();
    AudioContext* audio_context = AudioContext::Create(
        page->GetFrame().DomWindow(), AudioContextOptions::Create(),
        ASSERT_NO_EXCEPTION);
    node_ = MediaStreamAudioDestinationNode::Create(
        *audio_context, 2, ASSERT_NO_EXCEPTION);
    bus_ = AudioBus::Create(2, 10);
  }

  ~MediaStreamAudioDestinationHandlerTest() override = default;

  void CallSetDestinationConsumer(WebAudioDestinationConsumer* consumer,
                                  int num_channels,
                                  float sample_rate) {
    Handler().SetConsumer(consumer, num_channels, sample_rate);
  }

  bool CallRemoveDestinationConsumer() {
    return Handler().RemoveConsumer();
  }

  void CallConsumeAudio(AudioBus *bus, int number_of_frames) {
    Handler().ConsumeAudio(bus, number_of_frames);
  }

  MediaStreamAudioDestinationHandler& Handler() {
    return static_cast<MediaStreamAudioDestinationHandler&>(node_->Handler());
  }

 protected:
  Persistent<MediaStreamAudioDestinationNode> node_;
  StrictMock<MockWebAudioDestinationConsumer> consumer_;
  scoped_refptr<AudioBus> bus_;
  ScopedTestingPlatformSupport<AudioContextTestPlatform> platform_;
  test::TaskEnvironment task_environment_;
};

TEST_F(MediaStreamAudioDestinationHandlerTest, SetDestinationConsumerWithNull) {
  // Setting the destination consumer with nullptr should not crash.
  CallSetDestinationConsumer(nullptr, 2, 44100);
}

TEST_F(MediaStreamAudioDestinationHandlerTest, SetDestinationConsumer) {
  // Expect SetFormat() to be called with these arguments.
  EXPECT_CALL(consumer_, SetFormat(2, 44100));
  CallSetDestinationConsumer(&consumer_, 2, 44100);

  EXPECT_CALL(consumer_, ConsumeAudio(_, 10));
  CallConsumeAudio(bus_.get(), 10);
}

TEST_F(MediaStreamAudioDestinationHandlerTest, RemoveDestinationConsumer) {
  EXPECT_CALL(consumer_, SetFormat(2, 44100));
  CallSetDestinationConsumer(&consumer_, 2, 44100);

  // The removal should be successful.
  EXPECT_TRUE(CallRemoveDestinationConsumer());

  // The consumer should not be called.
  EXPECT_CALL(consumer_, ConsumeAudio(_, 10)).Times(0);
  CallConsumeAudio(bus_.get(), 10);
}

TEST_F(MediaStreamAudioDestinationHandlerTest,
       ConsumeInvalidDestinationConsumer) {
  // The consumer should get no calls.
  EXPECT_CALL(consumer_, ConsumeAudio(_, 10)).Times(0);
  CallConsumeAudio(bus_.get(), 10);
}

TEST_F(MediaStreamAudioDestinationHandlerTest,
       RemoveInvalidDestinationConsumer) {
  // The MediaStreamAudioDestinationHandler constructor initializes the
  // destination consumer with a valid pointer. So the first removal is
  // always successful.
  EXPECT_TRUE(CallRemoveDestinationConsumer());

  // Resetting it for the second time should return false.
  EXPECT_FALSE(CallRemoveDestinationConsumer());
}

}  // namespace blink
