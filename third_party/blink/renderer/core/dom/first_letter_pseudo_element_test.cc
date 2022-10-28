// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"

#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
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

TEST_F(FirstLetterPseudoElementTest, InitialLetter) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;
  ScopedCSSInitialLetterForTest enable_initial_letter_scope(true);
  LoadAhem();
  InsertStyleElement(
      "p { font: 20px/24px Ahem; }"
      "p::first-letter { initial-letter: 3; line-height: 200px; }");
  SetBodyContent("<p id=sample>This paragraph has an initial letter.</p>");
  auto& sample = *GetElementById("sample");
  const auto& initial_letter_box =
      *sample.GetPseudoElement(kPseudoIdFirstLetter)->GetLayoutObject();
  const auto& initial_letter_text1 =
      *To<LayoutTextFragment>(initial_letter_box.SlowFirstChild());

  EXPECT_TRUE(initial_letter_box.IsInitialLetterBox());
  EXPECT_EQ(3.0f, initial_letter_box.StyleRef().InitialLetter().Size());
  EXPECT_EQ(3, initial_letter_box.StyleRef().InitialLetter().Sink());

  EXPECT_EQ(sample.GetLayoutObject()->StyleRef().GetFont(),
            initial_letter_box.StyleRef().GetFont())
      << "initial letter box should have a specified font.";

  const auto& initial_letter_text_style1 = initial_letter_text1.StyleRef();
  EXPECT_EQ(EVerticalAlign::kBaseline,
            initial_letter_text_style1.VerticalAlign());
  EXPECT_EQ(LayoutUnit(80),
            initial_letter_text_style1.ComputedLineHeightAsFixed());
  EXPECT_EQ(FontHeight(LayoutUnit(64), LayoutUnit(16)),
            initial_letter_text_style1.GetFontHeight())
      << "initial letter box should have a cap font.";

  // Changing paragraph style should be distributed to initial letter text.
  sample.style()->setProperty(GetDocument().GetExecutionContext(), "font-size",
                              "30px", String(), ASSERT_NO_EXCEPTION);
  sample.style()->setProperty(GetDocument().GetExecutionContext(),
                              "line-height", "34px", String(),
                              ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  const auto& initial_letter_text2 =
      *To<LayoutTextFragment>(initial_letter_box.SlowFirstChild());
  EXPECT_EQ(&initial_letter_text2, &initial_letter_text1)
      << "font-size and line-height changes don't build new first-letter tree.";

  const auto& initial_letter_text_style2 = initial_letter_text2.StyleRef();
  EXPECT_EQ(EVerticalAlign::kBaseline,
            initial_letter_text_style2.VerticalAlign());
  EXPECT_EQ(LayoutUnit(115),
            initial_letter_text_style2.ComputedLineHeightAsFixed());
  EXPECT_EQ(FontHeight(LayoutUnit(92), LayoutUnit(23)),
            initial_letter_text_style2.GetFontHeight())
      << "initial letter box should have a cap font.";
}

TEST_F(FirstLetterPseudoElementTest, UnicodePairBreaking) {
  const UChar test_string[] = {0xD800, 0xDD00, 'A', 0xD800, 0xDD00,
                               0xD800, 0xDD00, 'B', 0};
  EXPECT_EQ(7u, FirstLetterPseudoElement::FirstLetterLength(test_string));
}

}  // namespace blink
