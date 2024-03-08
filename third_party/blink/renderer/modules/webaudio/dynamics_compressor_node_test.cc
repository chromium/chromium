// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/dynamics_compressor_node.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
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
  DeferredTaskHandler::GraphAutoLocker locker(context);
  handler.Dispose();
  // m_dynamicsCompressor should live after dispose() because an audio thread
  // is using it.
  EXPECT_TRUE(handler.dynamics_compressor_);
}

}  // namespace blink
