// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_label_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLLabelElementTest : public PageTestBase {};

TEST_F(HTMLLabelElementTest, TextContentExcludingLabelable_Simple) {
  SetBodyInnerHTML("<label id=test>Name:</label>");
  auto* label =
      To<HTMLLabelElement>(GetDocument().getElementById(AtomicString("test")));
  EXPECT_EQ("Name:", label->TextContentExcludingLabelable());
}

TEST_F(HTMLLabelElementTest, TextContentExcludingLabelable_ExcludesOptions) {
  SetBodyInnerHTML(R"HTML(
    <label id=test>Choose: <select><option>Option 1</option></select></label>
    )HTML");
  auto* label =
      To<HTMLLabelElement>(GetDocument().getElementById(AtomicString("test")));
  EXPECT_EQ("Choose: ", label->TextContentExcludingLabelable());
}

TEST_F(HTMLLabelElementTest, TextContentExcludingLabelable_KeepsNonLabelable) {
  SetBodyInnerHTML(R"HTML(
    <label id=test>Prefix <span>Span Text</span> Suffix</label>
    )HTML");
  auto* label =
      To<HTMLLabelElement>(GetDocument().getElementById(AtomicString("test")));
  EXPECT_EQ("Prefix Span Text Suffix", label->TextContentExcludingLabelable());
}

TEST_F(HTMLLabelElementTest, TextContentExcludingLabelable_MultipleElements) {
  SetBodyInnerHTML(R"HTML(
      <label id=test>
        Text1
        <input>
        Text2
        <span>Text3</span>
        <button>Button Text</button>
        Text4
      </label>
    )HTML");
  auto* label =
      To<HTMLLabelElement>(GetDocument().getElementById(AtomicString("test")));
  EXPECT_EQ("Text1 Text2 Text3 Text4",
            label->TextContentExcludingLabelable().SimplifyWhiteSpace());
}

TEST_F(HTMLLabelElementTest, TextContentExcludingLabelable_Empty) {
  SetBodyInnerHTML(R"HTML(<label id=test></label>)HTML");
  auto* label =
      To<HTMLLabelElement>(GetDocument().getElementById(AtomicString("test")));
  EXPECT_EQ("", label->TextContentExcludingLabelable());
}

TEST_F(HTMLLabelElementTest, TextContentExcludingLabelable_OnlyLabelable) {
  SetBodyInnerHTML(R"HTML(
    <label id=test>
      <input>
      <select><option>X</option></select>
      <button>B</button>
    </label>
    )HTML");
  auto* label =
      To<HTMLLabelElement>(GetDocument().getElementById(AtomicString("test")));
  EXPECT_EQ("", label->TextContentExcludingLabelable().StripWhiteSpace());
}

}  // namespace blink
