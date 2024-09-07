// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using css_test_helpers::RegisterProperty;

namespace {

class CustomPropertyTest : public PageTestBase {
 public:
  void SetElementWithStyle(const String& value) {
    GetDocument().body()->setInnerHTML("<div id='target' style='" + value +
                                       "'></div>");
    UpdateAllLifecyclePhasesForTest();
  }

  const ComputedStyle& GetComputedStyle() {
    Element* node = GetDocument().getElementById(AtomicString("target"));
    return node->ComputedStyleRef();
  }

  const CSSValue* GetComputedValue(const CustomProperty& property) {
    return property.CSSValueFromComputedStyle(
        GetComputedStyle(), nullptr /* layout_object */,
        false /* allow_visited_style */, CSSValuePhase::kComputedValue);
  }

  const CSSValue* ParseValue(const CustomProperty& property,
                             const String& value,
                             const CSSParserLocalContext& local_context) {
    auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    return property.Parse(value, *context, local_context);
  }
};

}  // namespace

TEST_F(CustomPropertyTest, UnregisteredPropertyIsInherited) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  EXPECT_TRUE(property.IsInherited());
}

TEST_F(CustomPropertyTest, RegisteredNonInheritedPropertyIsNotInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "42px", false);
  CustomProperty property(AtomicString("--x"), GetDocument());
  EXPECT_FALSE(property.IsInherited());
}

TEST_F(CustomPropertyTest, RegisteredInheritedPropertyIsInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "42px", true);
  CustomProperty property(AtomicString("--x"), GetDocument());
  EXPECT_TRUE(property.IsInherited());
}

TEST_F(CustomPropertyTest, StaticVariableInstance) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  EXPECT_FALSE(Variable::IsStaticInstance(property));
  EXPECT_TRUE(Variable::IsStaticInstance(GetCSSPropertyVariable()));
}

TEST_F(CustomPropertyTest, PropertyID) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  EXPECT_EQ(CSSPropertyID::kVariable, property.PropertyID());
}

TEST_F(CustomPropertyTest, GetPropertyNameAtomicString) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  EXPECT_EQ(AtomicString("--x"), property.GetPropertyNameAtomicString());
}

TEST_F(CustomPropertyTest, ComputedCSSValueUnregistered) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  SetElementWithStyle("--x:foo");
  const CSSValue* value = GetComputedValue(property);
  EXPECT_TRUE(value->IsUnparsedDeclaration());
  EXPECT_EQ("foo", value->CssText());
}

TEST_F(CustomPropertyTest, ComputedCSSValueInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", true);
  CustomProperty property(AtomicString("--x"), GetDocument());
  SetElementWithStyle("--x:100px");
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsPrimitiveValue());
  const auto* primitive_value = To<CSSPrimitiveValue>(value);
  EXPECT_EQ(
      100, primitive_value->ComputeLength<double>(CSSToLengthConversionData()));
}

TEST_F(CustomPropertyTest, ComputedCSSValueNonInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  CustomProperty property(AtomicString("--x"), GetDocument());
  SetElementWithStyle("--x:100px");
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsPrimitiveValue());
  const auto* primitive_value = To<CSSPrimitiveValue>(value);
  EXPECT_EQ(
      100, primitive_value->ComputeLength<double>(CSSToLengthConversionData()));
}

TEST_F(CustomPropertyTest, ComputedCSSValueInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "100px", false);
  CustomProperty property(AtomicString("--x"), GetDocument());
  SetElementWithStyle("");  // Do not apply --x.
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsPrimitiveValue());
  const auto* primitive_value = To<CSSPrimitiveValue>(value);
  EXPECT_EQ(
      100, primitive_value->ComputeLength<double>(CSSToLengthConversionData()));
}

TEST_F(CustomPropertyTest, ComputedCSSValueEmptyInitial) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  SetElementWithStyle("");  // Do not apply --x.
  const CSSValue* value = GetComputedValue(property);
  EXPECT_FALSE(value);
}

