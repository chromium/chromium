// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/convolver_node.h"

#include <memory>

#include "base/thread_annotations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(ConvolverNodeTest, ReverbLifetime) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  ConvolverNode* node = context->createConvolver(ASSERT_NO_EXCEPTION);
  ConvolverHandler& handler = node->GetConvolverHandler();
  // TS_UNCHECKED_READ: no threads here, testing only.
  EXPECT_FALSE(TS_UNCHECKED_READ(handler.reverb_));
  node->setBuffer(AudioBuffer::Create(2, 1, 48000), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(TS_UNCHECKED_READ(handler.reverb_));
  DeferredTaskHandler::GraphAutoLocker locker(context);
  handler.Dispose();
  // m_reverb should live after dispose() because an audio thread is using it.
  EXPECT_TRUE(TS_UNCHECKED_READ(handler.reverb_));
}

}  // namespace blink
