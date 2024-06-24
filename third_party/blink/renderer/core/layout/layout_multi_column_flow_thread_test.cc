// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class MultiColumnRenderingTest : public RenderingTest {
 protected:
  LayoutMultiColumnFlowThread* FindFlowThread(const char* id) const;

  // Generate a signature string based on what kind of column boxes the flow
  // thread has established. 'c' is used for regular column content sets, while
  // 's' is used for spanners. '?' is used when there's an unknown box type
  // (which should be considered a failure).
  String ColumnSetSignature(LayoutMultiColumnFlowThread*);
  String ColumnSetSignature(const char* multicol_id);

  void SetMulticolHTML(const String&);
};

LayoutMultiColumnFlowThread* MultiColumnRenderingTest::FindFlowThread(
    const char* id) const {
  if (auto* multicol_container =
          To<LayoutBlockFlow>(GetLayoutObjectByElementId(id)))
    return multicol_container->MultiColumnFlowThread();
  return nullptr;
}

String MultiColumnRenderingTest::ColumnSetSignature(
    LayoutMultiColumnFlowThread* flow_thread) {
  StringBuilder signature;
  for (LayoutBox* column_box = flow_thread->FirstMultiColumnBox(); column_box;
       column_box = column_box->NextSiblingMultiColumnBox()) {
    if (column_box->IsLayoutMultiColumnSpannerPlaceholder())
      signature.Append('s');
    else if (column_box->IsLayoutMultiColumnSet())
      signature.Append('c');
    else
      signature.Append('?');
  }
  return signature.ToString();
}

String MultiColumnRenderingTest::ColumnSetSignature(const char* multicol_id) {
  return ColumnSetSignature(FindFlowThread(multicol_id));
}

void MultiColumnRenderingTest::SetMulticolHTML(const String& html) {
  const char* style =
      "<style>"
      "  #mc { columns:2; }"
      "  .s, #spanner, #spanner1, #spanner2 { column-span:all; }"
      "</style>";
  SetBodyInnerHTML(style + html);
}

TEST_F(MultiColumnRenderingTest, OneBlockWithInDepthTreeStructureCheck) {
  // Examine the layout tree established by a simple multicol container with a
  // block with some text inside.
  SetMulticolHTML("<div id='mc'><div>xxx</div></div>");
  auto* multicol_container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("mc"));
  ASSERT_TRUE(multicol_container);
  LayoutMultiColumnFlowThread* flow_thread =
      multicol_container->MultiColumnFlowThread();
  ASSERT_TRUE(flow_thread);
  EXPECT_EQ(ColumnSetSignature(flow_thread), "c");
  EXPECT_EQ(flow_thread->Parent(), multicol_container);
  EXPECT_FALSE(flow_thread->PreviousSibling());
  LayoutMultiColumnSet* column_set = flow_thread->FirstMultiColumnSet();
  ASSERT_TRUE(column_set);
  EXPECT_EQ(column_set->PreviousSibling(), flow_thread);
  EXPECT_FALSE(column_set->NextSibling());
  auto* block = To<LayoutBlockFlow>(flow_thread->FirstChild());
  ASSERT_TRUE(block);
  EXPECT_FALSE(block->NextSibling());
  ASSERT_TRUE(block->FirstChild());
  EXPECT_TRUE(block->FirstChild()->IsText());
  EXPECT_FALSE(block->FirstChild()->NextSibling());
}

TEST_F(MultiColumnRenderingTest, Empty) {
  // If there's no column content, there should be no column set.
  SetMulticolHTML("<div id='mc'></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnRenderingTest, OneBlock) {
  // There is some content, so we should create a column set.
  SetMulticolHTML("<div id='mc'><div id='block'></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  ASSERT_EQ(ColumnSetSignature(flow_thread), "c");
  LayoutMultiColumnSet* column_set = flow_thread->FirstMultiColumnSet();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("block")),
            column_set);
}

TEST_F(MultiColumnRenderingTest, TwoBlocks) {
  // No matter how much content, we should only create one column set (unless
  // there are spanners).
  SetMulticolHTML(
      "<div id='mc'><div id='block1'></div><div id='block2'></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  ASSERT_EQ(ColumnSetSignature(flow_thread), "c");
  LayoutMultiColumnSet* column_set = flow_thread->FirstMultiColumnSet();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("block1")),
            column_set);
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("block2")),
            column_set);
}

TEST_F(MultiColumnRenderingTest, Spanner) {
  // With one spanner and no column content, we should create a spanner set.
  SetMulticolHTML("<div id='mc'><div id='spanner'></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  ASSERT_EQ(ColumnSetSignature(flow_thread), "s");
  LayoutBox* column_box = flow_thread->FirstMultiColumnBox();
  EXPECT_EQ(flow_thread->FirstMultiColumnSet(), nullptr);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner")),
            column_box);
  EXPECT_EQ(GetLayoutObjectByElementId("spanner")->SpannerPlaceholder(),
            column_box);
}