TEST_F(CustomPropertyTest, ComputedCSSValueLateRegistration) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  SetElementWithStyle("--x:100px");
  RegisterProperty(GetDocument(), "--x", "<length>", "100px", false);
  // The property was not registered when the style was computed, hence the
  // computed value should be what we expect for an unregistered property.
  const CSSValue* value = GetComputedValue(property);
  EXPECT_TRUE(value->IsUnparsedDeclaration());
  EXPECT_EQ("100px", value->CssText());
}

TEST_F(CustomPropertyTest, ComputedCSSValueNumberCalc) {
  RegisterProperty(GetDocument(), "--x", "<number>", "0", false);
  CustomProperty property(AtomicString("--x"), GetDocument());
  SetElementWithStyle("--x:calc(24 / 10)");
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsNumericLiteralValue());
  const auto* numeric_literal = To<CSSNumericLiteralValue>(value);
  EXPECT_DOUBLE_EQ(2.4, numeric_literal->GetDoubleValue());
}

TEST_F(CustomPropertyTest, ComputedCSSValueIntegerCalc) {
  RegisterProperty(GetDocument(), "--x", "<integer>", "0", false);
  CustomProperty property(AtomicString("--x"), GetDocument());
  SetElementWithStyle("--x:calc(24 / 10)");
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsNumericLiteralValue());
  const auto* numeric_literal = To<CSSNumericLiteralValue>(value);
  EXPECT_DOUBLE_EQ(2.0, numeric_literal->GetDoubleValue());
}

TEST_F(CustomPropertyTest, ParseSingleValueUnregistered) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  const CSSValue* value =
      ParseValue(property, "100px", CSSParserLocalContext());
  ASSERT_TRUE(value->IsUnparsedDeclaration());
  EXPECT_EQ("100px", value->CssText());
}

TEST_F(CustomPropertyTest, ParseSingleValueAnimationTainted) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  const CSSValue* value1 = ParseValue(
      property, "100px", CSSParserLocalContext().WithAnimationTainted(true));
  const CSSValue* value2 = ParseValue(
      property, "100px", CSSParserLocalContext().WithAnimationTainted(false));

  EXPECT_TRUE(To<CSSUnparsedDeclarationValue>(value1)
                  ->VariableDataValue()
                  ->IsAnimationTainted());
  EXPECT_FALSE(To<CSSUnparsedDeclarationValue>(value2)
                   ->VariableDataValue()
                   ->IsAnimationTainted());
}

TEST_F(CustomPropertyTest, ParseSingleValueTyped) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  CustomProperty property(AtomicString("--x"), GetDocument());
  const CSSValue* value1 =
      ParseValue(property, "100px", CSSParserLocalContext());
  EXPECT_TRUE(value1->IsPrimitiveValue());
  EXPECT_EQ(100, To<CSSPrimitiveValue>(value1)->ComputeLength<double>(
                     CSSToLengthConversionData()));

  const CSSValue* value2 =
      ParseValue(property, "maroon", CSSParserLocalContext());
  EXPECT_FALSE(value2);
}

TEST_F(CustomPropertyTest, GetCSSPropertyName) {
  CustomProperty property(AtomicString("--x"), GetDocument());
  EXPECT_EQ(CSSPropertyName(AtomicString("--x")),
            property.GetCSSPropertyName());
}

TEST_F(CustomPropertyTest, SupportsGuaranteedInvalid) {
  RegisterProperty(GetDocument(), "--universal", "*", "foo", true);
  RegisterProperty(GetDocument(), "--no-initial", "*", std::nullopt, true);
  RegisterProperty(GetDocument(), "--length", "<length>", "0px", true);

  CustomProperty unregistered(AtomicString("--unregistered"), GetDocument());
  CustomProperty universal(AtomicString("--universal"), GetDocument());
  CustomProperty no_initial_value(AtomicString("--no-initial"), GetDocument());
  CustomProperty length(AtomicString("--length"), GetDocument());

  EXPECT_TRUE(unregistered.SupportsGuaranteedInvalid());
  EXPECT_TRUE(universal.SupportsGuaranteedInvalid());
  EXPECT_TRUE(no_initial_value.SupportsGuaranteedInvalid());
  EXPECT_FALSE(length.SupportsGuaranteedInvalid());
}

