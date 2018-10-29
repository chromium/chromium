// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {

// blue
static const Color kFallbackTestColor = Color(0, 0, 255);

// black
static const Color kMainBgTestColor = Color(0, 0, 0);

// red
static const Color kTestColor = Color(255, 0, 0);

}  // namespace

class CSSVariableResolverTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();

    RuntimeEnabledFeatures::SetCSSEnvironmentVariablesEnabled(true);
    GetStyleEngine().EnsureEnvironmentVariables().SetVariable("test", "red");
  }

  void SetTestHTML(const String& value) {
    GetDocument().body()->SetInnerHTMLFromString(
        "<style>"
        "  #target {"
        "    --main-bg-color: black;"
        "    --test: red;"
        "    background-color: " +
        value +
        "  }"
        "</style>"
        "<div>"
        "  <div id=target></div>"
        "</div>");
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  const CSSCustomPropertyDeclaration* CreateCustomProperty(
      const String& value) {
    return CreateCustomProperty("--unused", value);
  }

  const CSSCustomPropertyDeclaration* CreateCustomProperty(
      const AtomicString& name,
      const String& value) {
    const auto tokens = CSSTokenizer(value).TokenizeToEOF();
    return CSSVariableParser::ParseDeclarationValue(
        name, tokens, false, *CSSParserContext::Create(GetDocument()));
  }

  const CSSValue* CreatePxValue(double px) {
    return CSSPrimitiveValue::Create(px, CSSPrimitiveValue::UnitType::kPixels);
  }
};

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Missing_NestedVar) {
  SetTestHTML("env(missing, var(--main-bg-color))");

  // Check that the element has the background color provided by the
  // nested variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kMainBgTestColor, target->ComputedStyleRef().VisitedDependentColor(
                                  GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Missing_NestedVar_Fallback) {
  SetTestHTML("env(missing, var(--missing, blue))");

  // Check that the element has the fallback background color.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kFallbackTestColor,
            target->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Missing_WithFallback) {
  SetTestHTML("env(missing, blue)");

  // Check that the element has the fallback background color.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kFallbackTestColor,
            target->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Valid) {
  SetTestHTML("env(test)");

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColor, target->ComputedStyleRef().VisitedDependentColor(
                            GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Valid_WithFallback) {
  SetTestHTML("env(test, blue)");

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColor, target->ComputedStyleRef().VisitedDependentColor(
                            GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_WhenNested) {
  SetTestHTML("var(--main-bg-color, env(missing))");

  // Check that the element has the background color provided by var().
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kMainBgTestColor, target->ComputedStyleRef().VisitedDependentColor(
                                  GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_WhenNested_WillFallback) {
  SetTestHTML("var(--missing, env(test))");

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColor, target->ComputedStyleRef().VisitedDependentColor(
                            GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, NoResolutionWithoutVar) {
  scoped_refptr<StyleInheritedVariables> inherited_variables =
      StyleInheritedVariables::Create();
  std::unique_ptr<StyleNonInheritedVariables> non_inherited_variables =
      StyleNonInheritedVariables::Create();

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());

  const auto* prop = CreateCustomProperty("#fefefe");

  inherited_variables->SetVariable("--prop", prop->Value());
  non_inherited_variables->SetVariable("--prop", prop->Value());

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());
}

TEST_F(CSSVariableResolverTest, VarNeedsResolution) {
  scoped_refptr<StyleInheritedVariables> inherited_variables =
      StyleInheritedVariables::Create();
  std::unique_ptr<StyleNonInheritedVariables> non_inherited_variables =
      StyleNonInheritedVariables::Create();

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());

  const auto* prop1 = CreateCustomProperty("var(--prop2)");
  const auto* prop2 = CreateCustomProperty("#fefefe");

  inherited_variables->SetVariable("--prop1", prop1->Value());
  non_inherited_variables->SetVariable("--prop1", prop1->Value());

  EXPECT_TRUE(inherited_variables->NeedsResolution());
  EXPECT_TRUE(non_inherited_variables->NeedsResolution());

  // While NeedsResolution() == true, add some properties without
  // var()-references.
  inherited_variables->SetVariable("--prop2", prop2->Value());
  non_inherited_variables->SetVariable("--prop2", prop2->Value());

  // We should still need resolution even after adding properties that don't
  // have var-references.
  EXPECT_TRUE(inherited_variables->NeedsResolution());
  EXPECT_TRUE(non_inherited_variables->NeedsResolution());

  inherited_variables->ClearNeedsResolution();
  non_inherited_variables->ClearNeedsResolution();

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());
}