TEST_F(MultiColumnRenderingTest, ContentThenSpanner) {
  // With some column content followed by a spanner, we need a column set
  // followed by a spanner set.
  SetMulticolHTML(
      "<div id='mc'><div id='columnContent'></div><div "
      "id='spanner'></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  ASSERT_EQ(ColumnSetSignature(flow_thread), "cs");
  LayoutBox* column_box = flow_thread->FirstMultiColumnBox();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("columnContent")),
            column_box);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner")),
            column_box);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("columnContent")),
            nullptr);
}

TEST_F(MultiColumnRenderingTest, SpannerThenContent) {
  // With a spanner followed by some column content, we need a spanner set
  // followed by a column set.
  SetMulticolHTML(
      "<div id='mc'><div id='spanner'></div><div "
      "id='columnContent'></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  ASSERT_EQ(ColumnSetSignature(flow_thread), "sc");
  LayoutBox* column_box = flow_thread->FirstMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner")),
            column_box);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("columnContent")),
            column_box);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("columnContent")),
            nullptr);
}

TEST_F(MultiColumnRenderingTest, ContentThenSpannerThenContent) {
  // With column content followed by a spanner followed by some column content,
  // we need a column
  // set followed by a spanner set followed by a column set.
  SetMulticolHTML(
      "<div id='mc'><div id='columnContentBefore'></div><div "
      "id='spanner'></div><div id='columnContentAfter'></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  ASSERT_EQ(ColumnSetSignature(flow_thread), "csc");
  LayoutBox* column_box = flow_thread->FirstMultiColumnSet();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("columnContentBefore")),
            column_box);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("columnContentBefore")),
            nullptr);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner")),
            column_box);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("columnContentAfter")),
            column_box);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("columnContentAfter")),
            nullptr);
}

TEST_F(MultiColumnRenderingTest, TwoSpanners) {
  // With two spanners and no column content, we need two spanner sets.
  SetMulticolHTML(
      "<div id='mc'><div id='spanner1'></div><div id='spanner2'></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  ASSERT_EQ(ColumnSetSignature(flow_thread), "ss");
  LayoutBox* column_box = flow_thread->FirstMultiColumnBox();
  EXPECT_EQ(flow_thread->FirstMultiColumnSet(), nullptr);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner1")),
            column_box);
  EXPECT_EQ(GetLayoutObjectByElementId("spanner1")->SpannerPlaceholder(),
            column_box);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner2")),
            column_box);
  EXPECT_EQ(GetLayoutObjectByElementId("spanner2")->SpannerPlaceholder(),
            column_box);
}

TEST_F(MultiColumnRenderingTest, SpannerThenContentThenSpanner) {
  // With two spanners and some column content in-between, we need a spanner
  // set, a column set and another spanner set.
  SetMulticolHTML(
      "<div id='mc'><div id='spanner1'></div><div "
      "id='columnContent'></div><div id='spanner2'></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  ASSERT_EQ(ColumnSetSignature(flow_thread), "scs");
  LayoutMultiColumnSet* column_set = flow_thread->FirstMultiColumnSet();
  EXPECT_EQ(column_set->NextSiblingMultiColumnSet(), nullptr);
  LayoutBox* column_box = flow_thread->FirstMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner1")),
            column_box);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(column_box, column_set);
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("columnContent")),
            column_set);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("columnContent")),
            nullptr);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner2")),
            column_box);
}

TEST_F(MultiColumnRenderingTest, SpannerWithSpanner) {
  // column-span:all on something inside column-span:all has no effect.
  SetMulticolHTML(
      "<div id='mc'><div id='spanner'><div id='invalidSpanner' "
      "class='s'></div></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  ASSERT_EQ(ColumnSetSignature(flow_thread), "s");
  LayoutBox* column_box = flow_thread->FirstMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner")),
            column_box);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("invalidSpanner")),
            column_box);
  EXPECT_EQ(To<LayoutMultiColumnSpannerPlaceholder>(column_box)
                ->LayoutObjectInFlowThread(),
            GetLayoutObjectByElementId("spanner"));
  EXPECT_EQ(GetLayoutObjectByElementId("spanner")->SpannerPlaceholder(),
            column_box);
  EXPECT_EQ(GetLayoutObjectByElementId("invalidSpanner")->SpannerPlaceholder(),
            nullptr);
}

TEST_F(MultiColumnRenderingTest, SubtreeWithSpanner) {
  SetMulticolHTML(
      "<div id='mc'><div id='outer'><div id='block1'></div><div "
      "id='spanner'></div><div id='block2'></div></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  EXPECT_EQ(ColumnSetSignature(flow_thread), "csc");
  LayoutBox* column_box = flow_thread->FirstMultiColumnBox();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("outer")),
            column_box);
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("block1")),
            column_box);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner")),
            column_box);
  EXPECT_EQ(GetLayoutObjectByElementId("spanner")->SpannerPlaceholder(),
            column_box);
  EXPECT_EQ(To<LayoutMultiColumnSpannerPlaceholder>(column_box)
                ->LayoutObjectInFlowThread(),
            GetLayoutObjectByElementId("spanner"));
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("outer")),
            nullptr);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("block1")),
            nullptr);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("block2")),
            nullptr);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("block2")),
            column_box);
}

