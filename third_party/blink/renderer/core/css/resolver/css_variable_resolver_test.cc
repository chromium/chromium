// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

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
    UpdateAllLifecyclePhasesForTest();
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
        name, tokens, false,
        *MakeGarbageCollected<CSSParserContext>(GetDocument()));
  }

  const CSSVariableReferenceValue* CreateVariableReference(
      const String& value) {
    const auto tokens = CSSTokenizer(value).TokenizeToEOF();

    const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    const bool is_animation_tainted = false;
    const bool needs_variable_resolution = true;

    return MakeGarbageCollected<CSSVariableReferenceValue>(
        CSSVariableData::Create(tokens, is_animation_tainted,
                                needs_variable_resolution, context->BaseURL(),
                                context->Charset()),
        *context);
  }

  const CSSValue* ResolveVar(StyleResolverState& state,
                             CSSPropertyID property_id,
                             const String& value) {
    const CSSVariableReferenceValue* var = CreateVariableReference(value);

    CSSVariableResolver resolver(state);
    const bool disallow_animation_tainted = false;

    return resolver.ResolveVariableReferences(property_id, *var,
                                              disallow_animation_tainted);
  }

  const CSSValue* CreatePxValue(double px) {
    return CSSNumericLiteralValue::Create(px,
                                          CSSPrimitiveValue::UnitType::kPixels);
  }

  size_t MaxSubstitutionTokens() const {
    return CSSVariableResolver::kMaxSubstitutionTokens;
  }

  void SetTestHTMLWithReferencedVariableValue(const String& referenced) {
    StringBuilder builder;
    builder.Append("<style>\n");
    builder.Append("#target {\n");
    builder.Append("  --x:var(--referenced);\n");
    builder.Append("  --referenced:");
    builder.Append(referenced);
    builder.Append(";\n");
    builder.Append("}\n");
    builder.Append("</style>\n");
    builder.Append("<div id=target></div>\n");

    GetDocument().body()->SetInnerHTMLFromString(builder.ToString());
    UpdateAllLifecyclePhasesForTest();
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
  auto non_inherited_variables = std::make_unique<StyleNonInheritedVariables>();

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());

  const auto* prop = CreateCustomProperty("#fefefe");

  inherited_variables->SetData("--prop", prop->Value());
  non_inherited_variables->SetData("--prop", prop->Value());

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());
}

TEST_F(CSSVariableResolverTest, VarNeedsResolution) {
  scoped_refptr<StyleInheritedVariables> inherited_variables =
      StyleInheritedVariables::Create();
  auto non_inherited_variables = std::make_unique<StyleNonInheritedVariables>();

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());

  const auto* prop1 = CreateCustomProperty("var(--prop2)");
  const auto* prop2 = CreateCustomProperty("#fefefe");

  inherited_variables->SetData("--prop1", prop1->Value());
  non_inherited_variables->SetData("--prop1", prop1->Value());

  EXPECT_TRUE(inherited_variables->NeedsResolution());
  EXPECT_TRUE(non_inherited_variables->NeedsResolution());

  // While NeedsResolution() == true, add some properties without
  // var()-references.
  inherited_variables->SetData("--prop2", prop2->Value());
  non_inherited_variables->SetData("--prop2", prop2->Value());

  // We should still need resolution even after adding properties that don't
  // have var-references.
  EXPECT_TRUE(inherited_variables->NeedsResolution());
  EXPECT_TRUE(non_inherited_variables->NeedsResolution());

  inherited_variables->ClearNeedsResolution();
  non_inherited_variables->ClearNeedsResolution();

  EXPECT_FALSE(inherited_variables->NeedsResolution());
  EXPECT_FALSE(non_inherited_variables->NeedsResolution());
}

