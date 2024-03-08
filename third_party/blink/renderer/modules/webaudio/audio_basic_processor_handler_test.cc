// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_basic_processor_handler.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_processor.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

// Rendering size for these tests.  This is the WebAudio default rendering size.
constexpr unsigned kRenderQuantumFrames = 128;

}  // namespace

class MockAudioProcessor final : public AudioProcessor {
 public:
  MockAudioProcessor() : AudioProcessor(48000, 2, kRenderQuantumFrames) {}
  void Initialize() override { initialized_ = true; }
  void Uninitialize() override { initialized_ = false; }
  void Process(const AudioBus*, AudioBus*, uint32_t) override {}
  void Reset() override {}
  void SetNumberOfChannels(unsigned) override {}
  unsigned NumberOfChannels() const override { return number_of_channels_; }
  bool RequiresTailProcessing() const override { return true; }
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }
};

class MockProcessorHandler final : public AudioBasicProcessorHandler {
 public:
  static scoped_refptr<MockProcessorHandler> Create(AudioNode& node,
                                                    float sample_rate) {
    return base::AdoptRef(new MockProcessorHandler(node, sample_rate));
  }

 private:
  MockProcessorHandler(AudioNode& node, float sample_rate)
      : AudioBasicProcessorHandler(AudioHandler::kNodeTypeWaveShaper,
                                   node,
                                   sample_rate,
                                   std::make_unique<MockAudioProcessor>()) {
    Initialize();
  }
};

class MockProcessorNode final : public AudioNode {
 public:
  explicit MockProcessorNode(BaseAudioContext& context) : AudioNode(context) {
    SetHandler(MockProcessorHandler::Create(*this, 48000));
  }
  void ReportDidCreate() final {}
  void ReportWillBeDestroyed() final {}
};

TEST(AudioBasicProcessorHandlerTest, ProcessorFinalization) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  MockProcessorNode* node = MakeGarbageCollected<MockProcessorNode>(*context);
  AudioBasicProcessorHandler& handler =
      static_cast<AudioBasicProcessorHandler&>(node->Handler());
  EXPECT_TRUE(handler.Processor());
  EXPECT_TRUE(handler.Processor()->IsInitialized());
  DeferredTaskHandler::GraphAutoLocker locker(context);
  handler.Dispose();
  // The AudioProcessor should live after dispose() and should not be
  // finalized because an audio thread is using it.
  EXPECT_TRUE(handler.Processor());
  EXPECT_TRUE(handler.Processor()->IsInitialized());
}

}  // namespace blink
