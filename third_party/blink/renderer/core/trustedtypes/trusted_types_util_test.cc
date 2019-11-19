// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html_or_trusted_script_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// Functions for checking throwing cases.
void GetStringFromTrustedTypeThrows(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL&
        string_or_trusted_type) {
  auto* document = MakeGarbageCollected<Document>();
  document->GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta);
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = GetStringFromTrustedType(string_or_trusted_type, document,
                                      exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void GetStringFromTrustedHTMLThrows(
    const StringOrTrustedHTML& string_or_trusted_html) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = GetStringFromTrustedHTML(string_or_trusted_html, &document,
                                      exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void GetStringFromTrustedScriptThrows(
    const StringOrTrustedScript& string_or_trusted_script) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = GetStringFromTrustedScript(string_or_trusted_script, &document,
                                        exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void GetStringFromTrustedScriptURLThrows(
    const StringOrTrustedScriptURL& string_or_trusted_script_url) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = GetStringFromTrustedScriptURL(string_or_trusted_script_url,
                                           &document, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

// Functions for checking non-throwing cases.
void GetStringFromTrustedTypeWorks(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL&
        string_or_trusted_type,
    String expected) {
  auto* document = MakeGarbageCollected<Document>();
  document->GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta);
  DummyExceptionStateForTesting exception_state;
  String s = GetStringFromTrustedType(string_or_trusted_type, document,
                                      exception_state);
  ASSERT_EQ(s, expected);
}

void GetStringFromTrustedHTMLWorks(
    const StringOrTrustedHTML& string_or_trusted_html,
    String expected) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String s = GetStringFromTrustedHTML(string_or_trusted_html, &document,
                                      exception_state);
  ASSERT_EQ(s, expected);
}

void GetStringFromTrustedScriptWorks(
    const StringOrTrustedScript& string_or_trusted_script,
    String expected) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String s = GetStringFromTrustedScript(string_or_trusted_script, &document,
                                        exception_state);
  ASSERT_EQ(s, expected);
}

void GetStringFromTrustedScriptURLWorks(
    const StringOrTrustedScriptURL& string_or_trusted_script_url,
    String expected) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.GetContentSecurityPolicy()->DidReceiveHeader(
      "trusted-types *", kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta);
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String s = GetStringFromTrustedScriptURL(string_or_trusted_script_url,
                                           &document, exception_state);
  ASSERT_EQ(s, expected);
}

// GetStringFromTrustedType() tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedType_TrustedHTML) {
  auto* html = MakeGarbageCollected<TrustedHTML>("A string");
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL trusted_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::FromTrustedHTML(
          html);
  GetStringFromTrustedTypeWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedType_TrustedScript) {
  auto* script = MakeGarbageCollected<TrustedScript>("A string");
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL trusted_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::FromTrustedScript(
          script);
  GetStringFromTrustedTypeWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedType_TrustedScriptURL) {
  String url_address = "http://www.example.com/";
  auto* script_url = MakeGarbageCollected<TrustedScriptURL>(url_address);
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL trusted_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::
          FromTrustedScriptURL(script_url);
  GetStringFromTrustedTypeWorks(trusted_value, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedType_TrustedScriptURL_Relative) {
  String url_address = "relative/url.html";
  auto* script_url = MakeGarbageCollected<TrustedScriptURL>(url_address);
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL trusted_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::
          FromTrustedScriptURL(script_url);
  GetStringFromTrustedTypeWorks(trusted_value, "relative/url.html");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedType_String) {
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL string_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::FromString(
          "A string");
  GetStringFromTrustedTypeThrows(string_value);
}

// GetStringFromTrustedTypeWithoutCheck() tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedTypeWithoutCheck_TrustedHTML) {
  auto* html = MakeGarbageCollected<TrustedHTML>("A string");
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL trusted_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::FromTrustedHTML(
          html);
  String s = GetStringFromTrustedTypeWithoutCheck(trusted_value);
  ASSERT_EQ(s, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedTypeWithoutCheck_TrustedScript) {
  auto* script = MakeGarbageCollected<TrustedScript>("A string");
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL trusted_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::FromTrustedScript(
          script);
  String s = GetStringFromTrustedTypeWithoutCheck(trusted_value);
  ASSERT_EQ(s, "A string");
}

TEST(TrustedTypesUtilTest,
     GetStringFromTrustedTypeWithoutCheck_TrustedScriptURL) {
  String url_address = "http://www.example.com/";
  auto* script_url = MakeGarbageCollected<TrustedScriptURL>(url_address);
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL trusted_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::
          FromTrustedScriptURL(script_url);
  String s = GetStringFromTrustedTypeWithoutCheck(trusted_value);
  ASSERT_EQ(s, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedTypeWithoutCheck_String) {
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL string_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::FromString(
          "A string");
  String s = GetStringFromTrustedTypeWithoutCheck(string_value);
  ASSERT_EQ(s, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedTypeWithoutCheck_Null) {
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL null_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL();
  String s = GetStringFromTrustedTypeWithoutCheck(null_value);
  ASSERT_EQ(s, "");
}

// GetStringFromTrustedHTML tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedHTML_TrustedHTML) {
  auto* html = MakeGarbageCollected<TrustedHTML>("A string");
  StringOrTrustedHTML trusted_value =
      StringOrTrustedHTML::FromTrustedHTML(html);
  GetStringFromTrustedHTMLWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedHTML_String) {
  StringOrTrustedHTML string_value =
      StringOrTrustedHTML::FromString("A string");
  GetStringFromTrustedHTMLThrows(string_value);
}

// GetStringFromTrustedScript tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedScript_TrustedScript) {
  auto* script = MakeGarbageCollected<TrustedScript>("A string");
  StringOrTrustedScript trusted_value =
      StringOrTrustedScript::FromTrustedScript(script);
  GetStringFromTrustedScriptWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedScript_String) {
  StringOrTrustedScript string_value =
      StringOrTrustedScript::FromString("A string");
  GetStringFromTrustedScriptThrows(string_value);
}

// GetStringFromTrustedScriptURL tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedScriptURL_TrustedScriptURL) {
  String url_address = "http://www.example.com/";
  auto* script_url = MakeGarbageCollected<TrustedScriptURL>(url_address);
  StringOrTrustedScriptURL trusted_value =
      StringOrTrustedScriptURL::FromTrustedScriptURL(script_url);
  GetStringFromTrustedScriptURLWorks(trusted_value, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedScriptURL_String) {
  StringOrTrustedScriptURL string_value =
      StringOrTrustedScriptURL::FromString("A string");
  GetStringFromTrustedScriptURLThrows(string_value);
}
}  // namespace blink
