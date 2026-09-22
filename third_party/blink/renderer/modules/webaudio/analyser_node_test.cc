// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/analyser_node.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(AnalyserNodeTest, TailProcessingNoLeak) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  Persistent<OfflineAudioContext> context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 44100, ASSERT_NO_EXCEPTION);
  Persistent<AudioDestinationNode> dest = context->destinationNode();
  Persistent<AnalyserNode> upstream =
      AnalyserNode::Create(*context, ASSERT_NO_EXCEPTION);

  // Clear any pre-existing garbage before measuring initial count.
  WebHeap::CollectAllGarbageForTesting();

  uint32_t initial_count = InstanceCounters::CounterValue(
      InstanceCounters::kAudioHandlerCounter);

  {
    AnalyserNode* analyser =
        AnalyserNode::Create(*context, ASSERT_NO_EXCEPTION);

    // Connect an upstream node to the analyser's input. This causes the
    // AnalyserHandler to be added to the automatic pull list.
    upstream->connect(analyser, 0, 0, ASSERT_NO_EXCEPTION);

    // Disconnect the upstream node. This triggers tail processing on the
    // analyser, adding it to DeferredTaskHandler's tail processing list.
    upstream->disconnect(0u, ASSERT_NO_EXCEPTION);
  }

  // Force GC. The AnalyserNode is collected.
  WebHeap::CollectAllGarbageForTesting();

  {
    DeferredTaskHandler::GraphAutoLocker locker(
        context->GetDeferredTaskHandler());
    context->GetDeferredTaskHandler().ClearHandlersToBeDeleted();
  }

  uint32_t current_count = InstanceCounters::CounterValue(
      InstanceCounters::kAudioHandlerCounter);
  EXPECT_EQ(initial_count, current_count);
}

}  // namespace blink
