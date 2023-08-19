// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/editing_style.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"

namespace blink {

class EditingStyleTest : public EditingTestBase {};

TEST_F(EditingStyleTest, mergeInlineStyleOfElement) {
  SetBodyContent(
      "<span id=s1 style='--A:var(---B)'>1</span>"
      "<span id=s2 style='float:var(--C)'>2</span>");
  UpdateAllLifecyclePhasesForTest();

  EditingStyle* editing_style = MakeGarbageCollected<EditingStyle>(
      To<HTMLElement>(GetDocument().getElementById(AtomicString("s2"))));
  editing_style->MergeInlineStyleOfElement(
      To<HTMLElement>(GetDocument().getElementById(AtomicString("s1"))),
      EditingStyle::kOverrideValues);

  EXPECT_FALSE(editing_style->Style()->HasProperty(CSSPropertyID::kFloat))
      << "Don't merge a property with unresolved value";
  EXPECT_EQ("var(---B)",
            editing_style->Style()->GetPropertyValue(AtomicString("--A")))
      << "Keep unresolved value on merging style";
}

// http://crbug.com/957952
TEST_F(EditingStyleTest, RemoveStyleFromRulesAndContext_TextAlignEffective) {
  // Note: <div>'s "text-align" is "start".
  // For <p> with "text-align:start", it equivalents to "text-align:right"
  SetBodyContent("<div><p dir=rtl id=target>");
  Element& target = *GetElementById("target");
  EditingStyle& style = *MakeGarbageCollected<EditingStyle>(
      CSSPropertyID::kTextAlign, "left", SecureContextMode::kInsecureContext);
  style.RemoveStyleFromRulesAndContext(&target, target.parentElement());

  EXPECT_EQ(CSSValueID::kLeft, style.GetProperty(CSSPropertyID::kTextAlign));
}

// http://crbug.com/957952
TEST_F(EditingStyleTest, RemoveStyleFromRulesAndContext_TextAlignRedundant) {
  // Note: <div>'s "text-align" is "start".
  // For <p> with "text-align:start", it equivalents to "text-align:right"
  SetBodyContent("<div><p dir=rtl id=target>");
  Element& target = *GetElementById("target");
  EditingStyle& style = *MakeGarbageCollected<EditingStyle>(
      CSSPropertyID::kTextAlign, "right", SecureContextMode::kInsecureContext);
  style.RemoveStyleFromRulesAndContext(&target, target.parentElement());

  EXPECT_EQ(CSSValueID::kInvalid, style.GetProperty(CSSPropertyID::kTextAlign));
}

}  // namespace blink