TEST_F(MultiColumnRenderingTest, SubtreeWithSpannerAfterSpanner) {
  SetMulticolHTML(
      "<div id='mc'><div id='spanner1'></div><div id='outer'>text<div "
      "id='spanner2'></div><div id='after'></div></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  EXPECT_EQ(ColumnSetSignature(flow_thread), "scsc");
  LayoutBox* column_box = flow_thread->FirstMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner1")),
            column_box);
  EXPECT_EQ(To<LayoutMultiColumnSpannerPlaceholder>(column_box)
                ->LayoutObjectInFlowThread(),
            GetLayoutObjectByElementId("spanner1"));
  EXPECT_EQ(GetLayoutObjectByElementId("spanner1")->SpannerPlaceholder(),
            column_box);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("outer")),
            column_box);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner2")),
            column_box);
  EXPECT_EQ(To<LayoutMultiColumnSpannerPlaceholder>(column_box)
                ->LayoutObjectInFlowThread(),
            GetLayoutObjectByElementId("spanner2"));
  EXPECT_EQ(GetLayoutObjectByElementId("spanner2")->SpannerPlaceholder(),
            column_box);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("outer")),
            nullptr);
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("after")),
            nullptr);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("after")),
            column_box);
}

TEST_F(MultiColumnRenderingTest, SubtreeWithSpannerBeforeSpanner) {
  SetMulticolHTML(
      "<div id='mc'><div id='outer'>text<div "
      "id='spanner1'></div>text</div><div id='spanner2'></div></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  EXPECT_EQ(ColumnSetSignature(flow_thread), "cscs");
  LayoutBox* column_box = flow_thread->FirstMultiColumnSet();
  EXPECT_EQ(flow_thread->MapDescendantToColumnSet(
                GetLayoutObjectByElementId("outer")),
            column_box);
  column_box = column_box->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner1")),
            column_box);
  EXPECT_EQ(GetLayoutObjectByElementId("spanner1")->SpannerPlaceholder(),
            column_box);
  EXPECT_EQ(To<LayoutMultiColumnSpannerPlaceholder>(column_box)
                ->LayoutObjectInFlowThread(),
            GetLayoutObjectByElementId("spanner1"));
  column_box =
      column_box->NextSiblingMultiColumnBox()->NextSiblingMultiColumnBox();
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("spanner2")),
            column_box);
  EXPECT_EQ(GetLayoutObjectByElementId("spanner2")->SpannerPlaceholder(),
            column_box);
  EXPECT_EQ(To<LayoutMultiColumnSpannerPlaceholder>(column_box)
                ->LayoutObjectInFlowThread(),
            GetLayoutObjectByElementId("spanner2"));
  EXPECT_EQ(flow_thread->ContainingColumnSpannerPlaceholder(
                GetLayoutObjectByElementId("outer")),
            nullptr);
}

TEST_F(MultiColumnRenderingTest, columnSetAtBlockOffset) {
  SetMulticolHTML(R"HTML(
      <div id='mc' style='line-height:100px;'>
        text<br>
        text<br>
        text<br>
        text<br>
        text
        <div id='spanner1'>spanner</div>
        text<br>
        text
        <div id='spanner2'>
          text<br>
          text
        </div>
        text
      </div>
  )HTML");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  EXPECT_EQ(ColumnSetSignature(flow_thread), "cscsc");
  LayoutMultiColumnSet* first_row = flow_thread->FirstMultiColumnSet();
  LayoutMultiColumnSet* second_row = first_row->NextSiblingMultiColumnSet();
  LayoutMultiColumnSet* third_row = second_row->NextSiblingMultiColumnSet();
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(-10000), LayoutBox::kAssociateWithFormerPage),
            first_row);  // negative overflow
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(-10000), LayoutBox::kAssociateWithLatterPage),
            first_row);  // negative overflow
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(), LayoutBox::kAssociateWithFormerPage),
            first_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(), LayoutBox::kAssociateWithLatterPage),
            first_row);
  LayoutUnit offset(600);
  // The first column row contains 5 lines, split into two columns, i.e. 3 lines
  // in the first and 2 lines in the second. Line height is 100px. There's 100px
  // of unused space at the end of the second column.
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithFormerPage),
            first_row);  // bottom of last line in first row.
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithLatterPage),
            first_row);  // bottom of last line in first row.
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithFormerPage),
            first_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithLatterPage),
            second_row);
  offset += LayoutUnit(200);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithFormerPage),
            second_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithLatterPage),
            second_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithFormerPage),
            second_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithLatterPage),
            third_row);
  offset += LayoutUnit(100);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithLatterPage),
            third_row);  // bottom of last row
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(10000), LayoutBox::kAssociateWithFormerPage),
            third_row);  // overflow
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(10000), LayoutBox::kAssociateWithLatterPage),
            third_row);  // overflow
}

