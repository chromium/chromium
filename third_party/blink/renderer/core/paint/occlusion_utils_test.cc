// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/occlusion_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class OcclusionUtilsTest : public RenderingTest {};

TEST_F(OcclusionUtilsTest, GetHitNodeOcclusion) {
  SetBodyInnerHTML(R"HTML(
    <div id="target"></div>
    <div id="empty"></div>
    <div id="invisible" style="opacity: 0;"></div>
    <div id="occluding"></div>
    <input id="input">
  )HTML");
  const Element& target = *GetElementById("target");
  const HitTestOcclusionTracker tracker(
      target,
      PaintedClientIds({GetLayoutObjectByElementId("invisible")->Id(),
                        GetLayoutObjectByElementId("occluding")->Id()}));

  EXPECT_EQ(tracker.GetHitNodeOcclusion(target), HitNodeOcclusion::kTarget);
  EXPECT_EQ(tracker.GetHitNodeOcclusion(*GetElementById("occluding")),
            HitNodeOcclusion::kOccluding);
  // E.g., the inner editor of a text field.
  const Node* user_agent_shadow_node =
      GetElementById("input")->UserAgentShadowRoot()->firstChild();
  ASSERT_TRUE(user_agent_shadow_node);
  EXPECT_EQ(tracker.GetHitNodeOcclusion(*user_agent_shadow_node),
            HitNodeOcclusion::kInUserAgentShadowTree);
  EXPECT_EQ(tracker.GetHitNodeOcclusion(*GetElementById("invisible")),
            HitNodeOcclusion::kZeroOpacity);
  EXPECT_EQ(tracker.GetHitNodeOcclusion(*GetElementById("empty")),
            HitNodeOcclusion::kNotPainted);

  // Without painted content to rely on, any node hides what's behind it.
  const HitTestOcclusionTracker default_tracker(target);
  EXPECT_EQ(default_tracker.GetHitNodeOcclusion(*GetElementById("empty")),
            HitNodeOcclusion::kOccluding);
}

TEST_F(OcclusionUtilsTest, OccludedArea) {
  SetBodyInnerHTML(R"HTML(<div id="target"></div>)HTML");
  HitTestOcclusionTracker tracker(*GetElementById("target"));
  EXPECT_EQ(tracker.OccludedArea(), 0);

  tracker.AddOccludedRect(gfx::Rect(0, 0, 50, 20));
  EXPECT_EQ(tracker.OccludedArea(), 1000);

  // The area hidden by several occluders is only counted once.
  tracker.AddOccludedRect(gfx::Rect(25, 0, 50, 20));
  EXPECT_EQ(tracker.OccludedArea(), 1500);

  tracker.Reset();
  EXPECT_EQ(tracker.OccludedArea(), 0);
}

}  // namespace blink
