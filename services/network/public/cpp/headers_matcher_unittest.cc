// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/headers_matcher.h"

#include <map>
#include <set>

#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class ShouldIgnoreHelper final {
 public:
  explicit ShouldIgnoreHelper(std::set<std::string> lowered_keys_to_ignore)
      : lowered_keys_to_ignore_(std::move(lowered_keys_to_ignore)) {}

  bool operator()(const std::string& lowered_key) const {
    call_counts_[lowered_key]++;
    return lowered_keys_to_ignore_.contains(lowered_key);
  }

  void VerifyCallCounts() const {
    for (const auto& [key, count] : call_counts_) {
      EXPECT_LE(count, 1) << "Key '" << key << "' was queried " << count
                          << " times.";
    }
  }

 private:
  std::set<std::string> lowered_keys_to_ignore_;
  mutable std::map<std::string, int> call_counts_;
};

TEST(HeadersMatcherTest, OrderInsensitive) {
  net::HttpRequestHeaders headers1;
  headers1.SetHeader("name1", "value1");
  headers1.SetHeader("name2", "value2");
  headers1.SetHeader("name3", "value3");

  net::HttpRequestHeaders headers2;
  headers2.SetHeader("name2", "value2");
  headers2.SetHeader("name3", "value3");
  headers2.SetHeader("name1", "value1");

  EXPECT_TRUE(
      MatchHttpRequestHeaders(headers1, headers2,
                              MatchHttpRequestHeadersValueOption::kEquals)
          .empty());
  EXPECT_TRUE(
      MatchHttpRequestHeaders(
          headers1, headers2,
          MatchHttpRequestHeadersValueOption::kEqualsCaseInsensitiveASCII)
          .empty());
}

TEST(HeadersMatcherTest, KeyCaseInsensitive) {
  net::HttpRequestHeaders headers1;
  headers1.SetHeader("NAME1", "value1");
  headers1.SetHeader("name2", "value2");

  net::HttpRequestHeaders headers2;
  headers2.SetHeader("name1", "value1");
  headers2.SetHeader("name2", "value2");

  EXPECT_TRUE(
      MatchHttpRequestHeaders(headers1, headers2,
                              MatchHttpRequestHeadersValueOption::kEquals)
          .empty());
  EXPECT_TRUE(
      MatchHttpRequestHeaders(
          headers1, headers2,
          MatchHttpRequestHeadersValueOption::kEqualsCaseInsensitiveASCII)
          .empty());
}

TEST(HeadersMatcherTest, ValueCaseDifference) {
  net::HttpRequestHeaders headers1;
  headers1.SetHeader("name1", "value1");
  headers1.SetHeader("name2", "value2");

  net::HttpRequestHeaders headers2;
  headers2.SetHeader("name1", "value1");
  headers2.SetHeader("name2", "VALUE2");

  {
    // Ignores no keys, just to verify `VerifyCallCounts()`.
    ShouldIgnoreHelper should_ignore({});
    EXPECT_FALSE(MatchHttpRequestHeaders(
                     headers1, headers2,
                     MatchHttpRequestHeadersValueOption::kEquals, should_ignore)
                     .empty());
    should_ignore.VerifyCallCounts();
  }

  {
    ShouldIgnoreHelper should_ignore({});
    EXPECT_TRUE(
        MatchHttpRequestHeaders(
            headers1, headers2,
            MatchHttpRequestHeadersValueOption::kEqualsCaseInsensitiveASCII,
            should_ignore)
            .empty());
    should_ignore.VerifyCallCounts();
  }
}

