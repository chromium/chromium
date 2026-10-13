// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_source_node.h"

#include <memory>

#include "base/test/run_until.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_context_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_track.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/testing/mock_web_audio_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

constexpr double kAudioHardwareSampleRate = 44100.0;
constexpr size_t kAudioHardwareBufferSize = 128;

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

  double AudioHardwareSampleRate() override { return kAudioHardwareSampleRate; }
  size_t AudioHardwareBufferSize() override { return kAudioHardwareBufferSize; }
};

}  // namespace

// Makes sure `HasPendingActivity()` tracks whether the node is connected while
// the context is running.
TEST(MediaStreamAudioSourceNodeTest, HasPendingActivity) {
  test::TaskEnvironment task_environment;
  ScopedTestingPlatformSupport<AudioContextTestPlatform> platform;
  auto page = std::make_unique<DummyPageHolder>();
  AudioContext* context =
      AudioContext::Create(page->GetFrame().DomWindow(),
                           AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  auto* track = MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>();
  track->SetKind("audio");
  auto* media_stream =
      MediaStream::Create(page->GetFrame().DomWindow(), {track});
  MediaStreamAudioSourceNode* node = MediaStreamAudioSourceNode::Create(
      *context, *media_stream, ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return context->ContextState() == V8AudioContextState::Enum::kRunning;
  }));

  EXPECT_FALSE(node->HasPendingActivity());

  node->connect(context->destinationNode(), 0, 0, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(node->HasPendingActivity());

  node->disconnect();
  EXPECT_FALSE(node->HasPendingActivity());
}

}  // namespace blink
