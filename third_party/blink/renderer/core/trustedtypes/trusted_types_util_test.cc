// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_trustedscript.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

void TrustedTypesCheckForHTMLThrows(const String& string) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  LocalDOMWindow* window = dummy_page_holder->GetFrame().DomWindow();
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = TrustedTypesCheckForHTML(string, window, "", "", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  window->GetContentSecurityPolicy()->AddPolicies(ParseContentSecurityPolicies(
      "require-trusted-types-for 'script'",
      network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta,
      *(window->GetSecurityOrigin())));
  ASSERT_FALSE(exception_state.HadException());
  String s1 = TrustedTypesCheckForHTML(string, window, "", "", exception_state);
  EXPECT_TRUE(exception_state.HadException());
}

void TrustedTypesCheckForScriptThrows(const String& string) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  LocalDOMWindow* window = dummy_page_holder->GetFrame().DomWindow();
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s =
      TrustedTypesCheckForScript(string, window, "", "", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  window->GetContentSecurityPolicy()->AddPolicies(ParseContentSecurityPolicies(
      "require-trusted-types-for 'script'",
      network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta,
      *(window->GetSecurityOrigin())));
  ASSERT_FALSE(exception_state.HadException());
  String s1 =
      TrustedTypesCheckForScript(string, window, "", "", exception_state);
  EXPECT_TRUE(exception_state.HadException());
}

void TrustedTypesCheckForScriptURLThrows(const String& string) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  LocalDOMWindow* window = dummy_page_holder->GetFrame().DomWindow();
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s =
      TrustedTypesCheckForScriptURL(string, window, "", "", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  window->GetContentSecurityPolicy()->AddPolicies(ParseContentSecurityPolicies(
      "require-trusted-types-for 'script'",
      network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kMeta,
      *(window->GetSecurityOrigin())));
  ASSERT_FALSE(exception_state.HadException());
  String s1 =
      TrustedTypesCheckForScriptURL(string, window, "", "", exception_state);
  EXPECT_TRUE(exception_state.HadException());
}

void TrustedTypesCheckForScriptWorks(
    const V8UnionStringOrTrustedScript* string_or_trusted_script,
    String expected) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  LocalDOMWindow* window = dummy_page_holder->GetFrame().DomWindow();
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String s = TrustedTypesCheckForScript(string_or_trusted_script, window, "",
                                        "", exception_state);
  ASSERT_EQ(s, expected);
}

// TrustedTypesCheckForHTML tests
TEST(TrustedTypesUtilTest, TrustedTypesCheckForHTML_String) {
  test::TaskEnvironment task_environment;
  TrustedTypesCheckForHTMLThrows("A string");
}

// TrustedTypesCheckForScript tests
TEST(TrustedTypesUtilTest, TrustedTypesCheckForScript_TrustedScript) {
  test::TaskEnvironment task_environment;
  auto* script = MakeGarbageCollected<TrustedScript>("A string");
  auto* trusted_value =
      MakeGarbageCollected<V8UnionStringOrTrustedScript>(script);
  TrustedTypesCheckForScriptWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, TrustedTypesCheckForScript_String) {
  test::TaskEnvironment task_environment;
  TrustedTypesCheckForScriptThrows("A string");
}

// TrustedTypesCheckForScriptURL tests
TEST(TrustedTypesUtilTest, TrustedTypesCheckForScriptURL_String) {
  test::TaskEnvironment task_environment;
  TrustedTypesCheckForScriptURLThrows("A string");
}
}  // namespace blink