TEST(HeadersMatcherTest, MismatchedHttpRequestHeader1) {
  net::HttpRequestHeaders headers1;
  headers1.SetHeader("name1", "value1");
  headers1.SetHeader("name2", "value2");
  headers1.SetHeader("name3", "value3");
  headers1.SetHeader("name5", "value3");

  net::HttpRequestHeaders headers2;
  headers2.SetHeader("name1", "value1");
  headers2.SetHeader("name3", "value2");
  headers2.SetHeader("name4", "value4");
  headers2.SetHeader("name5", "value3");

  {
    auto mismatches = MatchHttpRequestHeaders(
        headers1, headers2, MatchHttpRequestHeadersValueOption::kEquals);
    ASSERT_EQ(3u, mismatches.size());

    EXPECT_TRUE(mismatches[0].EqualsForTesting(
        MismatchedHttpRequestHeader("name2", "value2", std::nullopt)));
    EXPECT_TRUE(mismatches[1].EqualsForTesting(
        MismatchedHttpRequestHeader("name3", "value3", "value2")));
    EXPECT_TRUE(mismatches[2].EqualsForTesting(
        MismatchedHttpRequestHeader("name4", std::nullopt, "value4")));
  }

  {
    ShouldIgnoreHelper should_ignore({"name1", "name4"});
    auto mismatches_with_should_ignore = MatchHttpRequestHeaders(
        headers1, headers2, MatchHttpRequestHeadersValueOption::kEquals,
        should_ignore);
    ASSERT_EQ(2u, mismatches_with_should_ignore.size());
    should_ignore.VerifyCallCounts();

    EXPECT_TRUE(mismatches_with_should_ignore[0].EqualsForTesting(
        MismatchedHttpRequestHeader("name2", "value2", std::nullopt)));
    EXPECT_TRUE(mismatches_with_should_ignore[1].EqualsForTesting(
        MismatchedHttpRequestHeader("name3", "value3", "value2")));
  }
}

TEST(HeadersMatcherTest, MismatchedHttpRequestHeader2) {
  net::HttpRequestHeaders headers1;
  headers1.SetHeader("name5", "value1");
  headers1.SetHeader("name6", "value2");
  headers1.SetHeader("name7", "value3");

  net::HttpRequestHeaders headers2;
  headers2.SetHeader("name2", "value1");

  {
    auto mismatches = MatchHttpRequestHeaders(
        headers1, headers2, MatchHttpRequestHeadersValueOption::kEquals);
    ASSERT_EQ(4u, mismatches.size());

    EXPECT_TRUE(mismatches[0].EqualsForTesting(
        MismatchedHttpRequestHeader("name2", std::nullopt, "value1")));
    EXPECT_TRUE(mismatches[1].EqualsForTesting(
        MismatchedHttpRequestHeader("name5", "value1", std::nullopt)));
    EXPECT_TRUE(mismatches[2].EqualsForTesting(
        MismatchedHttpRequestHeader("name6", "value2", std::nullopt)));
    EXPECT_TRUE(mismatches[3].EqualsForTesting(
        MismatchedHttpRequestHeader("name7", "value3", std::nullopt)));
  }

  {
    ShouldIgnoreHelper should_ignore({"name2", "name6"});
    auto mismatches_with_should_ignore = MatchHttpRequestHeaders(
        headers1, headers2, MatchHttpRequestHeadersValueOption::kEquals,
        should_ignore);
    ASSERT_EQ(2u, mismatches_with_should_ignore.size());
    should_ignore.VerifyCallCounts();

    EXPECT_TRUE(mismatches_with_should_ignore[0].EqualsForTesting(
        MismatchedHttpRequestHeader("name5", "value1", std::nullopt)));
    EXPECT_TRUE(mismatches_with_should_ignore[1].EqualsForTesting(
        MismatchedHttpRequestHeader("name7", "value3", std::nullopt)));
  }
}