TEST_F(MultiColumnRenderingTest, columnSetAtBlockOffsetVerticalRl) {
  SetMulticolHTML(R"HTML(
      <div id='mc' style='line-height:100px; writing-mode:vertical-rl;'>
        text<br>
        text<br>
        text<br>
        text<br>
        text
        <div id='spanner1'>spanner</div>
        text<br>
        text
        <div id='spanner2'>
          text<br>
          text
        </div>
        text
      </div>
  )HTML");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  EXPECT_EQ(ColumnSetSignature(flow_thread), "cscsc");
  LayoutMultiColumnSet* first_row = flow_thread->FirstMultiColumnSet();
  LayoutMultiColumnSet* second_row = first_row->NextSiblingMultiColumnSet();
  LayoutMultiColumnSet* third_row = second_row->NextSiblingMultiColumnSet();
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(-10000), LayoutBox::kAssociateWithFormerPage),
            first_row);  // negative overflow
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(-10000), LayoutBox::kAssociateWithLatterPage),
            first_row);  // negative overflow
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(), LayoutBox::kAssociateWithFormerPage),
            first_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(), LayoutBox::kAssociateWithLatterPage),
            first_row);
  LayoutUnit offset(600);
  // The first column row contains 5 lines, split into two columns, i.e. 3 lines
  // in the first and 2 lines in the second. Line height is 100px. There's 100px
  // of unused space at the end of the second column.
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithFormerPage),
            first_row);  // bottom of last line in first row.
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithLatterPage),
            first_row);  // bottom of last line in first row.
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithFormerPage),
            first_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithLatterPage),
            second_row);
  offset += LayoutUnit(200);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithFormerPage),
            second_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithLatterPage),
            second_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithFormerPage),
            second_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithLatterPage),
            third_row);
  offset += LayoutUnit(100);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithLatterPage),
            third_row);  // bottom of last row
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(10000), LayoutBox::kAssociateWithFormerPage),
            third_row);  // overflow
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(10000), LayoutBox::kAssociateWithLatterPage),
            third_row);  // overflow
}

TEST_F(MultiColumnRenderingTest, columnSetAtBlockOffsetVerticalLr) {
  SetMulticolHTML(R"HTML(
      <div id='mc' style='line-height:100px; writing-mode:vertical-lr;'>
        text<br>
        text<br>
        text<br>
        text<br>
        text
        <div id='spanner1'>spanner</div>
        text<br>
        text
        <div id='spanner2'>
          text<br>
          text
        </div>
        text
      </div>
  )HTML");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  EXPECT_EQ(ColumnSetSignature(flow_thread), "cscsc");
  LayoutMultiColumnSet* first_row = flow_thread->FirstMultiColumnSet();
  LayoutMultiColumnSet* second_row = first_row->NextSiblingMultiColumnSet();
  LayoutMultiColumnSet* third_row = second_row->NextSiblingMultiColumnSet();
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(-10000), LayoutBox::kAssociateWithFormerPage),
            first_row);  // negative overflow
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(-10000), LayoutBox::kAssociateWithLatterPage),
            first_row);  // negative overflow
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(), LayoutBox::kAssociateWithFormerPage),
            first_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(), LayoutBox::kAssociateWithLatterPage),
            first_row);
  LayoutUnit offset(600);
  // The first column row contains 5 lines, split into two columns, i.e. 3 lines
  // in the first and 2 lines in the second. Line height is 100px. There's 100px
  // of unused space at the end of the second column.
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithFormerPage),
            first_row);  // bottom of last line in first row.
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithLatterPage),
            first_row);  // bottom of last line in first row.
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithFormerPage),
            first_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithLatterPage),
            second_row);
  offset += LayoutUnit(200);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithFormerPage),
            second_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithLatterPage),
            second_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithFormerPage),
            second_row);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset, LayoutBox::kAssociateWithLatterPage),
            third_row);
  offset += LayoutUnit(100);
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                offset - LayoutUnit(1), LayoutBox::kAssociateWithLatterPage),
            third_row);  // bottom of last row
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(10000), LayoutBox::kAssociateWithFormerPage),
            third_row);  // overflow
  EXPECT_EQ(flow_thread->ColumnSetAtBlockOffset(
                LayoutUnit(10000), LayoutBox::kAssociateWithLatterPage),
            third_row);  // overflow
}

class MultiColumnTreeModifyingTest : public MultiColumnRenderingTest {
 public:
  void SetMulticolHTML(const char*);
  void ReparentLayoutObject(const char* new_parent_id,
                            const char* child_id,
                            const char* insert_before_id = nullptr);
  void DestroyLayoutObject(LayoutObject* child);
  void DestroyLayoutObject(const char* child_id);
};

