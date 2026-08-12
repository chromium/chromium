// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/dynamics_compressor_node.h"

#include <algorithm>
#include <memory>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/dynamics_compressor.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(DynamicsCompressorNodeTest, ProcessorLifetime) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  DynamicsCompressorNode* node =
      context->createDynamicsCompressor(ASSERT_NO_EXCEPTION);
  DynamicsCompressorHandler& handler = node->GetDynamicsCompressorHandler();
  EXPECT_TRUE(handler.dynamics_compressor_);
  DeferredTaskHandler::GraphAutoLocker locker(
      context->GetDeferredTaskHandler());
  handler.Dispose();
  // m_dynamicsCompressor should live after dispose() because an audio thread
  // is using it.
  EXPECT_TRUE(handler.dynamics_compressor_);
}

TEST(DynamicsCompressorNodeTest, NonMultipleOf32QuantumProcessesAllFrames) {
  test::TaskEnvironment task_environment;
  constexpr unsigned kChannels = 2;
  constexpr float kSampleRate = 48000.0f;
  constexpr float kSentinel = -999.0f;

  for (unsigned frames_to_process :
       {0u, 1u, 3u, 31u, 32u, 35u, 64u, 128u, 130u}) {
    DynamicsCompressor compressor(kSampleRate, kChannels);
    scoped_refptr<AudioBus> source_bus =
        AudioBus::Create(kChannels, std::max(1u, frames_to_process));
    scoped_refptr<AudioBus> destination_bus =
        AudioBus::Create(kChannels, std::max(1u, frames_to_process));

    for (unsigned c = 0; c < kChannels; ++c) {
      std::ranges::fill(source_bus->Channel(c)->MutableSpan(), 0.5f);
      std::ranges::fill(destination_bus->Channel(c)->MutableSpan(), kSentinel);
    }

    compressor.Process(source_bus.get(), destination_bus.get(),
                       frames_to_process);

    for (unsigned c = 0; c < kChannels; ++c) {
      base::span<const float> dest_span = destination_bus->Channel(c)->Span();
      for (unsigned i = 0; i < frames_to_process; ++i) {
        EXPECT_NE(dest_span[i], kSentinel)
            << "frames_to_process " << frames_to_process << " channel " << c
            << " frame " << i << " left unwritten.";
      }
    }
  }
}

TEST(DynamicsCompressorNodeTest,
     MonoSourceNonMultipleOf32QuantumProcessesAllFrames) {
  test::TaskEnvironment task_environment;
  constexpr unsigned kSourceChannels = 1;
  constexpr unsigned kDestChannels = 2;
  constexpr unsigned frames_to_process = 35;
  constexpr float kSampleRate = 48000.0f;
  constexpr float kSentinel = -999.0f;

  DynamicsCompressor compressor(kSampleRate, kDestChannels);
  scoped_refptr<AudioBus> source_bus =
      AudioBus::Create(kSourceChannels, frames_to_process);
  scoped_refptr<AudioBus> destination_bus =
      AudioBus::Create(kDestChannels, frames_to_process);

  std::ranges::fill(source_bus->Channel(0)->MutableSpan(), 0.5f);
  for (unsigned c = 0; c < kDestChannels; ++c) {
    std::ranges::fill(destination_bus->Channel(c)->MutableSpan(), kSentinel);
  }

  compressor.Process(source_bus.get(), destination_bus.get(),
                     frames_to_process);

  for (unsigned c = 0; c < kDestChannels; ++c) {
    base::span<const float> dest_span = destination_bus->Channel(c)->Span();
    for (unsigned i = 0; i < frames_to_process; ++i) {
      EXPECT_NE(dest_span[i], kSentinel)
          << "channel " << c << " frame " << i << " left unwritten.";
    }
  }
}

}  // namespace blink
