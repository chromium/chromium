// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_wiring.h"
#include "third_party/blink/renderer/modules/webaudio/delay_node.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(AudioNodeInputTest, InputDestroyedBeforeOutput) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  DelayNode* node1 = context->createDelay(ASSERT_NO_EXCEPTION);
  auto& handler1 = node1->Handler();
  DelayNode* node2 = context->createDelay(ASSERT_NO_EXCEPTION);
  auto& handler2 = node2->Handler();

  auto input = std::make_unique<AudioNodeInput>(handler1);
  auto output = std::make_unique<AudioNodeOutput>(&handler2, 0);

  {
    DeferredTaskHandler::GraphAutoLocker graph_lock(context);
    AudioNodeWiring::Connect(*output, *input);
    ASSERT_TRUE(output->IsConnected());

    // This should not crash.
    input.reset();
    output->Dispose();
    output.reset();
  }
}

TEST(AudioNodeInputTest, OutputDestroyedBeforeInput) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  DelayNode* node1 = context->createDelay(ASSERT_NO_EXCEPTION);
  auto& handler1 = node1->Handler();
  DelayNode* node2 = context->createDelay(ASSERT_NO_EXCEPTION);
  auto& handler2 = node2->Handler();

  auto input = std::make_unique<AudioNodeInput>(handler1);
  auto output = std::make_unique<AudioNodeOutput>(&handler2, 0);

  {
    DeferredTaskHandler::GraphAutoLocker graph_lock(context);
    AudioNodeWiring::Connect(*output, *input);
    ASSERT_TRUE(output->IsConnected());

    // This should not crash.
    output->Dispose();
    output.reset();
    input.reset();
  }
}

}  // namespace blink