TEST_F(CustomPropertyTest, HasInitialValue) {
  RegisterProperty(GetDocument(), "--universal", "*", "foo", true);
  RegisterProperty(GetDocument(), "--no-initial", "*", std::nullopt, true);
  RegisterProperty(GetDocument(), "--length", "<length>", "0px", true);

  CustomProperty unregistered(AtomicString("--unregistered"), GetDocument());
  CustomProperty universal(AtomicString("--universal"), GetDocument());
  CustomProperty no_initial_value(AtomicString("--no-initial"), GetDocument());
  CustomProperty length(AtomicString("--length"), GetDocument());

  EXPECT_FALSE(unregistered.HasInitialValue());
  EXPECT_TRUE(universal.HasInitialValue());
  EXPECT_FALSE(no_initial_value.HasInitialValue());
  EXPECT_TRUE(length.HasInitialValue());
}

TEST_F(CustomPropertyTest, ParseAnchorQueriesAsLength) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  CustomProperty property(AtomicString("--x"), GetDocument());

  // We can't parse anchor queries as a <length>, because it can't be resolved
  // into a pixel value at style time.
  EXPECT_FALSE(
      ParseValue(property, "anchor(--foo top)", CSSParserLocalContext()));
  EXPECT_FALSE(ParseValue(property, "anchor-size(--foo width)",
                          CSSParserLocalContext()));
}

TEST_F(CustomPropertyTest, ParseAnchorQueriesAsLengthPercentage) {
  RegisterProperty(GetDocument(), "--x", "<length-percentage>", "0px", false);
  CustomProperty property(AtomicString("--x"), GetDocument());

  {
    const CSSValue* value =
        ParseValue(property, "anchor(--foo top)", CSSParserLocalContext());
    ASSERT_TRUE(value);
    EXPECT_EQ("anchor(--foo top)", value->CssText());
  }

  {
    const CSSValue* value = ParseValue(property, "anchor-size(--foo width)",
                                       CSSParserLocalContext());
    ASSERT_TRUE(value);
    EXPECT_EQ("anchor-size(--foo width)", value->CssText());
  }

  {
    // There are no restrictions on what anchor queries are allowed in a custom
    // property, so mixing anchor() and anchor-size() is also allowed, although
    // using it in any builtin property via var() makes it invalid at
    // computed-value time.
    const CSSValue* value = ParseValue(
        property, "calc(anchor(--foo top) + anchor-size(--foo width))",
        CSSParserLocalContext());
    ASSERT_TRUE(value);
    EXPECT_EQ("calc(anchor(--foo top) + anchor-size(--foo width))",
              value->CssText());
  }
}

TEST_F(CustomPropertyTest, ValueMode) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  CustomProperty property(AtomicString("--x"), GetDocument());

  CSSVariableData* data = css_test_helpers::CreateVariableData("100px");
  ASSERT_FALSE(data->IsAnimationTainted());
  auto* declaration = MakeGarbageCollected<CSSUnparsedDeclarationValue>(
      data, /* parser_context */ nullptr);

  // ValueMode::kNormal
  {
    StyleResolverState state(GetDocument(), *GetDocument().documentElement(),
                             /* StyleRecalcContext */ nullptr, StyleRequest());
    state.SetStyle(*GetDocument().GetStyleResolver().InitialStyleForElement());
    property.ApplyValue(state, *declaration, CSSProperty::ValueMode::kNormal);
    const ComputedStyle* style = state.TakeStyle();
    ASSERT_TRUE(style->GetVariableData(AtomicString("--x")));
    EXPECT_FALSE(
        style->GetVariableData(AtomicString("--x"))->IsAnimationTainted());
  }

  // ValueMode::kAnimated
  {
    StyleResolverState state(GetDocument(), *GetDocument().documentElement(),
                             /* StyleRecalcContext */ nullptr, StyleRequest());
    state.SetStyle(*GetDocument().GetStyleResolver().InitialStyleForElement());
    property.ApplyValue(state, *declaration, CSSProperty::ValueMode::kAnimated);
    const ComputedStyle* style = state.TakeStyle();
    ASSERT_TRUE(style->GetVariableData(AtomicString("--x")));
    EXPECT_TRUE(
        style->GetVariableData(AtomicString("--x"))->IsAnimationTainted());
  }
}

}  // namespace blink
