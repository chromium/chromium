// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url/dom_origin.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

using DOMOriginTest = testing::Test;

//
// Construction and parsing:
//
TEST_F(DOMOriginTest, BuildOpaqueOrigins) {
  DummyExceptionStateForTesting exception_state;

  {
    DOMOrigin* origin = DOMOrigin::Create();
    ASSERT_TRUE(origin);
    EXPECT_TRUE(origin->opaque());
  }

  {
    DOMOrigin* origin = DOMOrigin::Create("null", exception_state);
    ASSERT_TRUE(origin);
    EXPECT_TRUE(origin->opaque());
    EXPECT_FALSE(exception_state.HadException());
  }

  {
    DOMOrigin* origin = DOMOrigin::parse("null");
    ASSERT_TRUE(origin);
    EXPECT_TRUE(origin->opaque());
  }
}

TEST_F(DOMOriginTest, BuildTupleOrigins) {
  const char* test_cases[] = {
      "http://site.example",      "https://site.example",
      "https://site.example:123", "http://sub.site.example",
      "https://sub.site.example", "https://sub.site.example:123",
  };

  for (const char* test : test_cases) {
    SCOPED_TRACE(testing::Message() << "Construction(" << test << ")");

    DummyExceptionStateForTesting exception_state;
    DOMOrigin* origin = DOMOrigin::Create(test, exception_state);
    ASSERT_TRUE(origin);
    EXPECT_FALSE(exception_state.HadException());
    EXPECT_TRUE(SecurityOrigin::CreateFromString(test)->IsSameOriginWith(
        origin->GetOriginForTesting()));
  }

  for (const char* test : test_cases) {
    SCOPED_TRACE(testing::Message() << "Parsing(" << test << ")");

    DummyExceptionStateForTesting exception_state;
    DOMOrigin* origin = DOMOrigin::parse(test);
    ASSERT_TRUE(origin);
    EXPECT_TRUE(SecurityOrigin::CreateFromString(test)->IsSameOriginWith(
        origin->GetOriginForTesting()));
  }
}

//
// Comparison
//
TEST_F(DOMOriginTest, CompareOpaqueOrigins) {
  DummyExceptionStateForTesting exception_state;

  DOMOrigin* opaque_a = DOMOrigin::Create("null", exception_state);
  DOMOrigin* opaque_b = DOMOrigin::Create("null", exception_state);

  EXPECT_TRUE(opaque_a->isSameOrigin(opaque_a));
  EXPECT_TRUE(opaque_a->isSameSite(opaque_a));
  EXPECT_FALSE(opaque_a->isSameOrigin(opaque_b));
  EXPECT_FALSE(opaque_a->isSameSite(opaque_b));
  EXPECT_FALSE(opaque_b->isSameOrigin(opaque_a));
  EXPECT_FALSE(opaque_b->isSameSite(opaque_a));
}

TEST_F(DOMOriginTest, CompareTupleOrigins) {
  DummyExceptionStateForTesting exception_state;

  DOMOrigin* a = DOMOrigin::Create("https://a.example", exception_state);
  DOMOrigin* a_a = DOMOrigin::Create("https://a.a.example", exception_state);
  DOMOrigin* b_a = DOMOrigin::Create("https://b.a.example", exception_state);
  DOMOrigin* b = DOMOrigin::Create("https://b.example", exception_state);
  DOMOrigin* b_b = DOMOrigin::Create("https://b.b.example", exception_state);
  ASSERT_FALSE(exception_state.HadException());

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
TEST_F(DOMOriginTest, InvalidOrigins) {
  const char* test_cases[] = {
      "",
      "invalid",
      "about:blank",
      "https://trailing.slash/",
      "https://user:pass@site.example",
      "https://very.long.port:123456789",
      "https://ümlauted.example",
  };

  for (auto* test : test_cases) {
    SCOPED_TRACE(testing::Message() << "Construction(" << test << ")");

    DummyExceptionStateForTesting exception_state;
    DOMOrigin* origin = DOMOrigin::Create(test, exception_state);
    EXPECT_EQ(origin, nullptr);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Code(), ToExceptionCode(ESErrorType::kTypeError));
    EXPECT_EQ(exception_state.Message(), "Invalid serialized origin");
  }

  for (auto* test : test_cases) {
    SCOPED_TRACE(testing::Message() << "Parsing(" << test << ")");

    DOMOrigin* origin = DOMOrigin::parse(test);
    EXPECT_EQ(origin, nullptr);
  }
}

//
// Parsing serialized URLs, not Origins.
//
TEST_F(DOMOriginTest, ParsingInvalidURLs) {
  const char* test_cases[] = {
      "",
      "invalid",
      "https://very.long.port:123456789",
  };

  for (auto* test : test_cases) {
    SCOPED_TRACE(testing::Message() << "fromURL(" << test << ")");

    DOMOrigin* origin = DOMOrigin::fromURL(test);
    EXPECT_EQ(origin, nullptr);
  }
}

TEST_F(DOMOriginTest, ParsingOpaqueURLs) {
  const char* test_cases[] = {
      "about:blank",
  };

  for (auto* test : test_cases) {
    SCOPED_TRACE(testing::Message() << "fromURL(" << test << ")");

    DOMOrigin* origin = DOMOrigin::fromURL(test);
    ASSERT_TRUE(origin);
    EXPECT_TRUE(origin->opaque());
  }
}

TEST_F(DOMOriginTest, ParsingValidURLs) {
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
    SCOPED_TRACE(testing::Message() << "fromURL(" << test << ")");

    DOMOrigin* origin = DOMOrigin::fromURL(test);
    ASSERT_TRUE(origin);
    EXPECT_EQ(!origin->opaque(),
              SecurityOrigin::CreateFromString(test)->IsSameOriginWith(
                  origin->GetOriginForTesting()));
  }
}

}  // namespace

}  // namespace blink
