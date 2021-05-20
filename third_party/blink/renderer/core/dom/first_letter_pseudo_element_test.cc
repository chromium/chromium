// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class FirstLetterPseudoElementTest : public PageTestBase {};

TEST_F(FirstLetterPseudoElementTest, DoesNotBreakEmoji) {
  const UChar emoji[] = {0xD83D, 0xDE31, 0};
  EXPECT_EQ(2u, FirstLetterPseudoElement::FirstLetterLength(emoji));
}

// http://crbug.com/1187834
TEST_F(FirstLetterPseudoElementTest, AppendDataToSpace) {
  InsertStyleElement("div::first-letter { color: red; }");
  SetBodyContent("<div><b id=sample> <!---->xyz</b></div>");
  const auto& sample = *GetElementById("sample");
  const auto& sample_layout_object = *sample.GetLayoutObject();
  auto& first_text = *To<Text>(sample.firstChild());

  EXPECT_EQ(R"DUMP(
LayoutInline B id="sample"
  +--LayoutText #text " "
  +--LayoutInline ::first-letter
  |  +--LayoutTextFragment (anonymous) ("x")
  +--LayoutTextFragment #text "xyz" ("yz")
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));

  // Change leading white space " " to " AB".
  first_text.appendData("AB");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(R"DUMP(
LayoutInline B id="sample"
  +--LayoutInline ::first-letter
  |  +--LayoutTextFragment (anonymous) (" A")
  +--LayoutTextFragment #text " AB" ("B")
  +--LayoutTextFragment #text "xyz" ("xyz")
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));
}

// http://crbug.com/1159762
TEST_F(FirstLetterPseudoElementTest, EmptySpanOnly) {
  InsertStyleElement("p::first-letter { color: red; }");
  SetBodyContent("<div><p id=sample><b></b></p>abc</div>");
  Element& sample = *GetElementById("sample");
  // Call Element::RebuildFirstLetterLayoutTree()
  sample.setAttribute(html_names::kContenteditableAttr, "true");
  const PseudoElement* const first_letter =
      sample.GetPseudoElement(kPseudoIdFirstLetter);
  // We should not have ::first-letter pseudo element because <p> has no text.
  // See |FirstLetterPseudoElement::FirstLetterTextLayoutObject()| should
  // return nullptr during rebuilding layout tree.
  EXPECT_FALSE(first_letter);
}

TEST_F(FirstLetterPseudoElementTest, UnicodePairBreaking) {
  const UChar test_string[] = {0xD800, 0xDD00, 'A', 0xD800, 0xDD00,
                               0xD800, 0xDD00, 'B', 0};
  EXPECT_EQ(7u, FirstLetterPseudoElement::FirstLetterLength(test_string));
}

}  // namespace blink