TEST(HeadersMatcherTest, MismatchedHttpRequestHeader3) {
  net::HttpRequestHeaders headers1;
  headers1.SetHeader("name5", "value1");
  headers1.SetHeader("name6", "value2");

  net::HttpRequestHeaders headers2;
  headers2.SetHeader("name2", "value1");
  headers2.SetHeader("name6", "value2");
  headers2.SetHeader("name7", "value3");
  headers2.SetHeader("name8", "value3");

  auto mismatches = MatchHttpRequestHeaders(
      headers1, headers2, MatchHttpRequestHeadersValueOption::kEquals);
  ASSERT_EQ(4u, mismatches.size());

  EXPECT_TRUE(mismatches[0].EqualsForTesting(
      MismatchedHttpRequestHeader("name2", std::nullopt, "value1")));
  EXPECT_TRUE(mismatches[1].EqualsForTesting(
      MismatchedHttpRequestHeader("name5", "value1", std::nullopt)));
  EXPECT_TRUE(mismatches[2].EqualsForTesting(
      MismatchedHttpRequestHeader("name7", std::nullopt, "value3")));
  EXPECT_TRUE(mismatches[3].EqualsForTesting(
      MismatchedHttpRequestHeader("name8", std::nullopt, "value3")));
}

TEST(HeadersMatcherTest, MismatchedHttpRequestHeader_OnlyInActual) {
  net::HttpRequestHeaders headers1;

  net::HttpRequestHeaders headers2;
  headers2.SetHeader("name1", "value1");
  headers2.SetHeader("name2", "value2");
  headers2.SetHeader("name3", "value3");

  {
    auto mismatches = MatchHttpRequestHeaders(
        headers1, headers2, MatchHttpRequestHeadersValueOption::kEquals);
    ASSERT_EQ(3u, mismatches.size());

    EXPECT_TRUE(mismatches[0].EqualsForTesting(
        MismatchedHttpRequestHeader("name1", std::nullopt, "value1")));
    EXPECT_TRUE(mismatches[1].EqualsForTesting(
        MismatchedHttpRequestHeader("name2", std::nullopt, "value2")));
    EXPECT_TRUE(mismatches[2].EqualsForTesting(
        MismatchedHttpRequestHeader("name3", std::nullopt, "value3")));
  }

  {
    ShouldIgnoreHelper should_ignore({"name1", "name3"});
    auto mismatches_with_should_ignore = MatchHttpRequestHeaders(
        headers1, headers2, MatchHttpRequestHeadersValueOption::kEquals,
        should_ignore);
    ASSERT_EQ(1u, mismatches_with_should_ignore.size());
    should_ignore.VerifyCallCounts();

    EXPECT_TRUE(mismatches_with_should_ignore[0].EqualsForTesting(
        MismatchedHttpRequestHeader("name2", std::nullopt, "value2")));
  }
}

TEST(HeadersMatcherTest, MismatchedHttpRequestHeader_OnlyInExpected) {
  net::HttpRequestHeaders headers1;
  headers1.SetHeader("name1", "value1");
  headers1.SetHeader("name2", "value2");
  headers1.SetHeader("name3", "value3");

  net::HttpRequestHeaders headers2;

  {
    auto mismatches = MatchHttpRequestHeaders(
        headers1, headers2, MatchHttpRequestHeadersValueOption::kEquals);
    ASSERT_EQ(3u, mismatches.size());

    EXPECT_TRUE(mismatches[0].EqualsForTesting(
        MismatchedHttpRequestHeader("name1", "value1", std::nullopt)));
    EXPECT_TRUE(mismatches[1].EqualsForTesting(
        MismatchedHttpRequestHeader("name2", "value2", std::nullopt)));
    EXPECT_TRUE(mismatches[2].EqualsForTesting(
        MismatchedHttpRequestHeader("name3", "value3", std::nullopt)));
  }

  {
    ShouldIgnoreHelper should_ignore({"name2"});
    auto mismatches_with_should_ignore = MatchHttpRequestHeaders(
        headers1, headers2, MatchHttpRequestHeadersValueOption::kEquals,
        should_ignore);
    ASSERT_EQ(2u, mismatches_with_should_ignore.size());
    should_ignore.VerifyCallCounts();

    EXPECT_TRUE(mismatches_with_should_ignore[0].EqualsForTesting(
        MismatchedHttpRequestHeader("name1", "value1", std::nullopt)));
    EXPECT_TRUE(mismatches_with_should_ignore[1].EqualsForTesting(
        MismatchedHttpRequestHeader("name3", "value3", std::nullopt)));
  }
}

}  // namespace
}  // namespace network
