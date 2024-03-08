// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/stereo_panner_node.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(StereoPannerNodeTest, StereoPannerLifetime) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  StereoPannerNode* node = context->createStereoPanner(ASSERT_NO_EXCEPTION);
  StereoPannerHandler& handler =
      static_cast<StereoPannerHandler&>(node->Handler());
  EXPECT_TRUE(handler.stereo_panner_);
  DeferredTaskHandler::GraphAutoLocker locker(context);
  handler.Dispose();
  // m_stereoPanner should live after dispose() because an audio thread is
  // using it.
  EXPECT_TRUE(handler.stereo_panner_);
}

}  // namespace blink
