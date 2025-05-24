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

TEST_F(EditingStyleTest, MergeStyleFromRulesForSerialization) {
  const char* html = R"HTML(
    <style>
      :root {
            --font-size-header: 18px;
            --cell-padding: 12px;
            --font-family-header: Calibri, sans-serif;
            --header-bg-color: unset;
        }
        th {
            background-color: var(--header-bg-color);
            color: var(--header-text-color);
            font-size: var(--font-size-header);
            padding: var(--cell-padding);
            text-align: left;
            font-family: var(--font-family-header);
            --custom-prop: /*Tests a possible crash*/ var(--undefined-value);
        }
    </style>
    <table id=tableid>
        <thead>
            <tr>
                <th id=headid>Column 1</th>
                <th>Column 2</th>
            </tr>
        </thead>
    </table>
  )HTML";

  SetBodyContent(html);
  UpdateAllLifecyclePhasesForTest();

  auto* element = To<HTMLElement>(GetElementById("headid"));
  EditingStyle* editing_style = MakeGarbageCollected<EditingStyle>(element);
  editing_style->MergeStyleFromRulesForSerialization(element);

  auto* style = editing_style->Style();
  auto expected_property_count = 0;

  struct ExpectedProperty {
    const String name;
    const String expected_value;
  };

  const ExpectedProperty expected_properties[] = {
      {"background-color", "rgba(0, 0, 0, 0)"},
      {"font-size", "18px"},
      {"font-family", "Calibri, sans-serif"},
      {"text-align", "left"},
      {"padding-top", "12px"},
      {"padding-bottom", "12px"},
      {"padding-left", "12px"},
      {"padding-right", "12px"}};

  for (const auto& property : style->Properties()) {
    auto name = property.Name().ToAtomicString();
    const auto& value = property.Value().CssText();

    for (const auto& expected : expected_properties) {
      if (name == expected.name) {
        expected_property_count++;
        EXPECT_EQ(expected.expected_value, value);
      }
    }
  }

  EXPECT_EQ(std::size(expected_properties), expected_property_count);
}

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
