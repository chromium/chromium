// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/script_processor_node.h"

namespace blink {

TEST(ScriptProcessorNodeTest, BufferLifetime) {
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      &page->GetDocument(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  ScriptProcessorNode* node =
      context->createScriptProcessor(ASSERT_NO_EXCEPTION);
  ScriptProcessorHandler& handler =
      static_cast<ScriptProcessorHandler&>(node->Handler());
  EXPECT_EQ(2u, handler.shared_input_buffers_.size());
  EXPECT_EQ(2u, handler.shared_input_buffers_.size());
  BaseAudioContext::GraphAutoLocker locker(context);
  handler.Dispose();
  // Buffers should live after dispose() because an audio thread is using
  // them.
  EXPECT_EQ(2u, handler.shared_input_buffers_.size());
  EXPECT_EQ(2u, handler.shared_input_buffers_.size());
}

}  // namespace blink