TEST_F(CSSVariableResolverTest, CopiedVariablesRetainNeedsResolution) {
  scoped_refptr<StyleInheritedVariables> inherited_variables =
      StyleInheritedVariables::Create();
  auto non_inherited_variables = std::make_unique<StyleNonInheritedVariables>();

  const auto* prop = CreateCustomProperty("var(--x)");

  inherited_variables->SetData("--prop", prop->Value());
  non_inherited_variables->SetData("--prop", prop->Value());

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
  // This test is not relevant when CSSCascade is enabled, as we won't store
  // unresolved CSSVariableData on the ComputedStyle in that case.
  if (RuntimeEnabledFeatures::CSSCascadeEnabled())
    return;

  const ComputedStyle* initial = &ComputedStyle::InitialStyle();
  StyleResolverState state(GetDocument(), *GetDocument().documentElement(),
                           initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->InheritFrom(*initial);
  state.SetStyle(std::move(style));

  const auto* prop1 = CreateCustomProperty("--prop1", "var(--prop2)");
  const auto* prop2 = CreateCustomProperty("--prop2", "#fefefe");
  const auto* prop3 = CreateCustomProperty("--prop3", "var(--prop2)");

  // Register prop3 to make it non-inherited.
  base::Optional<CSSSyntaxDefinition> token_syntax =
      CSSSyntaxStringParser("*").Parse();
  ASSERT_TRUE(token_syntax);
  String initial_value_str("foo");
  const auto tokens = CSSTokenizer(initial_value_str).TokenizeToEOF();
  const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
  const CSSValue* initial_value =
      token_syntax->Parse(CSSParserTokenRange(tokens), context, false);
  ASSERT_TRUE(initial_value);
  ASSERT_TRUE(initial_value->IsVariableReferenceValue());
  PropertyRegistration* registration =
      MakeGarbageCollected<PropertyRegistration>(
          "--prop3", *token_syntax, false, initial_value,
          To<CSSVariableReferenceValue>(*initial_value).VariableDataValue());
  ASSERT_TRUE(GetDocument().GetPropertyRegistry());
  GetDocument().GetPropertyRegistry()->RegisterProperty("--prop3",
                                                        *registration);

  CustomProperty("--prop1", GetDocument()).ApplyValue(state, *prop1);
  CustomProperty("--prop2", GetDocument()).ApplyValue(state, *prop2);
  CustomProperty("--prop3", GetDocument()).ApplyValue(state, *prop3);

  EXPECT_TRUE(state.Style()->InheritedVariables()->NeedsResolution());
  EXPECT_TRUE(state.Style()->NonInheritedVariables()->NeedsResolution());

  CSSVariableResolver(state).ResolveVariableDefinitions();

  EXPECT_FALSE(state.Style()->InheritedVariables()->NeedsResolution());
  EXPECT_FALSE(state.Style()->NonInheritedVariables()->NeedsResolution());
}

TEST_F(CSSVariableResolverTest, DontCrashWhenSettingInheritedNullVariable) {
  scoped_refptr<StyleInheritedVariables> inherited_variables =
      StyleInheritedVariables::Create();
  AtomicString name("--test");
  inherited_variables->SetData(name, nullptr);
  inherited_variables->SetValue(name, nullptr);
}

TEST_F(CSSVariableResolverTest, DontCrashWhenSettingNonInheritedNullVariable) {
  auto inherited_variables = std::make_unique<StyleNonInheritedVariables>();
  AtomicString name("--test");
  inherited_variables->SetData(name, nullptr);
  inherited_variables->SetValue(name, nullptr);
}

TEST_F(CSSVariableResolverTest, TokenCountAboveLimitIsInValidForSubstitution) {
  StringBuilder builder;
  for (size_t i = 0; i < MaxSubstitutionTokens(); ++i)
    builder.Append(":");
  builder.Append(":");
  builder.Append(";");
  SetTestHTMLWithReferencedVariableValue(builder.ToString());

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  // A custom property with more than MaxSubstitutionTokens() is valid ...
  const CSSVariableData* referenced =
      target->ComputedStyleRef().GetVariableData("--referenced");
  ASSERT_TRUE(referenced);
  EXPECT_EQ(MaxSubstitutionTokens() + 1, referenced->Tokens().size());

  // ... it is not valid for substitution, however.
  EXPECT_FALSE(target->ComputedStyleRef().GetVariableData("--x"));
}

TEST_F(CSSVariableResolverTest, TokenCountAtLimitIsValidForSubstitution) {
  StringBuilder builder;
  for (size_t i = 0; i < MaxSubstitutionTokens(); ++i)
    builder.Append(":");
  builder.Append(";");
  SetTestHTMLWithReferencedVariableValue(builder.ToString());

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  const CSSVariableData* referenced =
      target->ComputedStyleRef().GetVariableData("--referenced");
  ASSERT_TRUE(referenced);
  EXPECT_EQ(MaxSubstitutionTokens(), referenced->Tokens().size());

  const CSSVariableData* x = target->ComputedStyleRef().GetVariableData("--x");
  ASSERT_TRUE(x);
  EXPECT_EQ(MaxSubstitutionTokens(), x->Tokens().size());

  EXPECT_EQ(*referenced, *x);
}

TEST_F(CSSVariableResolverTest, BillionLaughs) {
  StringBuilder builder;
  builder.Append("<style>\n");
  builder.Append("#target {\n");

  // Produces:
  //
  // --x1:lol;
  // --x2:var(--x1)var(--x1);
  // --x4:var(--x2)var(--x2);
  // --x8:var(--x4)var(--x4);
  // .. etc
  builder.Append("  --x1:lol;\n");

  size_t tokens = 1;
  while (tokens <= MaxSubstitutionTokens()) {
    tokens *= 2;
    builder.Append("--x");
    builder.AppendNumber(tokens);
    builder.Append(":var(--x");
    builder.AppendNumber(tokens / 2);
    builder.Append(")");
    builder.Append("var(--x");
    builder.AppendNumber(tokens / 2);
    builder.Append(")");
    builder.Append(";");
  }

  builder.AppendNumber(tokens);
  builder.Append(");\n");

  builder.Append("--ref-last:var(--x");
  builder.AppendNumber(tokens);
  builder.Append(");");

  builder.Append("--ref-next-to-last:var(--x");
  builder.AppendNumber(tokens / 2);
  builder.Append(");");

  builder.Append("}\n");
  builder.Append("</style>\n");
  builder.Append("<div id=target></div>\n");

  GetDocument().body()->SetInnerHTMLFromString(builder.ToString());
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  // The last --x2^N variable is over the limit. Any reference to that
  // should be invalid.
  const CSSVariableData* ref_last =
      target->ComputedStyleRef().GetVariableData("--ref-last");
  EXPECT_FALSE(ref_last);

  // The next-to-last (--x2^(N-1)) variable is not over the limit. A reference
  // to that is still valid.
  const CSSVariableData* ref_next_to_last =
      target->ComputedStyleRef().GetVariableData("--ref-next-to-last");
  ASSERT_TRUE(ref_next_to_last);
  EXPECT_EQ(tokens / 2, ref_next_to_last->Tokens().size());

  // Ensure that there are a limited number of unique backing strings.
  // Each variable will have many backing strings, but should point to
  // a small number of StringImpls.

  HashSet<const StringImpl*> impls;
  for (const String& string : ref_next_to_last->BackingStrings())
    impls.insert(string.Impl());

  size_t expected_unique_strings = 0;

  // Each --x2^N property has a unique backing string (for its var-tokens, etc).
  // For --x1, that unique string is of course "lol".
  for (size_t i = (tokens / 2); i != 0; i = i >> 1)
    expected_unique_strings += 1;

  // --ref-next-to-last also has a backing string.
  expected_unique_strings += 1;

  EXPECT_EQ(expected_unique_strings, impls.size());
}

TEST_F(CSSVariableResolverTest, CSSWideKeywords) {
  using CSSUnsetValue = cssvalue::CSSUnsetValue;

  const ComputedStyle* initial = &ComputedStyle::InitialStyle();
  StyleResolverState state(GetDocument(), *GetDocument().documentElement(),
                           initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->InheritFrom(*initial);
  state.SetStyle(std::move(style));

  const CSSValue* whitespace = CreateCustomProperty("--w", " ");
  StyleBuilder::ApplyProperty(CSSPropertyName("--w"), state, *whitespace);

  // Test initial/inherit/unset for an inherited property:
  EXPECT_EQ(CSSInitialValue::Create(),
            ResolveVar(state, CSSPropertyID::kColor, "var(--w) initial"));
  EXPECT_EQ(CSSInheritedValue::Create(),
            ResolveVar(state, CSSPropertyID::kColor, "var(--w) inherit"));
  EXPECT_EQ(CSSUnsetValue::Create(),
            ResolveVar(state, CSSPropertyID::kColor, "var(--w) unset"));

  // Test initial/inherit/unset for a non-inherited property:
  EXPECT_EQ(CSSInitialValue::Create(),
            ResolveVar(state, CSSPropertyID::kWidth, "var(--w) initial"));
  EXPECT_EQ(CSSInheritedValue::Create(),
            ResolveVar(state, CSSPropertyID::kWidth, "var(--w) inherit"));
  EXPECT_EQ(CSSUnsetValue::Create(),
            ResolveVar(state, CSSPropertyID::kWidth, "var(--w) unset"));

  // Test initial/inherit/unset in fallbacks:

  EXPECT_EQ(CSSInitialValue::Create(),
            ResolveVar(state, CSSPropertyID::kColor, "var(--u,initial)"));
  EXPECT_EQ(CSSInheritedValue::Create(),
            ResolveVar(state, CSSPropertyID::kColor, "var(--u,inherit)"));
  EXPECT_EQ(CSSUnsetValue::Create(),
            ResolveVar(state, CSSPropertyID::kColor, "var(--u,unset)"));

  EXPECT_EQ(CSSInitialValue::Create(),
            ResolveVar(state, CSSPropertyID::kWidth, "var(--u,initial)"));
  EXPECT_EQ(CSSInheritedValue::Create(),
            ResolveVar(state, CSSPropertyID::kWidth, "var(--u,inherit)"));
  EXPECT_EQ(CSSUnsetValue::Create(),
            ResolveVar(state, CSSPropertyID::kWidth, "var(--u,unset)"));
}

}  // namespace blink
