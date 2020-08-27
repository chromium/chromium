// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CSSPropertyTest : public PageTestBase {
 public:
  const CSSValue* Parse(String name, String value) {
    auto* set = css_test_helpers::ParseDeclarationBlock(name + ":" + value);
    DCHECK(set);
    if (set->PropertyCount() != 1)
      return nullptr;
    return &set->PropertyAt(0).Value();
  }

  scoped_refptr<ComputedStyle> ComputedStyleWithValue(
      const CSSProperty& property,
      const CSSValue& value) {
    StyleResolverState state(GetDocument(), *GetDocument().body());
    state.SetStyle(ComputedStyle::Create());

    // The border-style needs to be non-hidden and non-none, otherwise
    // the computed values of border-width properties are always zero.
    //
    // https://drafts.csswg.org/css-backgrounds-3/#the-border-width
    state.Style()->SetBorderBottomStyle(EBorderStyle::kSolid);
    state.Style()->SetBorderLeftStyle(EBorderStyle::kSolid);
    state.Style()->SetBorderRightStyle(EBorderStyle::kSolid);
    state.Style()->SetBorderTopStyle(EBorderStyle::kSolid);

    StyleBuilder::ApplyProperty(property, state, value);
    return state.TakeStyle();
  }
};

TEST_F(CSSPropertyTest, VisitedPropertiesAreNotWebExposed) {
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    EXPECT_TRUE(!property.IsVisited() ||
                !property.IsWebExposed(GetDocument().GetExecutionContext()));
  }
}

TEST_F(CSSPropertyTest, GetVisitedPropertyOnlyReturnsVisitedProperties) {
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    const CSSProperty* visited = property.GetVisitedProperty();
    EXPECT_TRUE(!visited || visited->IsVisited());
  }
}

TEST_F(CSSPropertyTest, GetUnvisitedPropertyFromVisited) {
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    EXPECT_EQ(property.IsVisited(),
              static_cast<bool>(property.GetUnvisitedProperty()));
  }
}

TEST_F(CSSPropertyTest, InternalResetEffectiveNotWebExposed) {
  const CSSPropertyValueSet* ua_set = css_test_helpers::ParseDeclarationBlock(
      "zoom:-internal-reset-effective", kUASheetMode);
  const CSSPropertyValueSet* author_set =
      css_test_helpers::ParseDeclarationBlock("zoom:-internal-reset-effective",
                                              kHTMLStandardMode);

  EXPECT_TRUE(ua_set->HasProperty(CSSPropertyID::kZoom));
  EXPECT_FALSE(author_set->HasProperty(CSSPropertyID::kZoom));
}

TEST_F(CSSPropertyTest, InternalFontSizeDeltaNotWebExposed) {
  ASSERT_FALSE(
      CSSProperty::Get(CSSPropertyID::kInternalFontSizeDelta).IsWebExposed());
}

TEST_F(CSSPropertyTest, VisitedPropertiesCanParseValues) {
  scoped_refptr<ComputedStyle> initial_style = ComputedStyle::Create();

  // Count the number of 'visited' properties seen.
  size_t num_visited = 0;

  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    const CSSProperty* visited = property.GetVisitedProperty();
    if (!visited)
      continue;

    // Get any value compatible with 'property'. The initial value will do.
    const CSSValue* initial_value = property.CSSValueFromComputedStyle(
        *initial_style, nullptr /* layout_object */,
        false /* allow_visited_style */);
    ASSERT_TRUE(initial_value);
    String css_text = initial_value->CssText();

    // Parse the initial value using both the regular property, and the
    // accompanying 'visited' property.
    const CSSValue* parsed_regular_value = css_test_helpers::ParseLonghand(
        GetDocument(), property, initial_value->CssText());
    const CSSValue* parsed_visited_value = css_test_helpers::ParseLonghand(
        GetDocument(), *visited, initial_value->CssText());

    // The properties should have identical parsing behavior.
    EXPECT_TRUE(DataEquivalent(parsed_regular_value, parsed_visited_value));

    num_visited++;
  }

  // Verify that we have seen at least one visited property. If we didn't (and
  // there is no bug), it means this test can be removed.
  EXPECT_GT(num_visited, 0u);
}

