// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "base/memory/values_equivalent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/scoped_css_value.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
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
    state.SetStyle(GetDocument().GetStyleResolver().CreateComputedStyle());

    // The border-style needs to be non-hidden and non-none, otherwise
    // the computed values of border-width properties are always zero.
    //
    // https://drafts.csswg.org/css-backgrounds-3/#the-border-width
    state.StyleBuilder().SetBorderBottomStyle(EBorderStyle::kSolid);
    state.StyleBuilder().SetBorderLeftStyle(EBorderStyle::kSolid);
    state.StyleBuilder().SetBorderRightStyle(EBorderStyle::kSolid);
    state.StyleBuilder().SetBorderTopStyle(EBorderStyle::kSolid);

    StyleBuilder::ApplyProperty(property, state,
                                ScopedCSSValue(value, &GetDocument()));
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

TEST_F(CSSPropertyTest, InternalFontSizeDeltaNotWebExposed) {
  ASSERT_FALSE(
      CSSProperty::Get(CSSPropertyID::kInternalFontSizeDelta).IsWebExposed());
}

TEST_F(CSSPropertyTest, VisitedPropertiesCanParseValues) {
  scoped_refptr<ComputedStyle> initial_style =
      GetDocument().GetStyleResolver().CreateComputedStyle();

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
    EXPECT_TRUE(
        base::ValuesEquivalent(parsed_regular_value, parsed_visited_value));

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

TEST_F(CSSPropertyTest, PairsWithIdenticalValues) {
  const CSSValue* border_radius = css_test_helpers::ParseLonghand(
      GetDocument(), GetCSSPropertyBorderTopLeftRadius(), "1% 1%");
  const CSSValue* perspective_origin = css_test_helpers::ParseLonghand(
      GetDocument(), GetCSSPropertyPerspectiveOrigin(), "1% 1%");

  // Border radius drops identical values
  EXPECT_EQ("1%", border_radius->CssText());
  // Perspective origin keeps identical values
  EXPECT_EQ("1% 1%", perspective_origin->CssText());
  // Therefore, the values are different
  EXPECT_NE(*border_radius, *perspective_origin);
}

TEST_F(CSSPropertyTest, StaticVariableInstanceFlags) {
  EXPECT_FALSE(GetCSSPropertyVariable().IsShorthand());
  EXPECT_FALSE(GetCSSPropertyVariable().IsRepeated());
}

}  // namespace blink
