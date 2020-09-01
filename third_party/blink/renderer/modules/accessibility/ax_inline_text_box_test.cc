// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace test {

TEST_P(ParameterizedAccessibilityTest, GetWordBoundaries) {
  // &#9728; is the sun emoji symbol.
  // &#2460; is circled digit one.
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph">
        &quot;This, &#9728; &#2460; is ... a---+++test.&quot;
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());

  VectorOf<int> expected_word_starts{0, 1, 5, 9, 11, 14, 18, 19, 25, 29};
  VectorOf<int> expected_word_ends{1, 5, 6, 10, 13, 17, 19, 22, 29, 31};
  VectorOf<int> word_starts, word_ends;
  ax_inline_text_box->GetWordBoundaries(word_starts, word_ends);
  EXPECT_EQ(expected_word_starts, word_starts);
  EXPECT_EQ(expected_word_ends, word_ends);
}

TEST_P(ParameterizedAccessibilityTest, TextOffsetInContainerWithASpan) {
  // There should be three inline text boxes in the following paragraph. The
  // span should reset the text start offset of all of them to 0.
  SetBodyInnerHTML(R"HTML(
      <style>* { font-size: 10px; }</style>
      <p id="paragraph">
        Hello <span>world </span>there.
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(1, ax_inline_text_box->TextOffsetInContainer(1));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(2, ax_inline_text_box->TextOffsetInContainer(2));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(3, ax_inline_text_box->TextOffsetInContainer(3));

  ASSERT_EQ(nullptr, ax_inline_text_box->NextInPreOrderIncludingIgnored());
}

TEST_P(ParameterizedAccessibilityTest,
       TextOffsetInContainerWithMultipleInlineTextBoxes) {
  // There should be four inline text boxes in the following paragraph. The span
  // should not affect the text start offset of the text outside the span.
  SetBodyInnerHTML(R"HTML(
      <style>* { font-size: 10px; }</style>
      <p id="paragraph" style="width: 5ch;">
        <span>Offset</span>Hello world there.
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(1, ax_inline_text_box->TextOffsetInContainer(1));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(1, ax_inline_text_box->TextOffsetInContainer(1));

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(6, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(8, ax_inline_text_box->TextOffsetInContainer(2));

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(12, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(15, ax_inline_text_box->TextOffsetInContainer(3));

  ASSERT_EQ(nullptr, ax_inline_text_box->NextInPreOrderIncludingIgnored());
}

TEST_P(ParameterizedAccessibilityTest, TextOffsetInContainerWithLineBreak) {
  // There should be three inline text boxes in the following paragraph. The
  // line break should reset the text start offset to 0 of both the inline text
  // box inside the line break, as well as the text start ofset of the second
  // line.
  SetBodyInnerHTML(R"HTML(
      <style>* { font-size: 10px; }</style>
      <p id="paragraph">
        Line one.<br>
        Line two.
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(1, ax_inline_text_box->TextOffsetInContainer(1));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(2, ax_inline_text_box->TextOffsetInContainer(2));

  ASSERT_EQ(nullptr, ax_inline_text_box->NextInPreOrderIncludingIgnored());
}

TEST_P(ParameterizedAccessibilityTest, TextOffsetInContainerWithBreakWord) {
  // There should be three inline text boxes in the following paragraph because
  // of the narrow width and the long word, coupled with the CSS "break-word"
  // property. Each inline text box should have a different offset in container.
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
      <style>* { font: 10px/10px Ahem; }</style>
      <p id="paragraph" style="width: 5ch; word-wrap: break-word;">
        VeryLongWord
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());

  int text_start_offset = 0;
  int text_end_offset = ax_inline_text_box->TextLength();
  EXPECT_EQ(text_start_offset, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(text_end_offset, ax_inline_text_box->TextOffsetInContainer(
                                 ax_inline_text_box->TextLength()));

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());

  text_start_offset = text_end_offset;
  text_end_offset = text_start_offset + ax_inline_text_box->TextLength();
  EXPECT_EQ(text_start_offset, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(text_end_offset, ax_inline_text_box->TextOffsetInContainer(
                                 ax_inline_text_box->TextLength()));

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());

  text_start_offset = text_end_offset;
  text_end_offset = text_start_offset + ax_inline_text_box->TextLength();
  EXPECT_EQ(text_start_offset, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(text_end_offset, ax_inline_text_box->TextOffsetInContainer(
                                 ax_inline_text_box->TextLength()));

  ASSERT_EQ(nullptr, ax_inline_text_box->NextSiblingIncludingIgnored());
}

}  // namespace test
}  // namespace blink