void MultiColumnTreeModifyingTest::SetMulticolHTML(const char* html) {
  MultiColumnRenderingTest::SetMulticolHTML(html);
  // Allow modifications to the layout tree structure, because that's what we
  // want to test.
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
}

void MultiColumnTreeModifyingTest::ReparentLayoutObject(
    const char* new_parent_id,
    const char* child_id,
    const char* insert_before_id) {
  LayoutObject* new_parent = GetLayoutObjectByElementId(new_parent_id);
  LayoutObject* child = GetLayoutObjectByElementId(child_id);
  LayoutObject* insert_before =
      insert_before_id ? GetLayoutObjectByElementId(insert_before_id) : nullptr;
  child->Remove();
  new_parent->AddChild(child, insert_before);
}

void MultiColumnTreeModifyingTest::DestroyLayoutObject(LayoutObject* child) {
  // Remove and destroy in separate steps, so that we get to test removal of
  // subtrees.
  child->Remove();
  child->GetNode()->DetachLayoutTree();
}

void MultiColumnTreeModifyingTest::DestroyLayoutObject(const char* child_id) {
  DestroyLayoutObject(GetLayoutObjectByElementId(child_id));
}

TEST_F(MultiColumnTreeModifyingTest, InsertFirstContentAndRemove) {
  SetMulticolHTML("<div id='block'></div><div id='mc'></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  auto* block = To<LayoutBlockFlow>(GetLayoutObjectByElementId("block"));
  auto* multicol_container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("mc"));
  block->Remove();
  multicol_container->AddChild(block);
  EXPECT_EQ(block->Parent(), flow_thread);
  // A set should have appeared, now that the multicol container has content.
  EXPECT_EQ(ColumnSetSignature(flow_thread), "c");

  DestroyLayoutObject(block);
  // The set should be gone again now, since there's nothing inside the multicol
  // container anymore.
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest, InsertContentBeforeContentAndRemove) {
  SetMulticolHTML(
      "<div id='block'></div><div id='mc'><div id='insertBefore'></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
  ReparentLayoutObject("mc", "block", "insertBefore");
  // There was already some content prior to our insertion, so no new set should
  // be inserted.
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
  DestroyLayoutObject("block");
  // There's still some content after the removal, so the set should remain.
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest, InsertContentAfterContentAndRemove) {
  SetMulticolHTML("<div id='block'></div><div id='mc'><div></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
  ReparentLayoutObject("mc", "block");
  // There was already some content prior to our insertion, so no new set should
  // be inserted.
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
  DestroyLayoutObject("block");
  // There's still some content after the removal, so the set should remain.
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest, InsertSpannerAndRemove) {
  SetMulticolHTML("<div id='spanner'></div><div id='mc'></div>");
  LayoutMultiColumnFlowThread* flow_thread = FindFlowThread("mc");
  auto* spanner = To<LayoutBlockFlow>(GetLayoutObjectByElementId("spanner"));
  auto* multicol_container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("mc"));
  spanner->Remove();
  multicol_container->AddChild(spanner);
  EXPECT_EQ(spanner->Parent(), flow_thread);
  // We should now have a spanner placeholder, since we just moved a spanner
  // into the multicol container.
  EXPECT_EQ(ColumnSetSignature(flow_thread), "s");
  DestroyLayoutObject(spanner);
  EXPECT_EQ(ColumnSetSignature(flow_thread), "");
}

TEST_F(MultiColumnTreeModifyingTest, InsertTwoSpannersAndRemove) {
  SetMulticolHTML(
      "<div id='block'>ee<div class='s'></div><div class='s'></div></div><div "
      "id='mc'></div>");
  ReparentLayoutObject("mc", "block");
  EXPECT_EQ(ColumnSetSignature("mc"), "css");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest, InsertSpannerAfterContentAndRemove) {
  SetMulticolHTML("<div id='spanner'></div><div id='mc'><div></div></div>");
  ReparentLayoutObject("mc", "spanner");
  // We should now have a spanner placeholder, since we just moved a spanner
  // into the multicol container.
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest, InsertSpannerBeforeContentAndRemove) {
  SetMulticolHTML(
      "<div id='spanner'></div><div id='mc'><div "
      "id='columnContent'></div></div>");
  ReparentLayoutObject("mc", "spanner", "columnContent");
  // We should now have a spanner placeholder, since we just moved a spanner
  // into the multicol container.
  EXPECT_EQ(ColumnSetSignature("mc"), "sc");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest, InsertSpannerBetweenContentAndRemove) {
  SetMulticolHTML(
      "<div id='spanner'></div><div id='mc'><div></div><div "
      "id='insertBefore'></div></div>");
  ReparentLayoutObject("mc", "spanner", "insertBefore");
  // Since the spanner was inserted in the middle of column content, what used
  // to be one column set had to be split in two, in order to get a spot to
  // insert the spanner placeholder.
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("spanner");
  // The spanner placeholder should be gone again now, and the two sets be
  // merged into one.
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSubtreeWithContentAndSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='block'>text<div id='spanner'></div>text</div><div "
      "id='mc'></div>");
  ReparentLayoutObject("mc", "block");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest, InsertInsideSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='block'>text</div><div id='mc'><div id='spanner'></div></div>");
  ReparentLayoutObject("spanner", "block");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSpannerInContentBeforeSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='spanner'></div><div id='mc'><div></div><div "
      "id='insertBefore'></div><div class='s'></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
  ReparentLayoutObject("mc", "spanner", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "cscs");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSpannerInContentAfterSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='spanner'></div><div id='mc'><div "
      "class='s'></div><div></div><div id='insertBefore'></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "sc");
  ReparentLayoutObject("mc", "spanner", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "scsc");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "sc");
}

