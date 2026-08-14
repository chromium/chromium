// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/script_processor_node.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_offline_audio_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiocontextrendersizecategory_unsignedlong.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
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
  DeferredTaskHandler::GraphAutoLocker graph_locker(
      context->GetDeferredTaskHandler());
  handler.Dispose();
  // Buffers should live after dispose() because an audio thread is using
  // them.
  {
    base::AutoLock locker(handler.GetBufferLock());
    EXPECT_EQ(2u, handler.shared_input_buffers_.size());
    EXPECT_EQ(2u, handler.shared_input_buffers_.size());
  }
}

TEST(ScriptProcessorNodeTest, IncompatibleRenderQuantumSize) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(65536);
  options->setSampleRate(44100.0);
  options->setRenderSizeHint(
      MakeGarbageCollected<V8UnionAudioContextRenderSizeCategoryOrUnsignedLong>(
          32768u));
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(context->renderQuantumSize(), 32768u);

  const uint32_t test_sizes[] = {0, 256, 512, 1024, 2048, 4096, 8192, 16384};
  for (uint32_t size : test_sizes) {
    DummyExceptionStateForTesting exception_state;
    ScriptProcessorNode* node =
        context->createScriptProcessor(size, 1, 1, exception_state);
    EXPECT_EQ(node, nullptr);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kNotSupportedError);
  }
}

}  // namespace blink
