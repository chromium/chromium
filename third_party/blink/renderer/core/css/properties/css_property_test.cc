// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include <cstring>
#include "base/memory/values_equivalent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CSSPropertyTest : public PageTestBase {
 public:
  const CSSValue* Parse(String name, String value) {
    auto* set = css_test_helpers::ParseDeclarationBlock(name + ":" + value);
    DCHECK(set);
    if (set->PropertyCount() != 1) {
      return nullptr;
    }
    return &set->PropertyAt(0).Value();
  }

  scoped_refptr<const ComputedStyle> ComputedStyleWithValue(
      const CSSProperty& property,
      const CSSValue& value) {
    StyleResolverState state(GetDocument(), *GetDocument().body());
    state.SetStyle(GetDocument().GetStyleResolver().InitialStyle());

    // The border-style needs to be non-hidden and non-none, otherwise
    // the computed values of border-width properties are always zero.
    //
    // https://drafts.csswg.org/css-backgrounds-3/#the-border-width
    state.StyleBuilder().SetBorderBottomStyle(EBorderStyle::kSolid);
    state.StyleBuilder().SetBorderLeftStyle(EBorderStyle::kSolid);
    state.StyleBuilder().SetBorderRightStyle(EBorderStyle::kSolid);
    state.StyleBuilder().SetBorderTopStyle(EBorderStyle::kSolid);

    StyleBuilder::ApplyProperty(property, state, value);
    return state.TakeStyle();
  }

  const ExecutionContext* GetExecutionContext() const {
    return GetDocument().GetExecutionContext();
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
  const ComputedStyle& initial_style =
      GetDocument().GetStyleResolver().InitialStyle();

  // Count the number of 'visited' properties seen.
  size_t num_visited = 0;

  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    const CSSProperty* visited = property.GetVisitedProperty();
    if (!visited) {
      continue;
    }

    // Get any value compatible with 'property'. The initial value will do.
    const CSSValue* initial_value = property.CSSValueFromComputedStyle(
        initial_style, nullptr /* layout_object */,
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
  // NOTE: The downcast here is to go through the CSSProperty vtable,
  // so that we don't have to mark these functions as CORE_EXPORT only for
  // the test.
  const CSSProperty& inline_size = GetCSSPropertyInlineSize();
  const CSSProperty& writing_mode = GetCSSPropertyWebkitWritingMode();
  EXPECT_EQ(&GetCSSPropertyWidth(),
            inline_size.SurrogateFor(TextDirection::kLtr,
                                     WritingMode::kHorizontalTb));
  EXPECT_EQ(
      &GetCSSPropertyHeight(),
      inline_size.SurrogateFor(TextDirection::kLtr, WritingMode::kVerticalRl));
  EXPECT_EQ(&GetCSSPropertyWritingMode(),
            writing_mode.SurrogateFor(TextDirection::kLtr,
                                      WritingMode::kHorizontalTb));
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

TEST_F(CSSPropertyTest, OriginTrialTestProperty) {
  const CSSProperty& property = GetCSSPropertyOriginTrialTestProperty();

  {
    ScopedOriginTrialsSampleAPIForTest scoped_feature(false);

    EXPECT_FALSE(property.IsWebExposed());
    EXPECT_FALSE(property.IsUAExposed());
    EXPECT_EQ(CSSExposure::kNone, property.Exposure());
  }

  {
    ScopedOriginTrialsSampleAPIForTest scoped_feature(true);

    EXPECT_TRUE(property.IsWebExposed());
    EXPECT_TRUE(property.IsUAExposed());
    EXPECT_EQ(CSSExposure::kWeb, property.Exposure());
  }
}

TEST_F(CSSPropertyTest, OriginTrialTestPropertyWithContext) {
  const CSSProperty& property = GetCSSPropertyOriginTrialTestProperty();

  // Origin trial not enabled:
  EXPECT_FALSE(property.IsWebExposed(GetExecutionContext()));
  EXPECT_FALSE(property.IsUAExposed(GetExecutionContext()));
  EXPECT_EQ(CSSExposure::kNone, property.Exposure(GetExecutionContext()));

  // Enable it:
  LocalDOMWindow* window = GetFrame().DomWindow();
  OriginTrialContext* context = window->GetOriginTrialContext();
  context->AddFeature(OriginTrialFeature::kOriginTrialsSampleAPI);

  // Context-aware exposure functions should now report the property as
  // exposed.
  EXPECT_TRUE(property.IsWebExposed(GetExecutionContext()));
  EXPECT_TRUE(property.IsUAExposed(GetExecutionContext()));
  EXPECT_EQ(CSSExposure::kWeb, property.Exposure(GetExecutionContext()));

  // Context-agnostic exposure functions should still report kNone:
  EXPECT_FALSE(property.IsWebExposed());
  EXPECT_FALSE(property.IsUAExposed());
  EXPECT_EQ(CSSExposure::kNone, property.Exposure());
}

TEST_F(CSSPropertyTest, AlternativePropertyData) {
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    if (CSSPropertyID alternative_id = property.GetAlternative();
        alternative_id != CSSPropertyID::kInvalid) {
      SCOPED_TRACE(property.GetPropertyName());

      const CSSProperty& alternative = CSSProperty::Get(alternative_id);

      // The web-facing names of a alternative must be equal to that of the main
      // property.
      EXPECT_EQ(property.GetPropertyNameAtomicString(),
                alternative.GetPropertyNameAtomicString());
      EXPECT_EQ(property.GetPropertyNameString(),
                alternative.GetPropertyNameString());
      EXPECT_EQ(std::strcmp(property.GetPropertyName(),
                            alternative.GetPropertyName()),
                0);
      EXPECT_EQ(std::strcmp(property.GetJSPropertyName(),
                            alternative.GetJSPropertyName()),
                0);

      // Alternative properties should should also use the same CSSSampleId.
      EXPECT_EQ(GetCSSSampleId(property_id), GetCSSSampleId(alternative_id));
    }
  }
}

TEST_F(CSSPropertyTest, AlternativePropertyExposure) {
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    if (CSSPropertyID alternative_id = property.GetAlternative();
        alternative_id != CSSPropertyID::kInvalid) {
      SCOPED_TRACE(property.GetPropertyName());

      const CSSProperty& alternative = CSSProperty::Get(alternative_id);

      bool property_exposed = property.Exposure() != CSSExposure::kNone;
      bool alternative_exposed = alternative.Exposure() != CSSExposure::kNone;

      // If the alternative is exposed, the main property can not be exposed.
      EXPECT_TRUE(alternative_exposed ? !property_exposed : true);
    }
  }
}

TEST_F(CSSPropertyTest, AlternativePropertySingle) {
  CSSBitset seen_properties;

  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    if (property.GetAlternative() != CSSPropertyID::kInvalid) {
      SCOPED_TRACE(property.GetPropertyName());

      // A alternative is only pointed to from a single property.
      ASSERT_FALSE(seen_properties.Has(property_id));
      seen_properties.Set(property_id);
    }
  }
}

TEST_F(CSSPropertyTest, AlternativePropertyCycle) {
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    SCOPED_TRACE(property.GetPropertyName());

    // Verify that alternative properties aren't cyclic.
    CSSBitset seen_properties;
    for (CSSPropertyID current_id = property_id;
         current_id != CSSPropertyID::kInvalid;
         current_id = CSSProperty::Get(current_id).GetAlternative()) {
      ASSERT_FALSE(seen_properties.Has(current_id));
      seen_properties.Set(current_id);
    }
  }
}

}  // namespace blink
