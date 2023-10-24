// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"

namespace blink {
namespace test {

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutReplaced) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before <img id="replaced" alt="alt"> after.
      </p>)HTML");

  const AXObject* ax_replaced = GetAXObjectByElementId("replaced");
  ASSERT_NE(nullptr, ax_replaced);
  ASSERT_EQ(ax::mojom::Role::kImage, ax_replaced->RoleValue());
  ASSERT_EQ("alt", ax_replaced->ComputedName());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_replaced->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_replaced->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutInline) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before <a id="inline" href="#">link</a> after.
      </p>)HTML");

  const AXObject* ax_inline = GetAXObjectByElementId("inline");
  ASSERT_NE(nullptr, ax_inline);
  ASSERT_EQ(ax::mojom::Role::kLink, ax_inline->RoleValue());
  ASSERT_EQ("link", ax_inline->ComputedName());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_inline->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_inline->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest,
       TextOffsetInFormattingContextWithLayoutBlockFlowAtInlineLevel) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before
        <b id="block-flow" style="display: inline-block;">block flow</b>
        after.
      </p>)HTML");

  const AXObject* ax_block_flow = GetAXObjectByElementId("block-flow");
  ASSERT_NE(nullptr, ax_block_flow);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_block_flow->RoleValue());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_block_flow->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_block_flow->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest,
       TextOffsetInFormattingContextWithLayoutBlockFlowAtBlockLevel) {
  // OffsetMapping does not support block flow objects that are at
  // block-level, so we do not support them as well.
  SetBodyInnerHTML(R"HTML(
      <p>
        Before
        <b id="block-flow" style="display: block;">block flow</b>
        after.
      </p>)HTML");

  const AXObject* ax_block_flow = GetAXObjectByElementId("block-flow");
  ASSERT_NE(nullptr, ax_block_flow);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_block_flow->RoleValue());
  // Since block-level elements do not expose a count of the number of
  // characters from the beginning of their formatting context, we return the
  // same offset that was passed in.
  EXPECT_EQ(0, ax_block_flow->TextOffsetInFormattingContext(0));
  EXPECT_EQ(1, ax_block_flow->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutText) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before <span id="span">text</span> after.
      </p>)HTML");

  const AXObject* ax_text =
      GetAXObjectByElementId("span")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());
  ASSERT_EQ("text", ax_text->ComputedName());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_text->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_text->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutBr) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before <br id="br"> after.
      </p>)HTML");

  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, ax_br->RoleValue());
  ASSERT_EQ("\n", ax_br->ComputedName());
  // After white space is compressed, the word "before" is of length 6.
  EXPECT_EQ(6, ax_br->TextOffsetInFormattingContext(0));
  EXPECT_EQ(7, ax_br->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutFirstLetter) {
  SetBodyInnerHTML(R"HTML(
      <style>
        q::first-letter {
          color: red;
        }
      </style>
      <p>
        Before
        <q id="first-letter">1. Remaining part</q>
        after.
      </p>)HTML");

  const AXObject* ax_first_letter = GetAXObjectByElementId("first-letter");
  ASSERT_NE(nullptr, ax_first_letter);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_first_letter->RoleValue());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_first_letter->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_first_letter->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest,
       TextOffsetInFormattingContextWithCSSGeneratedContent) {
  SetBodyInnerHTML(R"HTML(
      <style>
        q::before {
          content: "<";
          color: blue;
        }
        q::after {
          content: ">";
          color: red;
        }
      </style>
      <p>
        Before <q id="css-generated">CSS generated</q> after.
      </p>)HTML");

  const AXObject* ax_css_generated = GetAXObjectByElementId("css-generated");
  ASSERT_NE(nullptr, ax_css_generated);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_css_generated->RoleValue());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_css_generated->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_css_generated->TextOffsetInFormattingContext(1));
}

}  // namespace test
}  // namespace blink
