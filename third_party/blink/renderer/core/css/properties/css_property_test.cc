// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include <cstring>
#include "base/memory/values_equivalent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/css/anchor_evaluator.h"
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

namespace {

// Evaluates any query to '1' when it's in the expected mode,
// otherwise std::nullopt.
class ModeCheckingAnchorEvaluator : public AnchorEvaluator {
  STACK_ALLOCATED();

 public:
  explicit ModeCheckingAnchorEvaluator(AnchorScope::Mode required_mode)
      : required_mode_(required_mode) {}

  std::optional<LayoutUnit> Evaluate(
      const AnchorQuery&,
      const ScopedCSSName* position_anchor,
      const std::optional<PositionAreaOffsets>&) override {
    return (required_mode_ == GetMode()) ? std::optional<LayoutUnit>(1)
                                         : std::optional<LayoutUnit>();
  }

  std::optional<PositionAreaOffsets> ComputePositionAreaOffsetsForLayout(
      const ScopedCSSName*,
      PositionArea) override {
    return std::nullopt;
  }
  std::optional<PhysicalOffset> ComputeAnchorCenterOffsets(
      const ComputedStyleBuilder& builder) override {
    return std::nullopt;
  }

 private:
  AnchorScope::Mode required_mode_;
};

}  // namespace

class CSSPropertyTest : public PageTestBase {
 public:
  const CSSValue* Parse(String name, String value) {
    const CSSPropertyValueSet* set =
        css_test_helpers::ParseDeclarationBlock(name + ":" + value);
    DCHECK(set);
    if (set->PropertyCount() != 1) {
      return nullptr;
    }
    return &set->PropertyAt(0).Value();
  }

  const CSSPropertyValueSet* ParseShorthand(String name, String value) {
    return css_test_helpers::ParseDeclarationBlock(name + ":" + value);
  }

  String ComputedValue(String property_str,
                       String value_str,
                       StyleRecalcContext style_recalc_context) {
    CSSPropertyRef ref(property_str, GetDocument());
    CHECK(ref.IsValid());
    const CSSProperty& property = ref.GetProperty();

    const CSSValue* value =
        css_test_helpers::ParseLonghand(GetDocument(), property, value_str);
    CHECK(value);
    // Any tree-scoped references within `result` need to be populated with
    // their TreeScope. This is normally done by StyleCascade before length
    // conversion, and we're simulating that here.
    value = &value->EnsureScopedValue(&GetDocument());

    StyleResolverState state(GetDocument(), *GetDocument().body(),
                             &style_recalc_context);
    state.SetStyle(GetDocument().GetStyleResolver().InitialStyle());

    StyleBuilder::ApplyProperty(property, state, *value);
    const ComputedStyle* style = state.TakeStyle();
    CHECK(style);

    const CSSValue* computed_value = property.CSSValueFromComputedStyle(
        *style,
        /* layout_object */ nullptr,
        /* allow_visited_style */ true, CSSValuePhase::kComputedValue);
    CHECK(computed_value);

    return computed_value->CssText();
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
        false /* allow_visited_style */, CSSValuePhase::kComputedValue);
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
  const WritingDirectionMode kHorizontalLtr = {WritingMode::kHorizontalTb,
                                               TextDirection::kLtr};
  EXPECT_EQ(&GetCSSPropertyWidth(), inline_size.SurrogateFor(kHorizontalLtr));
  EXPECT_EQ(&GetCSSPropertyHeight(),
            inline_size.SurrogateFor(
                {WritingMode::kVerticalRl, TextDirection::kLtr}));
  EXPECT_EQ(&GetCSSPropertyWritingMode(),
            writing_mode.SurrogateFor(kHorizontalLtr));
  EXPECT_FALSE(GetCSSPropertyWidth().SurrogateFor(kHorizontalLtr));
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
  context->AddFeature(mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI);

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
    // TODO(pdr): Remove this IsPropertyAlias check, and properly handle aliases
    // in this test.
    if (CSSPropertyID alternative_id = property.GetAlternative();
        alternative_id != CSSPropertyID::kInvalid &&
        !IsPropertyAlias(alternative_id)) {
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
    // TODO(pdr): Remove this call to `ResolveCSSPropertyID` by properly
    // handling aliases in this test.
    if (CSSPropertyID alternative_id =
            ResolveCSSPropertyID(property.GetAlternative());
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
         // TODO(pdr): Remove this call to `ResolveCSSPropertyID` by properly
         // handling aliases in this test.
         current_id = ResolveCSSPropertyID(
             CSSProperty::Get(current_id).GetAlternative())) {
      ASSERT_FALSE(seen_properties.Has(current_id));
      seen_properties.Set(current_id);
    }
  }
}