TEST_F(MultiColumnTreeModifyingTest, InsertSpannerAfterSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='spanner'></div><div id='mc'><div class='s'></div></div>");
  ReparentLayoutObject("mc", "spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "ss");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
}

TEST_F(MultiColumnTreeModifyingTest, InsertSpannerBeforeSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='spanner'></div><div id='mc'><div id='insertBefore' "
      "class='s'></div></div>");
  ReparentLayoutObject("mc", "spanner", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "ss");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
}

TEST_F(MultiColumnTreeModifyingTest, InsertContentBeforeSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='block'></div><div id='mc'><div id='insertBefore' "
      "class='s'></div></div>");
  ReparentLayoutObject("mc", "block", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertContentAfterContentBeforeSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='block'></div><div id='mc'>text<div id='insertBefore' "
      "class='s'></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
  ReparentLayoutObject("mc", "block", "insertBefore");
  // There was already some content before the spanner prior to our insertion,
  // so no new set should be inserted.
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertContentAfterContentAndSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='block'></div><div id='mc'>content<div class='s'></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
  ReparentLayoutObject("mc", "block");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertContentBeforeSpannerAndContentAndRemove) {
  SetMulticolHTML(
      "<div id='block'></div><div id='mc'><div id='insertBefore' "
      "class='s'></div>content</div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "sc");
  ReparentLayoutObject("mc", "block", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "sc");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSpannerIntoContentBeforeSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='spanner'></div><div id='mc'><div></div><div "
      "id='insertBefore'></div><div class='s'></div><div "
      "class='s'></div><div></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cssc");
  ReparentLayoutObject("mc", "spanner", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "cscssc");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "cssc");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSpannerIntoContentAfterSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='spanner'></div><div id='mc'><div></div><div "
      "class='s'></div><div class='s'></div><div></div><div "
      "id='insertBefore'></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cssc");
  ReparentLayoutObject("mc", "spanner", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "csscsc");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "cssc");
}

