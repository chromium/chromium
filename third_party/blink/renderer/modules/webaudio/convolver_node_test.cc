// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/convolver_node.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"

namespace blink {

TEST(ConvolverNodeTest, ReverbLifetime) {
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      &page->GetDocument(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  ConvolverNode* node = context->createConvolver(ASSERT_NO_EXCEPTION);
  ConvolverHandler& handler = node->GetConvolverHandler();
  EXPECT_FALSE(handler.reverb_);
  node->setBuffer(AudioBuffer::Create(2, 1, 48000), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(handler.reverb_);
  BaseAudioContext::GraphAutoLocker locker(context);
  handler.Dispose();
  // m_reverb should live after dispose() because an audio thread is using it.
  EXPECT_TRUE(handler.reverb_);
}

}  // namespace blink