TEST_F(CSSPropertyTest, AnchorModeTop) {
  ModeCheckingAnchorEvaluator anchor_evaluator(AnchorScope::Mode::kTop);
  StyleRecalcContext context = {.anchor_evaluator = &anchor_evaluator};

  EXPECT_EQ("1px", ComputedValue("top", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("right", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("bottom", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("left", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-height", "anchor-size(width, 0px)", context));
}

TEST_F(CSSPropertyTest, AnchorModeRight) {
  ModeCheckingAnchorEvaluator anchor_evaluator(AnchorScope::Mode::kRight);
  StyleRecalcContext context = {.anchor_evaluator = &anchor_evaluator};

  EXPECT_EQ("0px", ComputedValue("top", "anchor(top, 0px)", context));
  EXPECT_EQ("1px", ComputedValue("right", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("bottom", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("left", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-height", "anchor-size(width, 0px)", context));
}

TEST_F(CSSPropertyTest, AnchorModeBottom) {
  ModeCheckingAnchorEvaluator anchor_evaluator(AnchorScope::Mode::kBottom);
  StyleRecalcContext context = {.anchor_evaluator = &anchor_evaluator};

  EXPECT_EQ("0px", ComputedValue("top", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("right", "anchor(top, 0px)", context));
  EXPECT_EQ("1px", ComputedValue("bottom", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("left", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-height", "anchor-size(width, 0px)", context));
}

TEST_F(CSSPropertyTest, AnchorModeLeft) {
  ModeCheckingAnchorEvaluator anchor_evaluator(AnchorScope::Mode::kLeft);
  StyleRecalcContext context = {.anchor_evaluator = &anchor_evaluator};

  EXPECT_EQ("0px", ComputedValue("top", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("right", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("bottom", "anchor(top, 0px)", context));
  EXPECT_EQ("1px", ComputedValue("left", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-height", "anchor-size(width, 0px)", context));
}

TEST_F(CSSPropertyTest, AnchorModeWidth) {
  ModeCheckingAnchorEvaluator anchor_evaluator(AnchorScope::Mode::kWidth);
  StyleRecalcContext context = {.anchor_evaluator = &anchor_evaluator};

  EXPECT_EQ("0px", ComputedValue("top", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("right", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("bottom", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("left", "anchor(top, 0px)", context));
  EXPECT_EQ("1px", ComputedValue("width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("1px",
            ComputedValue("min-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("1px",
            ComputedValue("max-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-height", "anchor-size(width, 0px)", context));
}

TEST_F(CSSPropertyTest, AnchorModeHeight) {
  ModeCheckingAnchorEvaluator anchor_evaluator(AnchorScope::Mode::kHeight);
  StyleRecalcContext context = {.anchor_evaluator = &anchor_evaluator};

  EXPECT_EQ("0px", ComputedValue("top", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("right", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("bottom", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("left", "anchor(top, 0px)", context));
  EXPECT_EQ("0px", ComputedValue("width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("1px", ComputedValue("height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("min-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("1px",
            ComputedValue("min-height", "anchor-size(width, 0px)", context));
  EXPECT_EQ("0px",
            ComputedValue("max-width", "anchor-size(width, 0px)", context));
  EXPECT_EQ("1px",
            ComputedValue("max-height", "anchor-size(width, 0px)", context));
}

TEST_F(CSSPropertyTest, InsetAreaDisabled) {
  ScopedCSSInsetAreaPropertyForTest inset_area_enabled(false);
  auto* declarations = ParseShorthand("position-area", "center top");
  ASSERT_TRUE(declarations);
  ASSERT_EQ(declarations->PropertyCount(), 1u);
  declarations = ParseShorthand("inset-area", "center top");
  ASSERT_TRUE(declarations);
  ASSERT_EQ(declarations->PropertyCount(), 0u);
}

}  // namespace blink
