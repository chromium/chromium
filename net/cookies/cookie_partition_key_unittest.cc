// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEY_UNITTEST_H_
#define NET_COOKIES_COOKIE_PARTITION_KEY_UNITTEST_H_

#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(CookiePartitionKeyTest, Serialization) {
  struct {
    absl::optional<CookiePartitionKey> input;
    bool expected_ret;
    std::string expected_output;
  } cases[] = {
      // No partition key
      {absl::nullopt, true, kEmptyCookiePartitionKey},
      // Partition key present
      {absl::make_optional(CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com"))),
       true, "https://toplevelsite.com"},
      // Local file URL
      {absl::make_optional(CookiePartitionKey::FromURLForTesting(
           GURL("file:///path/to/file.txt"))),
       true, "file://"},
      // File URL with host
      {absl::make_optional(CookiePartitionKey::FromURLForTesting(
           GURL("file://toplevelsite.com/path/to/file.pdf"))),
       true, "file://toplevelsite.com"},
      // Opaque origin
      {absl::make_optional(CookiePartitionKey::FromURLForTesting(GURL())),
       false, ""},
      // Invalid partition key
      {absl::make_optional(
           CookiePartitionKey::FromURLForTesting(GURL("abc123foobar!!"))),
       false, ""},
  };

  for (const auto& tc : cases) {
    std::string got;
    EXPECT_EQ(tc.expected_ret, CookiePartitionKey::Serialize(tc.input, got));
    EXPECT_EQ(tc.expected_output, got);
  }
}

TEST(CookiePartitionKeyTest, Deserialization) {
  struct {
    std::string input;
    bool expected_ret;
    absl::optional<CookiePartitionKey> expected_output;
  } cases[] = {
      {kEmptyCookiePartitionKey, true, absl::nullopt},
      {"https://toplevelsite.com", true,
       absl::make_optional(CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com")))},
      {"abc123foobar!!", false, absl::nullopt},
  };

  for (const auto& tc : cases) {
    absl::optional<CookiePartitionKey> got;
    EXPECT_EQ(tc.expected_ret, CookiePartitionKey::Deserialize(tc.input, got));
    if (tc.expected_output.has_value()) {
      EXPECT_TRUE(got.has_value());
      EXPECT_EQ(tc.expected_output.value(), got.value());
    } else {
      EXPECT_FALSE(got.has_value());
    }
  }
}

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEY_UNITTEST_H_