TEST_F(CSSVariableResolverTest, UrlNeedsResolution) {
  scoped_refptr<StyleInheritedVariables> inherited_variables =
      StyleInheritedVariables::Create();
  std::unique_ptr<StyleNonInheritedVariables> non_inherited_variables =
      StyleNonInheritedVariables::Create();

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());

  const auto* prop = CreateCustomProperty("url(a)");

  inherited_variables->SetVariable("--prop", prop->Value());
  non_inherited_variables->SetVariable("--prop", prop->Value());

  EXPECT_TRUE(inherited_variables->NeedsResolution());
  EXPECT_TRUE(non_inherited_variables->NeedsResolution());
}

TEST_F(CSSVariableResolverTest, CopiedVariablesRetainNeedsResolution) {
  scoped_refptr<StyleInheritedVariables> inherited_variables =
      StyleInheritedVariables::Create();
  std::unique_ptr<StyleNonInheritedVariables> non_inherited_variables =
      StyleNonInheritedVariables::Create();

  const auto* prop = CreateCustomProperty("var(--x)");

  inherited_variables->SetVariable("--prop", prop->Value());
  non_inherited_variables->SetVariable("--prop", prop->Value());

  EXPECT_TRUE(inherited_variables->NeedsResolution());
  EXPECT_TRUE(non_inherited_variables->NeedsResolution());
  EXPECT_TRUE(inherited_variables->Copy()->NeedsResolution());
  EXPECT_TRUE(non_inherited_variables->Clone()->NeedsResolution());

  inherited_variables->ClearNeedsResolution();
  non_inherited_variables->ClearNeedsResolution();

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());
  EXPECT_FALSE(inherited_variables->Copy()->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->Clone()->NeedsResolution());
}

