// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class NGFragmentItemsTest : public NGLayoutTest {};

// crbug.com/1147357
// DirtyLinesFromNeedsLayout() didn't work well with an orthogonal writing-mode
// root as a child, and it caused a failure of OOF descendants propagation.
TEST_F(NGFragmentItemsTest,
       DirtyLinesFromNeedsLayoutWithOrthogonalWritingMode) {
  SetBodyInnerHTML(R"HTML(
<style>
button {
  font-size: 100px;
}
#span1 {
  position: absolute;
}
code {
  writing-mode: vertical-rl;
}
</style>
<rt id="rt1"><span id="span1"></span></rt>
<button>
<code><ruby id="ruby1"></ruby></code>
b AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
</button>)HTML");
  RunDocumentLifecycle();

  GetDocument().getElementById("ruby1")->appendChild(
      GetDocument().getElementById("rt1"));
  RunDocumentLifecycle();

  EXPECT_TRUE(GetLayoutObjectByElementId("span1")->EverHadLayout());
}

TEST_F(NGFragmentItemsTest, IsContainerForCulledInline) {
  ScopedLayoutNGBlockFragmentationForTest ng_block_frag(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      #multicol {
        columns: 3;
        width: 40px;
        column-fill: auto;
        height: 100px;
        line-height: 30px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div id="multicol">
      <div id="container">
        <br><br><br><br><br>
        <span id="culled1">
          <br><br><br><br><br><br>
        </span>
        <span id="culled2">
          xxxxxxxxxxxxxxxxxxx
          xxxxxxxxxxxxxxxxxxx
          xxxxxxxxxxxxxxxxxxx
          xxxxxxxxxxxxxxxxxxx
          xxxxxxxxxxxxxxxxxxx
          xxxxxxxxxxxxxxxxxxx
        </span>
        <br><br><br>
      </div>
    </div>
  )HTML");

  const auto* container = GetLayoutBoxByElementId("container");
  const auto* culled1 =
      DynamicTo<LayoutInline>(GetLayoutObjectByElementId("culled1"));
  const auto* culled2 =
      DynamicTo<LayoutInline>(GetLayoutObjectByElementId("culled2"));

  ASSERT_TRUE(container);
  ASSERT_TRUE(culled1);
  ASSERT_TRUE(culled2);

  ASSERT_EQ(container->PhysicalFragmentCount(), 7u);
  const NGPhysicalBoxFragment* fragment = container->GetPhysicalFragment(0);
  ASSERT_TRUE(fragment->Items());
  bool is_first, is_last;
  EXPECT_FALSE(fragment->Items()->IsContainerForCulledInline(
      *culled1, &is_first, &is_last));
  EXPECT_FALSE(fragment->Items()->IsContainerForCulledInline(
      *culled2, &is_first, &is_last));

  fragment = container->GetPhysicalFragment(1);
  ASSERT_TRUE(fragment->Items());
  EXPECT_TRUE(fragment->Items()->IsContainerForCulledInline(*culled1, &is_first,
                                                            &is_last));
  EXPECT_TRUE(is_first);
  EXPECT_FALSE(is_last);
  EXPECT_FALSE(fragment->Items()->IsContainerForCulledInline(
      *culled2, &is_first, &is_last));

  fragment = container->GetPhysicalFragment(2);
  ASSERT_TRUE(fragment->Items());
  EXPECT_TRUE(fragment->Items()->IsContainerForCulledInline(*culled1, &is_first,
                                                            &is_last));
  EXPECT_FALSE(is_first);
  EXPECT_FALSE(is_last);
  EXPECT_FALSE(fragment->Items()->IsContainerForCulledInline(
      *culled2, &is_first, &is_last));

  fragment = container->GetPhysicalFragment(3);
  ASSERT_TRUE(fragment->Items());
  EXPECT_TRUE(fragment->Items()->IsContainerForCulledInline(*culled1, &is_first,
                                                            &is_last));
  EXPECT_FALSE(is_first);
  EXPECT_TRUE(is_last);
  EXPECT_TRUE(fragment->Items()->IsContainerForCulledInline(*culled2, &is_first,
                                                            &is_last));
  EXPECT_TRUE(is_first);
  EXPECT_FALSE(is_last);

  fragment = container->GetPhysicalFragment(4);
  ASSERT_TRUE(fragment->Items());
  EXPECT_FALSE(fragment->Items()->IsContainerForCulledInline(
      *culled1, &is_first, &is_last));
  EXPECT_TRUE(fragment->Items()->IsContainerForCulledInline(*culled2, &is_first,
                                                            &is_last));
  EXPECT_FALSE(is_first);
  EXPECT_FALSE(is_last);

  fragment = container->GetPhysicalFragment(5);
  ASSERT_TRUE(fragment->Items());
  EXPECT_FALSE(fragment->Items()->IsContainerForCulledInline(
      *culled1, &is_first, &is_last));
  EXPECT_TRUE(fragment->Items()->IsContainerForCulledInline(*culled2, &is_first,
                                                            &is_last));
  EXPECT_FALSE(is_first);
  EXPECT_TRUE(is_last);

  fragment = container->GetPhysicalFragment(6);
  ASSERT_TRUE(fragment->Items());
  EXPECT_FALSE(fragment->Items()->IsContainerForCulledInline(
      *culled1, &is_first, &is_last));
  EXPECT_FALSE(fragment->Items()->IsContainerForCulledInline(
      *culled2, &is_first, &is_last));
}

}  // namespace blink
