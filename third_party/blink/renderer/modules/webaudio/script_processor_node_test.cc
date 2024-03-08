// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/script_processor_node.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(ScriptProcessorNodeTest, BufferLifetime) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  ScriptProcessorNode* node =
      context->createScriptProcessor(ASSERT_NO_EXCEPTION);
  ScriptProcessorHandler& handler =
      static_cast<ScriptProcessorHandler&>(node->Handler());
  {
    base::AutoLock locker(handler.GetBufferLock());
    EXPECT_EQ(2u, handler.shared_input_buffers_.size());
    EXPECT_EQ(2u, handler.shared_input_buffers_.size());
  }
  DeferredTaskHandler::GraphAutoLocker graph_locker(context);
  handler.Dispose();
  // Buffers should live after dispose() because an audio thread is using
  // them.
  {
    base::AutoLock locker(handler.GetBufferLock());
    EXPECT_EQ(2u, handler.shared_input_buffers_.size());
    EXPECT_EQ(2u, handler.shared_input_buffers_.size());
  }
}

}  // namespace blink
