// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_style_declaration.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/property_set_css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(CSSStyleDeclarationTest, getPropertyShorthand) {
  test::TaskEnvironment task_environment;
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("div { padding: var(--p); }");
  ASSERT_TRUE(sheet.CssRules());
  ASSERT_EQ(1u, sheet.CssRules()->length());
  ASSERT_EQ(CSSRule::kStyleRule, sheet.CssRules()->item(0)->GetType());
  CSSStyleRule* style_rule = To<CSSStyleRule>(sheet.CssRules()->item(0));
  CSSStyleDeclaration* style = style_rule->style();
  ASSERT_TRUE(style);
  EXPECT_EQ(AtomicString(), style->GetPropertyShorthand("padding"));
}

TEST(CSSStyleDeclarationTest, ParsingRevertWithFeatureEnabled) {
  test::TaskEnvironment task_environment;
  css_test_helpers::TestStyleSheet sheet;
  sheet.AddCSSRules("div { top: revert; --x: revert; }");
  ASSERT_TRUE(sheet.CssRules());
  ASSERT_EQ(1u, sheet.CssRules()->length());
  CSSStyleRule* style_rule = To<CSSStyleRule>(sheet.CssRules()->item(0));
  CSSStyleDeclaration* style = style_rule->style();
  ASSERT_TRUE(style);
  EXPECT_EQ("revert", style->getPropertyValue("top"));
  EXPECT_EQ("revert", style->getPropertyValue("--x"));

  // Test setProperty/getPropertyValue:

  DummyExceptionStateForTesting exception_state;

  style->SetPropertyInternal(CSSPropertyID::kLeft, "left", "revert", false,
                             SecureContextMode::kSecureContext,
                             exception_state);
  style->SetPropertyInternal(CSSPropertyID::kVariable, "--y", " revert", false,
                             SecureContextMode::kSecureContext,
                             exception_state);

  EXPECT_EQ("revert", style->getPropertyValue("left"));
  EXPECT_EQ("revert", style->getPropertyValue("--y"));
  EXPECT_FALSE(exception_state.HadException());
}

// CSSStyleDeclaration has a cache which maps e.g. backgroundPositionY to
// its associated CSSPropertyID.
//
// See CssPropertyInfo in css_style_declaration.cc.
TEST(CSSStyleDeclarationTest, ExposureCacheLeak) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_testing_scope;

  auto* property_value_set = MakeGarbageCollected<MutableCSSPropertyValueSet>(
      CSSParserMode::kHTMLStandardMode);
  auto* style = MakeGarbageCollected<PropertySetCSSStyleDeclaration>(
      v8_testing_scope.GetExecutionContext(), *property_value_set);

  ScriptState* script_state = v8_testing_scope.GetScriptState();
  v8::Isolate* isolate = v8_testing_scope.GetIsolate();

  v8::Local<v8::String> normal = V8String(isolate, "normal");

  DummyExceptionStateForTesting exception_state;

  const AtomicString origin_trial_test_property("originTrialTestProperty");
  {
    ScopedOriginTrialsSampleAPIForTest scoped_feature(true);
    EXPECT_TRUE(
        style->NamedPropertyQuery(origin_trial_test_property, exception_state));
    EXPECT_EQ(NamedPropertySetterResult::kIntercepted,
              style->AnonymousNamedSetter(script_state,
                                          origin_trial_test_property, normal));
    EXPECT_EQ("normal",
              style->AnonymousNamedGetter(origin_trial_test_property));
  }

  {
    ScopedOriginTrialsSampleAPIForTest scoped_feature(false);
    // Now that the feature is disabled, 'origin_trial_test_property' must not
    // be usable just because it was enabled and accessed previously.
    EXPECT_FALSE(
        style->NamedPropertyQuery(origin_trial_test_property, exception_state));
    EXPECT_EQ(NamedPropertySetterResult::kDidNotIntercept,
              style->AnonymousNamedSetter(script_state,
                                          origin_trial_test_property, normal));
    EXPECT_EQ(g_null_atom,
              style->AnonymousNamedGetter(origin_trial_test_property));
  }
}

}  // namespace blink
