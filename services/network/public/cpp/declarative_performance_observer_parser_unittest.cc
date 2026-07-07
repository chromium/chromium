// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/declarative_performance_observer_parser.h"

#include "services/network/public/mojom/declarative_performance_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(DeclarativePerformanceObserverParserTest, ParseValid) {
  std::string header =
      "report-to=\"default\", "
      "entry-types=(\"navigation\" \"visibility-state\" \"mark\")";
  auto policy = ParseDeclarativePerformanceObserverPolicy(header);
  ASSERT_TRUE(policy);
  ASSERT_TRUE(policy->reporting_endpoint.has_value());
  EXPECT_EQ(policy->reporting_endpoint.value(), "default");
  ASSERT_EQ(policy->entry_types.size(), 3u);
  EXPECT_EQ(policy->entry_types[0], mojom::PerformanceEntryType::kNavigation);
  EXPECT_EQ(policy->entry_types[1],
            mojom::PerformanceEntryType::kVisibilityState);
  EXPECT_EQ(policy->entry_types[2], mojom::PerformanceEntryType::kMark);
}

TEST(DeclarativePerformanceObserverParserTest, ParseInvalid) {
  std::string header = "invalid-header-@@";
  auto policy = ParseDeclarativePerformanceObserverPolicy(header);
  EXPECT_FALSE(policy);
}

TEST(DeclarativePerformanceObserverParserTest, ParseUserTiming) {
  std::string header = R"(include-user-timing=("my-mark"))";
  auto policy = ParseDeclarativePerformanceObserverPolicy(header);
  ASSERT_TRUE(policy);
  ASSERT_TRUE(policy->include_user_timing.has_value());
  ASSERT_EQ(policy->include_user_timing.value().size(), 1u);
  EXPECT_EQ(policy->include_user_timing.value()[0], "my-mark");
}

TEST(DeclarativePerformanceObserverParserTest, ParseEarlyFailures) {
  std::string header = R"(capture-early-failures=?1)";
  auto policy = ParseDeclarativePerformanceObserverPolicy(header);
  ASSERT_TRUE(policy);
  EXPECT_TRUE(policy->capture_early_failures);
}

TEST(DeclarativePerformanceObserverParserTest, ParseEarlyFailuresImplicitTrue) {
  // In Structured Headers (RFC 8941 Dictionary), a dictionary member with no
  // value (i.e., just the key name) is implicitly treated as boolean true.
  std::string header = "report-to=\"default\", capture-early-failures";
  auto policy = ParseDeclarativePerformanceObserverPolicy(header);
  ASSERT_TRUE(policy);
  EXPECT_TRUE(policy->capture_early_failures);
}

TEST(DeclarativePerformanceObserverParserTest, IgnoreUnknownFields) {
  std::string header = R"(unknown-field="value", report-to="my-endpoint")";
  auto policy = ParseDeclarativePerformanceObserverPolicy(header);
  ASSERT_TRUE(policy);
  ASSERT_TRUE(policy->reporting_endpoint.has_value());
  EXPECT_EQ(policy->reporting_endpoint.value(), "my-endpoint");
}

TEST(DeclarativePerformanceObserverParserTest, ParseEmpty) {
  auto policy = ParseDeclarativePerformanceObserverPolicy("");
  // structured_headers::ParseDictionary returns an empty dictionary for an
  // empty string, which is valid but results in a policy with no fields.
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->reporting_endpoint.has_value());
  EXPECT_TRUE(policy->entry_types.empty());
  EXPECT_FALSE(policy->include_user_timing.has_value());
  EXPECT_FALSE(policy->capture_early_failures);
}

TEST(DeclarativePerformanceObserverParserTest, SkipInvalidEnumValues) {
  std::string header = "entry-types=(\"navigation\" \"invalid-type\" \"mark\")";
  auto policy = ParseDeclarativePerformanceObserverPolicy(header);
  ASSERT_TRUE(policy);
  ASSERT_EQ(policy->entry_types.size(), 2u);
  EXPECT_EQ(policy->entry_types[0], mojom::PerformanceEntryType::kNavigation);
  EXPECT_EQ(policy->entry_types[1], mojom::PerformanceEntryType::kMark);
}

TEST(DeclarativePerformanceObserverParserTest, TypeMismatches) {
  // report-to as a token (should be string)
  // capture-early-failures as a string (should be boolean)
  std::string header = "report-to=default, capture-early-failures=\"true\"";
  auto policy = ParseDeclarativePerformanceObserverPolicy(header);
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->reporting_endpoint.has_value());
  EXPECT_FALSE(policy->capture_early_failures);
}

}  // namespace network
