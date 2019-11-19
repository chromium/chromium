// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "cc/animation/animation_host.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/testing/compositor_test.h"

namespace blink {

class CompositorAnimationTimelineTest : public CompositorTest {};

TEST_F(CompositorAnimationTimelineTest,
       CompositorTimelineDeletionDetachesFromAnimationHost) {
  auto timeline = std::make_unique<CompositorAnimationTimeline>();

  scoped_refptr<cc::AnimationTimeline> cc_timeline =
      timeline->GetAnimationTimeline();
  EXPECT_FALSE(cc_timeline->animation_host());

  std::unique_ptr<cc::AnimationHost> animation_host =
      cc::AnimationHost::CreateMainInstance();

  animation_host->AddAnimationTimeline(timeline->GetAnimationTimeline());
  EXPECT_EQ(cc_timeline->animation_host(), animation_host.get());
  EXPECT_TRUE(animation_host->GetTimelineById(cc_timeline->id()));

  // Delete CompositorAnimationTimeline while attached to host.
  timeline = nullptr;

  EXPECT_EQ(cc_timeline->animation_host(), nullptr);
  EXPECT_FALSE(animation_host->GetTimelineById(cc_timeline->id()));
}

}  // namespace blink
