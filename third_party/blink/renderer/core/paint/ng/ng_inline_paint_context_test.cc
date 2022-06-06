// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class NGInlinePaintContextTest : public RenderingTest,
                                 private ScopedLayoutNGForTest,
                                 private ScopedTextDecoratingBoxForTest {
 public:
  NGInlinePaintContextTest()
      : ScopedLayoutNGForTest(true), ScopedTextDecoratingBoxForTest(true) {}

  Vector<float> GetFontSizes(
      const NGInlinePaintContext::DecoratingBoxList& boxes) {
    Vector<float> font_sizes;
    for (const NGDecoratingBox& box : boxes)
      font_sizes.push_back(box.Style().ComputedFontSize());
    return font_sizes;
  }
};

TEST_F(NGInlinePaintContextTest, NestedBlocks) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .ul {
      text-decoration: underline;
      background: yellow;  /* Ensure not to cull inline boxes. */
    }
    </style>
    <div class="ul" style="font-size: 40px">
      <div id="ifc" class="ul" style="font-size: 20px">
        <span id="span10" class="ul" style="font-size: 10px">
          <span id="span5" class="ul" style="font-size: 5px">10</span>
        </span>
      </div>
    </div>
  )HTML");

  NGInlinePaintContext context;
  const auto* ifc = To<LayoutBlockFlow>(GetLayoutObjectByElementId("ifc"));
  NGInlineCursor cursor(*ifc);
  cursor.MoveToFirstLine();
  context.SetLineBox(*cursor.Current());
  // Two text decorations are propagated to the `ifc`. The outer one does not
  // establish an inline formatting context, so the anonymous inline box of the
  // `ifc` is the decorating box of both decorations.
  EXPECT_THAT(GetFontSizes(context.DecoratingBoxes()),
              testing::ElementsAre(20.f, 20.f));

  const LayoutObject* span10 = GetLayoutObjectByElementId("span10");
  cursor.MoveTo(*span10);
  EXPECT_TRUE(cursor.Current());
  context.PushDecoratingBox(*cursor.Current());
  EXPECT_THAT(GetFontSizes(context.DecoratingBoxes()),
              testing::ElementsAre(20.f, 20.f, 10.f));

  const LayoutObject* span5 = GetLayoutObjectByElementId("span5");
  cursor.MoveTo(*span5);
  EXPECT_TRUE(cursor.Current());
  context.PushDecoratingBox(*cursor.Current());
  EXPECT_THAT(GetFontSizes(context.DecoratingBoxes()),
              testing::ElementsAre(20.f, 20.f, 10.f, 5.f));

  // Push all decorating boxes in the ancestor chain of the `span5`.
  NGInlinePaintContext context2;
  context2.PushDecoratingBoxAncestors(cursor);
  EXPECT_THAT(GetFontSizes(context2.DecoratingBoxes()),
              testing::ElementsAre(20.f, 20.f, 10.f));
}

}  // namespace blink
