// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {

bool ForcesLegacyLayout(const Element& element) {
  return element.ShouldForceLegacyLayout() &&
         !element.GetLayoutObject()->IsLayoutNGMixin();
}

bool UsesNGLayout(const Element& element) {
  return !element.ShouldForceLegacyLayout() &&
         element.GetLayoutObject()->IsLayoutNGMixin();
}

}  // anonymous namespace

class ForceLegacyLayoutTest : public RenderingTest {
 public:
  ForceLegacyLayoutTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(ForceLegacyLayoutTest, ForceLegacyBfcRecalcAncestorStyle) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  // This test assumes that contenteditable forces the entire block formatting
  // context to use legacy layout. This will eventually change (when
  // contenteditable is natively supported in LayoutNG), and when that happens,
  // we'll need to come up with a different test here (provided that we're going
  // to keep the legacy-forcing machinery for other reasons than
  // contenteditable).
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id="bfc" style="overflow:hidden;">
      <div id="container" style="display:list-item;">
        <div id="middle">
          <div id="inner" style="overflow:hidden;">
            <div id="child"></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* body = GetDocument().body();
  Element* bfc = GetDocument().getElementById("bfc");
  Element* container = GetDocument().getElementById("container");
  Element* middle = GetDocument().getElementById("middle");
  Element* inner = GetDocument().getElementById("inner");
  Element* child = GetDocument().getElementById("child");

  // Initially, everything should be laid out by NG.
  EXPECT_TRUE(UsesNGLayout(*body));
  EXPECT_TRUE(UsesNGLayout(*bfc));
  EXPECT_TRUE(UsesNGLayout(*container));
  EXPECT_TRUE(UsesNGLayout(*middle));
  EXPECT_TRUE(UsesNGLayout(*inner));
  EXPECT_TRUE(UsesNGLayout(*child));

  // Enable contenteditable on an element that establishes a formatting context.
  inner->setAttribute(html_names::kContenteditableAttr, "true");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesNGLayout(*body));
  EXPECT_TRUE(UsesNGLayout(*bfc));
  EXPECT_TRUE(UsesNGLayout(*container));
  EXPECT_TRUE(UsesNGLayout(*middle));
  EXPECT_TRUE(ForcesLegacyLayout(*inner));
  EXPECT_TRUE(ForcesLegacyLayout(*child));

  // Remove overflow:hidden, so that the contenteditable element no longer
  // establishes a formatting context.
  inner->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesNGLayout(*body));
  EXPECT_TRUE(ForcesLegacyLayout(*bfc));
  EXPECT_TRUE(ForcesLegacyLayout(*container));
  EXPECT_TRUE(ForcesLegacyLayout(*middle));
  EXPECT_TRUE(ForcesLegacyLayout(*inner));
  EXPECT_TRUE(ForcesLegacyLayout(*child));

  // Change a non-inherited property. Legacy layout is triggered by #inner, but
  // should be propagated all the way up to #container (which is the node that
  // establishes the formatting context (due to overflow:hidden)). We'll now
  // test that this persists through style recalculation.
  middle->setAttribute(html_names::kStyleAttr, "background-color:blue;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesNGLayout(*body));
  EXPECT_TRUE(ForcesLegacyLayout(*bfc));
  EXPECT_TRUE(ForcesLegacyLayout(*container));
  EXPECT_TRUE(ForcesLegacyLayout(*middle));
  EXPECT_TRUE(ForcesLegacyLayout(*inner));
  EXPECT_TRUE(ForcesLegacyLayout(*child));

  // Change a property that requires re-attachment.
  container->setAttribute(html_names::kStyleAttr, "display:block;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesNGLayout(*body));
  EXPECT_TRUE(ForcesLegacyLayout(*bfc));
  EXPECT_TRUE(ForcesLegacyLayout(*container));
  EXPECT_TRUE(ForcesLegacyLayout(*middle));
  EXPECT_TRUE(ForcesLegacyLayout(*inner));
  EXPECT_TRUE(ForcesLegacyLayout(*child));
}

}  // namespace blink