TEST_F(CSSPropertyTest, Surrogates) {
  EXPECT_EQ(&GetCSSPropertyWidth(),
            GetCSSPropertyInlineSize().SurrogateFor(
                TextDirection::kLtr, WritingMode::kHorizontalTb));
  EXPECT_EQ(&GetCSSPropertyHeight(),
            GetCSSPropertyInlineSize().SurrogateFor(TextDirection::kLtr,
                                                    WritingMode::kVerticalRl));
  EXPECT_EQ(&GetCSSPropertyWritingMode(),
            GetCSSPropertyWebkitWritingMode().SurrogateFor(
                TextDirection::kLtr, WritingMode::kHorizontalTb));
  EXPECT_FALSE(GetCSSPropertyWidth().SurrogateFor(TextDirection::kLtr,
                                                  WritingMode::kHorizontalTb));
}

TEST_F(CSSPropertyTest, ComputedValuesEqualsSelf) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  for (CSSPropertyID id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(id);
    if (!property.IsComputedValueComparable())
      continue;
    EXPECT_TRUE(property.ComputedValuesEqual(*style, *style));
  }
}

namespace {

// Examples must produce unique computed values. For example, it's not
// allowed to list both 2px and calc(1px + 1px).
const char* align_content_examples[] = {"normal", "first baseline", "stretch",
                                        "safe end", nullptr};
const char* border_style_examples[] = {"none", "solid", "dashed", nullptr};
const char* color_examples[] = {"red", "green", "#fef", "#faf", nullptr};
const char* direction_examples[] = {"ltr", "rtl", nullptr};
const char* flex_direction_examples[] = {"row", "column", nullptr};
const char* flex_wrap_examples[] = {"nowrap", "wrap", nullptr};
const char* float_examples[] = {"1", "2.5", nullptr};
const char* justify_content_examples[] = {"normal", "stretch", "safe end",
                                          "left", nullptr};
const char* length_or_auto_examples[] = {"auto", "1px", "2px", "5%", nullptr};
const char* length_or_none_examples[] = {"none", "1px", "2px", "5%", nullptr};
const char* length_percentage_examples[] = {"1px", "2%", "calc(1% + 2px)",
                                            nullptr};
const char* length_size_examples[] = {"4px", "1px 2px", "3%", "calc(1% + 1px)",
                                      nullptr};
const char* line_width_examples[] = {"medium", "thin", "100px", nullptr};
const char* none_auto_examples[] = {"none", "auto", nullptr};
const char* self_align_examples[] = {"flex-start", "flex-end", "first baseline",
                                     "safe end", nullptr};
const char* text_decoration_line_examples[] = {"none", "underline", nullptr};
const char* text_decoration_style_examples[] = {"solid", "dashed", nullptr};
const char* vertical_align_examples[] = {"sub", "super", "1px", "3%", nullptr};
const char* writing_mode_examples[] = {"horizontal-tb", "vertical-rl", nullptr};

struct ComputedValuesEqualData {
  const char* name;
  const char** examples;
} computed_values_equal_data[] = {
    {"-webkit-writing-mode", writing_mode_examples},
    {"align-content", align_content_examples},
    {"align-items", self_align_examples},
    {"align-self", self_align_examples},
    {"border-bottom-color", color_examples},
    {"border-bottom-left-radius", length_size_examples},
    {"border-bottom-right-radius", length_size_examples},
    {"border-bottom-style", border_style_examples},
    {"border-bottom-width", line_width_examples},
    {"border-left-color", color_examples},
    {"border-left-style", border_style_examples},
    {"border-left-width", line_width_examples},
    {"border-right-color", color_examples},
    {"border-right-style", border_style_examples},
    {"border-right-width", line_width_examples},
    {"border-top-color", color_examples},
    {"border-top-left-radius", length_size_examples},
    {"border-top-right-radius", length_size_examples},
    {"border-top-style", border_style_examples},
    {"border-top-width", line_width_examples},
    {"bottom", length_or_auto_examples},
    {"direction", direction_examples},
    {"flex-basis", length_or_auto_examples},
    {"flex-direction", flex_direction_examples},
    {"flex-grow", float_examples},
    {"flex-shrink", float_examples},
    {"flex-wrap", flex_wrap_examples},
    {"height", length_or_auto_examples},
    {"justify-content", justify_content_examples},
    {"justify-items", self_align_examples},
    {"justify-self", self_align_examples},
    {"left", length_or_auto_examples},
    {"margin-bottom", length_or_auto_examples},
    {"margin-left", length_or_auto_examples},
    {"margin-right", length_or_auto_examples},
    {"margin-top", length_or_auto_examples},
    {"max-height", length_or_none_examples},
    {"max-width", length_or_none_examples},
    {"min-height", length_or_auto_examples},
    {"min-width", length_or_auto_examples},
    {"padding-bottom", length_percentage_examples},
    {"padding-left", length_percentage_examples},
    {"padding-right", length_percentage_examples},
    {"padding-top", length_percentage_examples},
    {"right", length_or_auto_examples},
    {"text-decoration-color", color_examples},
    {"text-decoration-line", text_decoration_line_examples},
    {"text-decoration-skip-ink", none_auto_examples},
    {"text-decoration-style", text_decoration_style_examples},
    {"text-decoration-thickness", length_or_auto_examples},
    {"top", length_or_auto_examples},
    {"vertical-align", vertical_align_examples},
    {"width", length_or_auto_examples},
    {"writing-mode", writing_mode_examples},
};

}  // namespace

