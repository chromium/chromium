// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class LayoutTreeBuilderTraversalTest : public RenderingTest {
 protected:
  void SetupSampleHTML(const char* main_html);
};

void LayoutTreeBuilderTraversalTest::SetupSampleHTML(const char* main_html) {
  SetBodyInnerHTML(String::FromUTF8(main_html));
}

TEST_F(LayoutTreeBuilderTraversalTest, emptySubTree) {
  const char* const kHtml = "<div id='top'></div>";
  SetupSampleHTML(kHtml);

  Element* top = GetDocument().QuerySelector(AtomicString("#top"));
  Element* body = GetDocument().QuerySelector(AtomicString("body"));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::FirstChild(*top));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::NextSibling(*top));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::PreviousSibling(*top));
  EXPECT_EQ(body, LayoutTreeBuilderTraversal::Parent(*top));
}

TEST_F(LayoutTreeBuilderTraversalTest, pseudos) {
  const char* const kHtml =
      "<style>"
      "#top { display: list-item; }"
      "#top::marker { content: \"baz\"; }"
      "#top::before { content: \"foo\"; }"
      "#top::after { content: \"bar\"; }"
      "</style>"
      "<div id='top'></div>";
  SetupSampleHTML(kHtml);

  Element* top = GetDocument().QuerySelector(AtomicString("#top"));
  Element* marker = top->GetPseudoElement(kPseudoIdMarker);
  Element* before = top->GetPseudoElement(kPseudoIdBefore);
  Element* after = top->GetPseudoElement(kPseudoIdAfter);
  EXPECT_EQ(marker, LayoutTreeBuilderTraversal::Next(*top, nullptr));
  EXPECT_EQ(before, LayoutTreeBuilderTraversal::NextSibling(*marker));
  EXPECT_EQ(after, LayoutTreeBuilderTraversal::NextSibling(*before));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::NextSibling(*after));
  EXPECT_EQ(before, LayoutTreeBuilderTraversal::PreviousSibling(*after));
  EXPECT_EQ(marker, LayoutTreeBuilderTraversal::PreviousSibling(*before));
  EXPECT_EQ(nullptr, LayoutTreeBuilderTraversal::PreviousSibling(*marker));
  EXPECT_EQ(marker, LayoutTreeBuilderTraversal::FirstChild(*top));
  EXPECT_EQ(after, LayoutTreeBuilderTraversal::LastChild(*top));
}

TEST_F(LayoutTreeBuilderTraversalTest, emptyDisplayContents) {
  const char* const kHtml =
      "<div></div>"
      "<div style='display: contents'></div>"
      "<div id='last'></div>";
  SetupSampleHTML(kHtml);

  Element* first = GetDocument().QuerySelector(AtomicString("div"));
  Element* last = GetDocument().QuerySelector(AtomicString("#last"));

  EXPECT_TRUE(last->GetLayoutObject());
  EXPECT_EQ(last->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*first));
}

TEST_F(LayoutTreeBuilderTraversalTest, displayContentsChildren) {
  const char* const kHtml =
      "<div></div>"
      "<div id='contents' style='display: contents'><div "
      "id='inner'></div></div>"
      "<div id='last'></div>";
  SetupSampleHTML(kHtml);

  Element* first = GetDocument().QuerySelector(AtomicString("div"));
  Element* inner = GetDocument().QuerySelector(AtomicString("#inner"));
  Element* contents = GetDocument().QuerySelector(AtomicString("#contents"));
  Element* last = GetDocument().QuerySelector(AtomicString("#last"));

  EXPECT_TRUE(inner->GetLayoutObject());
  EXPECT_TRUE(last->GetLayoutObject());
  EXPECT_TRUE(first->GetLayoutObject());
  EXPECT_FALSE(contents->GetLayoutObject());

  EXPECT_EQ(inner->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*first));
  EXPECT_EQ(first->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*inner));

  EXPECT_EQ(last->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*inner));
  EXPECT_EQ(inner->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*last));
}

TEST_F(LayoutTreeBuilderTraversalTest, displayContentsChildrenNested) {
  const char* const kHtml =
      "<div></div>"
      "<div style='display: contents'>"
      "<div style='display: contents'>"
      "<div id='inner'></div>"
      "<div id='inner-sibling'></div>"
      "</div>"
      "</div>"
      "<div id='last'></div>";
  SetupSampleHTML(kHtml);

  Element* first = GetDocument().QuerySelector(AtomicString("div"));
  Element* inner = GetDocument().QuerySelector(AtomicString("#inner"));
  Element* sibling =
      GetDocument().QuerySelector(AtomicString("#inner-sibling"));
  Element* last = GetDocument().QuerySelector(AtomicString("#last"));

  EXPECT_TRUE(first->GetLayoutObject());
  EXPECT_TRUE(inner->GetLayoutObject());
  EXPECT_TRUE(sibling->GetLayoutObject());
  EXPECT_TRUE(last->GetLayoutObject());

  EXPECT_EQ(inner->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*first));
  EXPECT_EQ(first->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*inner));

  EXPECT_EQ(sibling->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*inner));
  EXPECT_EQ(inner->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*sibling));

  EXPECT_EQ(last->GetLayoutObject(),
            LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*sibling));
  EXPECT_EQ(sibling->GetLayoutObject(),
            LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(*last));
}

