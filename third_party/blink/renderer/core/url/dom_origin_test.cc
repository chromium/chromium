// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url/dom_origin.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

ScriptValue ScriptValueFromString(V8TestingScope& scope, String blink_string) {
  return ScriptValue(scope.GetIsolate(),
                     V8String(scope.GetIsolate(), blink_string));
}

DOMOrigin* DOMOriginFromString(V8TestingScope& scope, String blink_string) {
  return DOMOrigin::from(scope.GetScriptState(),
                         ScriptValueFromString(scope, blink_string),
                         scope.GetExceptionState());
}

using DOMOriginTest = testing::Test;

//
// Construction and parsing:
//
TEST_F(DOMOriginTest, BuildOpaqueOrigins) {
  test::TaskEnvironment environment;
  V8TestingScope scope;

  DOMOrigin* origin = DOMOrigin::Create();
  ASSERT_TRUE(origin);
  EXPECT_TRUE(origin->opaque());
}

TEST_F(DOMOriginTest, BuildTupleOrigins) {
  test::TaskEnvironment environment;
  V8TestingScope scope;

  const char* test_cases[] = {
      "http://site.example",      "https://site.example",
      "https://site.example:123", "http://sub.site.example",
      "https://sub.site.example", "https://sub.site.example:123",
  };

  for (const char* test : test_cases) {
    SCOPED_TRACE(testing::Message() << "from(\"" << test << "\")");

    DOMOrigin* origin = DOMOriginFromString(scope, test);
    ASSERT_TRUE(origin);
    EXPECT_FALSE(scope.GetExceptionState().HadException());
    EXPECT_TRUE(SecurityOrigin::CreateFromString(test)->IsSameOriginWith(
        origin->GetOriginForTesting()));
  }
}

//
// Comparison
//
TEST_F(DOMOriginTest, CompareOpaqueOrigins) {
  test::TaskEnvironment environment;
  V8TestingScope scope;

  DOMOrigin* opaque_a = DOMOrigin::Create();
  DOMOrigin* opaque_b = DOMOrigin::Create();

  EXPECT_TRUE(opaque_a->isSameOrigin(opaque_a));
  EXPECT_TRUE(opaque_a->isSameSite(opaque_a));
  EXPECT_FALSE(opaque_a->isSameOrigin(opaque_b));
  EXPECT_FALSE(opaque_a->isSameSite(opaque_b));
  EXPECT_FALSE(opaque_b->isSameOrigin(opaque_a));
  EXPECT_FALSE(opaque_b->isSameSite(opaque_a));
}

TEST_F(DOMOriginTest, CompareTupleOrigins) {
  test::TaskEnvironment environment;
  V8TestingScope scope;

  DOMOrigin* a = DOMOriginFromString(scope, "https://a.example");
  DOMOrigin* a_a = DOMOriginFromString(scope, "https://a.a.example");
  DOMOrigin* b_a = DOMOriginFromString(scope, "https://b.a.example");
  DOMOrigin* b = DOMOriginFromString(scope, "https://b.example");
  DOMOrigin* b_b = DOMOriginFromString(scope, "https://b.b.example");

  EXPECT_TRUE(a->isSameOrigin(a));
  EXPECT_FALSE(a->isSameOrigin(a_a));
  EXPECT_FALSE(a->isSameOrigin(b_a));
  EXPECT_FALSE(a->isSameOrigin(b));
  EXPECT_FALSE(a->isSameOrigin(b_b));

  EXPECT_TRUE(a->isSameSite(a));
  EXPECT_TRUE(a->isSameSite(a_a));
  EXPECT_TRUE(a->isSameSite(b_a));
  EXPECT_FALSE(a->isSameSite(b));
  EXPECT_FALSE(a->isSameSite(b_b));

  EXPECT_TRUE(a_a->isSameSite(a));
  EXPECT_TRUE(a_a->isSameSite(a_a));
  EXPECT_TRUE(a_a->isSameSite(b_a));
  EXPECT_FALSE(a_a->isSameSite(b));
  EXPECT_FALSE(a_a->isSameSite(b_b));
}

//
// Invalid
//
TEST_F(DOMOriginTest, InvalidURLs) {
  test::TaskEnvironment environment;
  V8TestingScope scope;

  const char* test_cases[] = {
      "",
      "invalid",
      "https://very.long.port:123456789",
  };

  for (auto* test : test_cases) {
    SCOPED_TRACE(testing::Message() << "from(\"" << test << "\")");

    DOMOrigin* origin = DOMOriginFromString(scope, test);
    EXPECT_EQ(origin, nullptr);

    DummyExceptionStateForTesting& exception_state = scope.GetExceptionState();
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Code(), ToExceptionCode(ESErrorType::kTypeError));
    EXPECT_EQ(exception_state.Message(),
              "The string provided cannot be parsed as a serialized URL.");
  }
}

TEST_F(DOMOriginTest, ParsingValidURLs) {
  test::TaskEnvironment environment;
  V8TestingScope scope;

  const char* test_cases[] = {
      "https://trailing.slash/",
      "https://user:pass@site.example",
      "https://has.a.port:1234/and/path",
      "https://ümlauted.example",
      "file:///path/to/a/file.txt",
      "blob:https://example.com/some-guid",
      "data:text/plain,hello",
      "ftp://example.com/",
      "https://example.com/path?query#fragment",
      "https://127.0.0.1/",
      "https://[::1]/",
      "https://xn--ls8h.example/",
  };

  for (auto* test : test_cases) {
    SCOPED_TRACE(testing::Message() << "from(\"" << test << "\")");

    DOMOrigin* origin = DOMOriginFromString(scope, test);
    ASSERT_TRUE(origin);
    EXPECT_FALSE(scope.GetExceptionState().HadException());
    EXPECT_EQ(!origin->opaque(),
              SecurityOrigin::CreateFromString(test)->IsSameOriginWith(
                  origin->GetOriginForTesting()));
  }
}

}  // namespace

}  // namespace blink