TEST_F(CSSVariableResolverTest, NeedsResolutionClearedByResolver) {
  const ComputedStyle* initial = &ComputedStyle::InitialStyle();
  StyleResolverState state(GetDocument(), nullptr, initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->InheritFrom(*initial);
  state.SetStyle(std::move(style));

  const auto* prop1 = CreateCustomProperty("--prop1", "var(--prop2)");
  const auto* prop2 = CreateCustomProperty("--prop2", "#fefefe");
  const auto* prop3 = CreateCustomProperty("--prop3", "var(--prop2)");

  // Register prop3 to make it non-inherited.
  CSSSyntaxDescriptor token_syntax("*");
  String initial_value_str("foo");
  const auto tokens = CSSTokenizer(initial_value_str).TokenizeToEOF();
  const CSSParserContext* context = CSSParserContext::Create(GetDocument());
  const CSSValue* initial_value =
      token_syntax.Parse(CSSParserTokenRange(tokens), context, false);
  ASSERT_TRUE(initial_value);
  ASSERT_TRUE(initial_value->IsVariableReferenceValue());
  PropertyRegistration* registration = new PropertyRegistration(
      "--prop3", token_syntax, false, initial_value,
      ToCSSVariableReferenceValue(*initial_value).VariableDataValue());
  ASSERT_TRUE(GetDocument().GetPropertyRegistry());
  GetDocument().GetPropertyRegistry()->RegisterProperty("--prop3",
                                                        *registration);

  ToLonghand(GetCSSPropertyVariable()).ApplyValue(state, *prop1);
  ToLonghand(GetCSSPropertyVariable()).ApplyValue(state, *prop2);
  ToLonghand(GetCSSPropertyVariable()).ApplyValue(state, *prop3);

  EXPECT_TRUE(state.Style()->InheritedVariables()->NeedsResolution());
  EXPECT_TRUE(state.Style()->NonInheritedVariables()->NeedsResolution());

  CSSVariableResolver(state).ResolveVariableDefinitions();

  EXPECT_FALSE(state.Style()->InheritedVariables()->NeedsResolution());
  EXPECT_FALSE(state.Style()->NonInheritedVariables()->NeedsResolution());
}

TEST_F(CSSVariableResolverTest, RemoveInheritedVariableAtRoot) {
  scoped_refptr<StyleInheritedVariables> inherited_variables_root =
      StyleInheritedVariables::Create();

  AtomicString name("--prop");
  const auto* prop = CreateCustomProperty("test");
  const CSSValue* value = CreatePxValue(10.0);
  inherited_variables_root->SetVariable(name, prop->Value());
  inherited_variables_root->SetRegisteredVariable(name, value);

  EXPECT_TRUE(inherited_variables_root->GetVariable(name));
  EXPECT_TRUE(inherited_variables_root->RegisteredVariable(name));

  inherited_variables_root->RemoveVariable(name);

  EXPECT_FALSE(inherited_variables_root->GetVariable(name));
  EXPECT_FALSE(inherited_variables_root->RegisteredVariable(name));
}

TEST_F(CSSVariableResolverTest, RemoveInheritedVariableAtNonRoot) {
  scoped_refptr<StyleInheritedVariables> inherited_variables_root =
      StyleInheritedVariables::Create();
  scoped_refptr<StyleInheritedVariables> inherited_variables =
      inherited_variables_root->Copy();

  AtomicString name("--prop");
  const auto* prop = CreateCustomProperty("test");
  const CSSValue* value = CreatePxValue(10.0);
  inherited_variables->SetVariable(name, prop->Value());
  inherited_variables->SetRegisteredVariable(name, value);

  EXPECT_TRUE(inherited_variables->GetVariable(name));
  EXPECT_TRUE(inherited_variables->RegisteredVariable(name));

  inherited_variables->RemoveVariable(name);

  EXPECT_FALSE(inherited_variables->GetVariable(name));
  EXPECT_FALSE(inherited_variables->RegisteredVariable(name));
}

TEST_F(CSSVariableResolverTest, RemoveVariableInheritedViaRoot) {
  scoped_refptr<StyleInheritedVariables> inherited_variables_root =
      StyleInheritedVariables::Create();

  AtomicString name("--prop");
  const auto* prop = CreateCustomProperty("test");
  const CSSValue* value = CreatePxValue(10.0);
  inherited_variables_root->SetVariable(name, prop->Value());
  inherited_variables_root->SetRegisteredVariable(name, value);

  scoped_refptr<StyleInheritedVariables> inherited_variables =
      inherited_variables_root->Copy();

  EXPECT_TRUE(inherited_variables->GetVariable(name));
  EXPECT_TRUE(inherited_variables->RegisteredVariable(name));

  inherited_variables->RemoveVariable(name);

  EXPECT_FALSE(inherited_variables->GetVariable(name));
  EXPECT_FALSE(inherited_variables->RegisteredVariable(name));
}

TEST_F(CSSVariableResolverTest, RemoveNonInheritedVariable) {
  std::unique_ptr<StyleNonInheritedVariables> non_inherited_variables =
      StyleNonInheritedVariables::Create();

  AtomicString name("--prop");
  const auto* prop = CreateCustomProperty("test");
  const CSSValue* value = CreatePxValue(10.0);
  non_inherited_variables->SetVariable(name, prop->Value());
  non_inherited_variables->SetRegisteredVariable(name, value);

  EXPECT_TRUE(non_inherited_variables->GetVariable(name));
  EXPECT_TRUE(non_inherited_variables->RegisteredVariable(name));

  non_inherited_variables->RemoveVariable(name);

  EXPECT_FALSE(non_inherited_variables->GetVariable(name));
  EXPECT_FALSE(non_inherited_variables->RegisteredVariable(name));
}

TEST_F(CSSVariableResolverTest, DontCrashWhenSettingInheritedNullVariable) {
  scoped_refptr<StyleInheritedVariables> inherited_variables =
      StyleInheritedVariables::Create();
  AtomicString name("--test");
  inherited_variables->SetVariable(name, nullptr);
  inherited_variables->SetRegisteredVariable(name, nullptr);
}

TEST_F(CSSVariableResolverTest, DontCrashWhenSettingNonInheritedNullVariable) {
  std::unique_ptr<StyleNonInheritedVariables> inherited_variables =
      StyleNonInheritedVariables::Create();
  AtomicString name("--test");
  inherited_variables->SetVariable(name, nullptr);
  inherited_variables->SetRegisteredVariable(name, nullptr);
}

}  // namespace blink