TEST_F(LayoutTreeBuilderTraversalTest, limits) {
  const char* const kHtml =
      "<div></div>"
      "<div style='display: contents'></div>"
      "<div style='display: contents'>"
      "<div style='display: contents'>"
      "</div>"
      "</div>"
      "<div id='shouldNotBeFound'></div>";

  SetupSampleHTML(kHtml);

  Element* first = GetDocument().QuerySelector(AtomicString("div"));

  EXPECT_TRUE(first->GetLayoutObject());
  LayoutObject* next_sibling =
      LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*first, 2);
  EXPECT_FALSE(next_sibling);  // Should not overrecurse
}

TEST_F(LayoutTreeBuilderTraversalTest, ColumnScrollMarkers) {
  SetupSampleHTML(R"(
      <style>
        #test {
          overflow: hidden;
          scroll-marker-group: before;
          columns: 1;
          height: 100px;
          width: 100px;
        }
        #test::scroll-marker-group {
          content: 'smg';
          display: flex;
          height: 100px;
          width: 100px;
        }
        #test::marker {
          content: 'm';
        }
        #test::column::scroll-marker {
          content: 'csm';
          height: 100px;
          width: 30px;
        }
        #test::before {
          content: 'b';
        }
        #test div {
          height: 100px;
          width: 100px;
        }
      </style>
      <li id='test'>
        <div></div>
        <div></div>
      </li>
      )");
  UpdateAllLifecyclePhasesForTest();

  Element* body = GetDocument().body();
  Element* test = body->QuerySelector(AtomicString("#test"));
  PseudoElement* before = test->GetPseudoElement(kPseudoIdBefore);
  PseudoElement* marker = test->GetPseudoElement(kPseudoIdMarker);
  PseudoElement* scroll_marker_group =
      test->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore);
  PseudoElement* first_column = test->GetColumnPseudoElements()->front();
  PseudoElement* first_column_scroll_marker =
      first_column->GetPseudoElement(kPseudoIdScrollMarker);
  PseudoElement* second_column = test->GetColumnPseudoElements()->at(1u);
  PseudoElement* second_column_scroll_marker =
      second_column->GetPseudoElement(kPseudoIdScrollMarker);
  PseudoElement* third_column = test->GetColumnPseudoElements()->back();
  PseudoElement* third_column_scroll_marker =
      third_column->GetPseudoElement(kPseudoIdScrollMarker);
  EXPECT_EQ(test->GetColumnPseudoElements()->size(), 3u);

  EXPECT_EQ(scroll_marker_group, LayoutTreeBuilderTraversal::FirstChild(*test));
  EXPECT_EQ(marker,
            LayoutTreeBuilderTraversal::Next(*scroll_marker_group, nullptr));
  EXPECT_EQ(first_column, LayoutTreeBuilderTraversal::Next(*marker, nullptr));
  EXPECT_EQ(first_column_scroll_marker,
            LayoutTreeBuilderTraversal::Next(*first_column, nullptr));
  EXPECT_EQ(second_column, LayoutTreeBuilderTraversal::Next(
                               *first_column_scroll_marker, nullptr));
  EXPECT_EQ(second_column_scroll_marker,
            LayoutTreeBuilderTraversal::Next(*second_column, nullptr));
  EXPECT_EQ(third_column, LayoutTreeBuilderTraversal::Next(
                              *second_column_scroll_marker, nullptr));
  EXPECT_EQ(third_column_scroll_marker,
            LayoutTreeBuilderTraversal::Next(*third_column, nullptr));
  EXPECT_EQ(before, LayoutTreeBuilderTraversal::Next(
                        *third_column_scroll_marker, nullptr));

  EXPECT_EQ(third_column_scroll_marker,
            LayoutTreeBuilderTraversal::Previous(*before, nullptr));
  EXPECT_EQ(third_column, LayoutTreeBuilderTraversal::Previous(
                              *third_column_scroll_marker, nullptr));
  EXPECT_EQ(second_column_scroll_marker,
            LayoutTreeBuilderTraversal::Previous(*third_column, nullptr));
  EXPECT_EQ(second_column, LayoutTreeBuilderTraversal::Previous(
                               *second_column_scroll_marker, nullptr));
  EXPECT_EQ(first_column_scroll_marker,
            LayoutTreeBuilderTraversal::Previous(*second_column, nullptr));
  EXPECT_EQ(first_column, LayoutTreeBuilderTraversal::Previous(
                              *first_column_scroll_marker, nullptr));
  EXPECT_EQ(marker,
            LayoutTreeBuilderTraversal::Previous(*first_column, nullptr));
  EXPECT_EQ(scroll_marker_group,
            LayoutTreeBuilderTraversal::Previous(*marker, nullptr));
}

}  // namespace blink