TEST_F(CSSPropertyTest, ComparablePropertiesAreListed) {
  HashSet<String> names;
  for (const auto& data : computed_values_equal_data)
    names.insert(data.name);

  for (CSSPropertyID id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(id);
    EXPECT_TRUE(!property.IsComputedValueComparable() ||
                names.Contains(property.GetPropertyNameString()))
        << property.GetPropertyNameString() << " missing";
  }
}

// This test verifies the correctness of CSSProperty::ComputedValuesEqual for
// all properties that have the kComputedValueComparable flag.
class ComputedValuesEqual
    : public CSSPropertyTest,
      public testing::WithParamInterface<ComputedValuesEqualData> {};

INSTANTIATE_TEST_SUITE_P(CSSPropertyTest,
                         ComputedValuesEqual,
                         testing::ValuesIn(computed_values_equal_data));

TEST_P(ComputedValuesEqual, Examples) {
  auto data = GetParam();

  CSSPropertyRef ref(data.name, GetDocument());
  ASSERT_TRUE(ref.IsValid()) << data.name;
  const CSSProperty& property = ref.GetProperty();
  ASSERT_TRUE(property.IsComputedValueComparable()) << data.name;

  // Convert const char* examples to CSSValues.
  HeapVector<Member<const CSSValue>> values;
  for (const char** example = data.examples; *example; ++example) {
    const CSSValue* value = Parse(data.name, *example);
    ASSERT_TRUE(value) << data.name << ":" << *example;
    values.push_back(value);
  }

  for (const CSSValue* value_a : values) {
    for (const CSSValue* value_b : values) {
      auto style_a = ComputedStyleWithValue(property, *value_a);
      auto style_b = ComputedStyleWithValue(property, *value_b);
      if (value_a == value_b) {
        EXPECT_TRUE(property.ComputedValuesEqual(*style_a, *style_b))
            << property.GetPropertyNameString()
            << ": expected equality between " << value_a->CssText() << " and "
            << value_b->CssText();
      } else {
        EXPECT_FALSE(property.ComputedValuesEqual(*style_a, *style_b))
            << property.GetPropertyNameString()
            << ": expected non-equality between " << value_a->CssText()
            << " and " << value_b->CssText();
      }
    }
  }
}

}  // namespace blink
