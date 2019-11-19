// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

using css_test_helpers::RegisterProperty;
using VariableMode = CSSParserLocalContext::VariableMode;

namespace {

class CustomPropertyTest : public PageTestBase {
 public:
  void SetElementWithStyle(const String& value) {
    GetDocument().body()->SetInnerHTMLFromString("<div id='target' style='" +
                                                 value + "'></div>");
    UpdateAllLifecyclePhasesForTest();
  }

  const CSSValue* GetComputedValue(const CustomProperty& property) {
    Element* node = GetDocument().getElementById("target");
    return property.CSSValueFromComputedStyle(node->ComputedStyleRef(),
                                              nullptr /* layout_object */,
                                              false /* allow_visited_style */);
  }

  const CSSValue* ParseValue(const Longhand& property,
                             const String& value,
                             const CSSParserLocalContext& local_context) {
    CSSTokenizer tokenizer(value);
    const auto tokens = tokenizer.TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    return property.ParseSingleValue(range, *context, local_context);
  }
};

}  // namespace

TEST_F(CustomPropertyTest, UnregisteredPropertyIsInherited) {
  CustomProperty property("--x", GetDocument());
  EXPECT_TRUE(property.IsInherited());
}

TEST_F(CustomPropertyTest, RegisteredNonInheritedPropertyIsNotInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "42px", false);
  CustomProperty property("--x", GetDocument());
  EXPECT_FALSE(property.IsInherited());
}

TEST_F(CustomPropertyTest, RegisteredInheritedPropertyIsInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "42px", true);
  CustomProperty property("--x", GetDocument());
  EXPECT_TRUE(property.IsInherited());
}

TEST_F(CustomPropertyTest, StaticVariableInstance) {
  CustomProperty property("--x", GetDocument());
  EXPECT_FALSE(Variable::IsStaticInstance(property));
  EXPECT_TRUE(Variable::IsStaticInstance(GetCSSPropertyVariable()));
}

TEST_F(CustomPropertyTest, PropertyID) {
  CustomProperty property("--x", GetDocument());
  EXPECT_EQ(CSSPropertyID::kVariable, property.PropertyID());
}

TEST_F(CustomPropertyTest, GetPropertyNameAtomicString) {
  CustomProperty property("--x", GetDocument());
  EXPECT_EQ(AtomicString("--x"), property.GetPropertyNameAtomicString());
}

TEST_F(CustomPropertyTest, ComputedCSSValueUnregistered) {
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("--x:foo");
  const CSSValue* value = GetComputedValue(property);
  EXPECT_TRUE(value->IsCustomPropertyDeclaration());
  EXPECT_EQ("foo", value->CssText());
}

TEST_F(CustomPropertyTest, ComputedCSSValueInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", true);
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("--x:100px");
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsPrimitiveValue());
  const auto* primitive_value = To<CSSPrimitiveValue>(value);
  EXPECT_EQ(100, primitive_value->GetIntValue());
}

TEST_F(CustomPropertyTest, ComputedCSSValueNonInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("--x:100px");
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsPrimitiveValue());
  const auto* primitive_value = To<CSSPrimitiveValue>(value);
  EXPECT_EQ(100, primitive_value->GetIntValue());
}

TEST_F(CustomPropertyTest, ComputedCSSValueInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "100px", false);
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("");  // Do not apply --x.
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsPrimitiveValue());
  const auto* primitive_value = To<CSSPrimitiveValue>(value);
  EXPECT_EQ(100, primitive_value->GetIntValue());
}

TEST_F(CustomPropertyTest, ComputedCSSValueEmptyInitial) {
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("");  // Do not apply --x.
  const CSSValue* value = GetComputedValue(property);
  EXPECT_FALSE(value);
}

TEST_F(CustomPropertyTest, ComputedCSSValueLateRegistration) {
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("--x:100px");
  RegisterProperty(GetDocument(), "--x", "<length>", "100px", false);
  // The property was not registered when the style was computed, hence the
  // computed value should be what we expect for an unregistered property.
  const CSSValue* value = GetComputedValue(property);
  EXPECT_TRUE(value->IsCustomPropertyDeclaration());
  EXPECT_EQ("100px", value->CssText());
}

TEST_F(CustomPropertyTest, ParseSingleValueUnregistered) {
  CustomProperty property("--x", GetDocument());
  const CSSValue* value =
      ParseValue(property, "100px", CSSParserLocalContext());
  ASSERT_TRUE(value->IsCustomPropertyDeclaration());
  EXPECT_EQ("100px", value->CssText());
}

TEST_F(CustomPropertyTest, ParseSingleValueAnimationTainted) {
  CustomProperty property("--x", GetDocument());
  const CSSValue* value1 = ParseValue(
      property, "100px", CSSParserLocalContext().WithAnimationTainted(true));
  const CSSValue* value2 = ParseValue(
      property, "100px", CSSParserLocalContext().WithAnimationTainted(false));

  EXPECT_TRUE(
      To<CSSCustomPropertyDeclaration>(value1)->Value()->IsAnimationTainted());
  EXPECT_FALSE(
      To<CSSCustomPropertyDeclaration>(value2)->Value()->IsAnimationTainted());
}

TEST_F(CustomPropertyTest, ParseSingleValueTyped) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  CustomProperty property("--x", GetDocument());
  const CSSValue* value1 =
      ParseValue(property, "100px", CSSParserLocalContext());
  EXPECT_TRUE(value1->IsPrimitiveValue());
  EXPECT_EQ(100, To<CSSPrimitiveValue>(value1)->GetIntValue());

  const CSSValue* value2 =
      ParseValue(property, "maroon", CSSParserLocalContext());
  EXPECT_FALSE(value2);
}

TEST_F(CustomPropertyTest, ParseSingleValueUntyped) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  CustomProperty property("--x", GetDocument());
  const CSSValue* value = ParseValue(
      property, "maroon",
      CSSParserLocalContext().WithVariableMode(VariableMode::kUntyped));
  ASSERT_TRUE(value->IsCustomPropertyDeclaration());
  EXPECT_EQ("maroon", value->CssText());
}

TEST_F(CustomPropertyTest, ParseSingleValueValidatedUntyped) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  CustomProperty property("--x", GetDocument());
  auto local_context =
      CSSParserLocalContext().WithVariableMode(VariableMode::kValidatedUntyped);
  const CSSValue* value1 = ParseValue(property, "100px", local_context);
  ASSERT_TRUE(value1->IsCustomPropertyDeclaration());
  EXPECT_EQ("100px", value1->CssText());

  const CSSValue* value2 = ParseValue(property, "maroon", local_context);
  EXPECT_FALSE(value2);
}

TEST_F(CustomPropertyTest, GetCSSPropertyName) {
  CustomProperty property("--x", GetDocument());
  EXPECT_EQ(CSSPropertyName("--x"), property.GetCSSPropertyName());
}

}  // namespace blink
