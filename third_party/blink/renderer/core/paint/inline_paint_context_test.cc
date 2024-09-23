// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_paint_context.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

String StringFromTextItem(const InlineCursor& cursor) {
  return cursor.Current().Text(cursor).ToString().StripWhiteSpace();
}

}  // namespace

class InlinePaintContextTest : public RenderingTest {
 public:
  InlinePaintContextTest() {}

  Vector<float> GetFontSizes(
      const InlinePaintContext::DecoratingBoxList& boxes) {
    Vector<float> font_sizes;
    for (const DecoratingBox& box : boxes) {
      font_sizes.push_back(box.Style().ComputedFontSize());
    }
    return font_sizes;
  }
};

TEST_F(InlinePaintContextTest, MultiLine) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
      margin: 0;
      width: 800px;
    }
    .ul {
      text-decoration-line: underline;
    }
    .no-cull {
      background: yellow;
    }
    </style>
    <div id="container" class="ul">
      <br><br>
      <span id="span" class="no-cull">
        0<br>1
      </span>
    </div>
  )HTML");
  // Test the `#span` fragment in the first line.
  const LayoutObject* span = GetLayoutObjectByElementId("span");
  InlineCursor cursor;
  cursor.MoveTo(*span);
  ASSERT_TRUE(cursor.Current());
  EXPECT_EQ(cursor.Current()->Type(), FragmentItem::kBox);
  const FragmentItem& span0_item = *cursor.Current();
  EXPECT_EQ(span0_item.InkOverflowRect(), PhysicalRect(0, 0, 10, 10));

  // Test the text "0".
  cursor.MoveToNext();
  EXPECT_EQ(StringFromTextItem(cursor), "0");
  const FragmentItem& text0_item = *cursor.Current();
  EXPECT_EQ(text0_item.InkOverflowRect(), PhysicalRect(0, 0, 10, 10));

  cursor.MoveToNext();
  EXPECT_TRUE(cursor.Current().IsLineBreak());
  const FragmentItem& br_item = *cursor.Current();
  EXPECT_EQ(br_item.InkOverflowRect(), PhysicalRect(0, 0, 0, 10));

  // Test the `#span` fragment in the second line.
  cursor.MoveToNext();
  EXPECT_EQ(cursor.Current()->Type(), FragmentItem::kLine);
  cursor.MoveToNext();
  EXPECT_EQ(cursor.Current()->Type(), FragmentItem::kBox);
  const FragmentItem& span1_item = *cursor.Current();
  EXPECT_EQ(span1_item.InkOverflowRect(), PhysicalRect(0, 0, 10, 10));

  // Test the text "1".
  cursor.MoveToNext();
  EXPECT_EQ(StringFromTextItem(cursor), "1");
  const FragmentItem& text1_item = *cursor.Current();
  EXPECT_EQ(text1_item.InkOverflowRect(), PhysicalRect(0, 0, 10, 10));

  // Test the containing block.
  const PhysicalBoxFragment& container_fragment = cursor.ContainerFragment();
  EXPECT_EQ(container_fragment.InkOverflowRect(), PhysicalRect(0, 0, 800, 40));
}

TEST_F(InlinePaintContextTest, VerticalAlign) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    :root {
      font-family: Ahem;
      font-size: 10px;
    }
    .ul {
      text-decoration-line: underline;
    }
    .up {
      vertical-align: 1em;
    }
    </style>
    <div>
      <span id="span1" class="ul">
        span1
        <span id="span2" class="up ul">
          span2
          <span id="span3" class="up">
            span3
          </span>
        </span>
      </span>
    </div>
  )HTML");

  InlineCursor cursor;
  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  cursor.MoveToIncludingCulledInline(*span1);
  EXPECT_EQ(cursor.Current().GetLayoutObject(), span1);
  const FragmentItem& span1_item = *cursor.Current();

  const LayoutObject* span2 = GetLayoutObjectByElementId("span2");
  cursor.MoveToIncludingCulledInline(*span2);
  EXPECT_EQ(cursor.Current().GetLayoutObject(), span2);
  const FragmentItem& span2_item = *cursor.Current();

  const LayoutObject* span3 = GetLayoutObjectByElementId("span3");
  cursor.MoveToIncludingCulledInline(*span3);
  EXPECT_EQ(StringFromTextItem(cursor), "span3");
  const FragmentItem& span3_item = *cursor.Current();

  // The bottom of ink overflows of `span1`, `span2`, and `span3` should match,
  // because underlines are drawn at the decorating box; i.e., `span1`.
  EXPECT_EQ(span1_item.InkOverflowRect().Bottom() +
                span1_item.OffsetInContainerFragment().top,
            span2_item.InkOverflowRect().Bottom() +
                span2_item.OffsetInContainerFragment().top);
  EXPECT_EQ(span1_item.InkOverflowRect().Bottom() +
                span1_item.OffsetInContainerFragment().top,
            span3_item.InkOverflowRect().Bottom() +
                span3_item.OffsetInContainerFragment().top);
}

TEST_F(InlinePaintContextTest, NestedBlocks) {
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

  InlinePaintContext context;
  const auto* ifc = To<LayoutBlockFlow>(GetLayoutObjectByElementId("ifc"));
  InlineCursor cursor(*ifc);
  cursor.MoveToFirstLine();
  context.SetLineBox(cursor);
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
  InlinePaintContext context2;
  context2.PushDecoratingBoxAncestors(cursor);
  EXPECT_THAT(GetFontSizes(context2.DecoratingBoxes()),
              testing::ElementsAre(20.f, 20.f, 10.f));
}

TEST_F(InlinePaintContextTest, StopPropagateTextDecorations) {
  // The `<rt>` element produces an inline box that stops propagations.
  SetBodyInnerHTML(R"HTML(
    <style>
    .ul {
      text-decoration: underline;
    }
    </style>
    <div class="ul">
      <rt>
        <u></u>
      </rt>
    </div>
  )HTML");
  // Test pass if no DCHECK failures.
}

}  // namespace blink