TEST_F(MultiColumnTreeModifyingTest, InsertInvalidSpannerAndRemove) {
  SetMulticolHTML(
      "<div class='s' id='invalidSpanner'></div><div id='mc'><div "
      "id='spanner'></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
  ReparentLayoutObject("spanner", "invalidSpanner");
  // It's not allowed to nest spanners.
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
  DestroyLayoutObject("invalidSpanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
}

TEST_F(MultiColumnTreeModifyingTest, InsertSpannerWithInvalidSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='spanner'><div class='s' id='invalidSpanner'></div></div><div "
      "id='mc'></div>");
  ReparentLayoutObject("mc", "spanner");
  // It's not allowed to nest spanners.
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertInvalidSpannerInSpannerBetweenContentAndRemove) {
  SetMulticolHTML(
      "<div class='s' id='invalidSpanner'></div><div id='mc'>text<div "
      "id='spanner'></div>text</div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  ReparentLayoutObject("spanner", "invalidSpanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("invalidSpanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
}

TEST_F(MultiColumnTreeModifyingTest, InsertContentAndSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='block'>text<div id='spanner'></div></div><div "
      "id='mc'>text</div>");
  ReparentLayoutObject("mc", "block");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertContentAndSpannerAndContentAndRemove) {
  SetMulticolHTML(
      "<div id='block'><div id='spanner'></div>text</div><div id='mc'></div>");
  ReparentLayoutObject("mc", "block");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest, InsertSubtreeWithSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='block'>text<div class='s'></div>text</div><div id='mc'></div>");
  ReparentLayoutObject("mc", "block");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSubtreeWithSpannerAfterContentAndRemove) {
  SetMulticolHTML(
      "<div id='block'>text<div class='s'></div>text</div><div id='mc'>column "
      "content</div>");
  ReparentLayoutObject("mc", "block");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSubtreeWithSpannerBeforeContentAndRemove) {
  SetMulticolHTML(
      "<div id='block'>text<div class='s'></div>text</div><div id='mc'><div "
      "id='insertBefore'>column content</div></div>");
  ReparentLayoutObject("mc", "block", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSubtreeWithSpannerInsideContentAndRemove) {
  SetMulticolHTML(
      "<div id='block'>text<div class='s'></div>text</div><div id='mc'><div "
      "id='newParent'>outside<div id='insertBefore'>outside</div></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
  ReparentLayoutObject("newParent", "block", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSubtreeWithSpannerAfterSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='block'>text<div class='s'></div>text</div><div id='mc'><div "
      "class='s'></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
  ReparentLayoutObject("mc", "block");
  EXPECT_EQ(ColumnSetSignature("mc"), "scsc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
}

TEST_F(MultiColumnTreeModifyingTest,
       InsertSubtreeWithSpannerBeforeSpannerAndRemove) {
  SetMulticolHTML(
      "<div id='block'>text<div class='s'></div>text</div><div id='mc'><div "
      "id='insertBefore' class='s'></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
  ReparentLayoutObject("mc", "block", "insertBefore");
  EXPECT_EQ(ColumnSetSignature("mc"), "cscs");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "s");
}

TEST_F(MultiColumnTreeModifyingTest, RemoveSpannerAndContent) {
  SetMulticolHTML(
      "<div id='mc'><div id='block'>text<div class='s'></div>text</div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest, RemoveSpannerAndSomeContentBefore) {
  SetMulticolHTML(
      "<div id='mc'>text<div id='block'>text<div class='s'></div></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest, RemoveSpannerAndAllContentBefore) {
  SetMulticolHTML(
      "<div id='mc'><div id='block'>text<div class='s'></div></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cs");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest,
       RemoveSpannerAndAllContentBeforeWithContentAfter) {
  SetMulticolHTML(
      "<div id='mc'><div id='block'>text<div class='s'></div></div>text</div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest, RemoveSpannerAndSomeContentAfter) {
  SetMulticolHTML(
      "<div id='mc'><div id='block'><div class='s'></div>text</div>text</div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest, RemoveSpannerAndAllContentAfter) {
  SetMulticolHTML(
      "<div id='mc'><div id='block'><div class='s'></div>text</div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest,
       RemoveSpannerAndAllContentAfterWithContentBefore) {
  SetMulticolHTML(
      "<div id='mc'>text<div id='block'><div class='s'></div>text</div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest, RemoveTwoSpannersBeforeContent) {
  SetMulticolHTML(
      "<div id='mc'><div id='block'><div class='s'></div><div "
      "class='s'></div></div>text</div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cssc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest, RemoveSpannerAndContentAndSpanner) {
  SetMulticolHTML(
      "<div id='mc'><div id='block'><div class='s'></div>text<div "
      "class='s'></div>text</div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cscsc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "");
}

TEST_F(MultiColumnTreeModifyingTest,
       RemoveSpannerAndContentAndSpannerBeforeContent) {
  SetMulticolHTML(
      "<div id='mc'><div id='block'><div class='s'></div>text<div "
      "class='s'></div></div>text</div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cscsc");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest,
       RemoveSpannerAndContentAndSpannerAfterContent) {
  SetMulticolHTML(
      "<div id='mc'>text<div id='block'><div class='s'></div>text<div "
      "class='s'></div></div></div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "cscs");
  DestroyLayoutObject("block");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnTreeModifyingTest,
       RemoveInvalidSpannerInSpannerBetweenContent) {
  SetMulticolHTML(
      "<div id='mc'>text<div class='s'><div "
      "id='spanner'></div></div>text</div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
}

TEST_F(MultiColumnTreeModifyingTest,
       RemoveSpannerWithInvalidSpannerBetweenContent) {
  SetMulticolHTML(
      "<div id='mc'>text<div id='spanner'><div "
      "class='s'></div></div>text</div>");
  EXPECT_EQ(ColumnSetSignature("mc"), "csc");
  DestroyLayoutObject("spanner");
  EXPECT_EQ(ColumnSetSignature("mc"), "c");
}

TEST_F(MultiColumnRenderingTest, Continuation) {
  InsertStyleElement("#mc { column-count: 2}");
  SetBodyInnerHTML("<div id=mc><span>x<div id=inner></div>y</div>");
  auto& multicol = *GetElementById("mc");
  const auto& container = *To<LayoutBlockFlow>(multicol.GetLayoutObject());
  const auto& flow_thread = *container.MultiColumnFlowThread();

  ASSERT_TRUE(&flow_thread)
      << "We have flow thread even if container has no children.";

  // 1. Continuations should be in anonymous block in LayoutNG.
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutInline SPAN
  |  |  |  +--LayoutText #text "x"
  |  |  |  +--LayoutBlockFlow (anonymous)
  |  |  |  |  +--LayoutBlockFlow DIV id="inner"
  |  |  |  +--LayoutText #text "y"
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 2. Remove #inner to avoid continuation.
  GetElementById("inner")->remove();
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutInline SPAN
  |  |  |  +--LayoutText #text "x"
  |  |  |  +--LayoutText #text "y"
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 3. Normalize to merge "x" and "y".
  // See http://crbug.com/1201508 for redundant |LayoutInline SPAN|.
  multicol.normalize();
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutInline SPAN
  |  |  |  +--LayoutText #text "xy"
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));
}

TEST_F(MultiColumnRenderingTest, InsertBlock) {
  InsertStyleElement("#mc { column-count: 3}");
  SetBodyInnerHTML("<div id=mc></div>");

  auto& multicol = *GetElementById("mc");
  const auto& container = *To<LayoutBlockFlow>(multicol.GetLayoutObject());
  const auto& flow_thread = *container.MultiColumnFlowThread();

  ASSERT_TRUE(&flow_thread)
      << "We have flow thread even if container has no children.";
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 1. Add inline child
  multicol.appendChild(Text::Create(GetDocument(), "x"));
  RunDocumentLifecycle();
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutText #text "x"
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 2. Remove inline child
  multicol.removeChild(multicol.firstChild());
  RunDocumentLifecycle();

  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 3. Insert block
  multicol.insertBefore(MakeGarbageCollected<HTMLDivElement>(GetDocument()),
                        multicol.lastChild());
  RunDocumentLifecycle();
  EXPECT_FALSE(flow_thread.ChildrenInline());

  EXPECT_EQ(
      R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow DIV
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
      ToSimpleLayoutTree(container));
}

TEST_F(MultiColumnRenderingTest, InsertInline) {
  InsertStyleElement("#mc { column-count: 3}");
  SetBodyInnerHTML("<div id=mc></div>");

  auto& multicol = *GetElementById("mc");
  const auto& container = *To<LayoutBlockFlow>(multicol.GetLayoutObject());
  const auto& flow_thread = *container.MultiColumnFlowThread();

  ASSERT_TRUE(&flow_thread)
      << "We have flow thread even if container has no children.";
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 1. Add inline child
  multicol.appendChild(Text::Create(GetDocument(), "x"));
  RunDocumentLifecycle();
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutText #text "x"
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 2. Remove inline child
  multicol.removeChild(multicol.firstChild());
  RunDocumentLifecycle();

  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 3. Insert inline
  multicol.insertBefore(MakeGarbageCollected<HTMLSpanElement>(GetDocument()),
                        multicol.lastChild());
  RunDocumentLifecycle();
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutInline SPAN
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));
}

TEST_F(MultiColumnRenderingTest, ListItem) {
  InsertStyleElement("#mc { column-count: 3; display: list-item; }");
  SetBodyInnerHTML("<div id=mc></div>");

  auto& multicol = *GetElementById("mc");
  const auto& container = *To<LayoutBlockFlow>(multicol.GetLayoutObject());
  const auto& flow_thread = *container.MultiColumnFlowThread();

  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutListItem DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutOutsideListMarker ::marker
  |  |  +--LayoutTextFragment (anonymous) ("\u2022 ")
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));
}

TEST_F(MultiColumnRenderingTest, SplitInline) {
  InsertStyleElement("#mc { column-count: 3}");
  SetBodyInnerHTML("<div id=mc></div>");

  auto& multicol = *GetElementById("mc");
  const auto& container = *To<LayoutBlockFlow>(multicol.GetLayoutObject());
  const auto& flow_thread = *container.MultiColumnFlowThread();

  ASSERT_TRUE(&flow_thread)
      << "We have flow thread even if container has no children.";
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 1. Add inline child
  multicol.appendChild(Text::Create(GetDocument(), "x"));
  RunDocumentLifecycle();

  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutText #text "x"
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 2. Remove inline child
  multicol.removeChild(multicol.firstChild());
  RunDocumentLifecycle();
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 3. Add inline child again
  multicol.appendChild(Text::Create(GetDocument(), "x"));
  RunDocumentLifecycle();
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutText #text "x"
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 4. Add inline child (one more)
  multicol.appendChild(Text::Create(GetDocument(), "y"));
  RunDocumentLifecycle();
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutText #text "x"
  |  |  +--LayoutText #text "y"
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));

  // 5. Add a block child to split inline children.
  multicol.insertBefore(MakeGarbageCollected<HTMLDivElement>(GetDocument()),
                        multicol.lastChild());
  RunDocumentLifecycle();
  EXPECT_FALSE(flow_thread.ChildrenInline());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="mc"
  +--LayoutMultiColumnFlowThread (anonymous)
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutText #text "x"
  |  +--LayoutBlockFlow DIV
  |  +--LayoutBlockFlow (anonymous)
  |  |  +--LayoutText #text "y"
  +--LayoutMultiColumnSet (anonymous)
)DUMP",
            ToSimpleLayoutTree(container));
}

TEST_F(MultiColumnRenderingTest, FlowThreadUpdateGeometryCrash) {
  SetBodyInnerHTML(R"HTML(
      <video width="64" height="64" controls>
      <iframe width=320 height=320></iframe>)HTML");
  UpdateAllLifecyclePhasesForTest();
  InsertStyleElement(R"CSS(
      body, html {
        column-count: 2;
        overflow: clip;
      })CSS");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crash in LayoutMultiColumnFlowThread::UpdateGeometry() call
  // from LayoutMedia::ComputePanelWidth().
}

}  // anonymous namespace

}  // namespace blink
