// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/origins_list.h"

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace signed_exchange_utils {

TEST(OriginsList, IsEmpty) {
  OriginsList origins_list;
  EXPECT_TRUE(origins_list.IsEmpty());
}

TEST(OriginsList, ExactMatch) {
  OriginsList origins_list(
      "example.com,test.example.com,https://invalid.entry,example.net:1234");

  EXPECT_FALSE(origins_list.IsEmpty());

  static constexpr const char* kShouldMatchList[] = {
      "https://example.com", "https://test.example.com",
      "https://example.net:1234",
  };

  for (const char* should_match : kShouldMatchList) {
    EXPECT_TRUE(origins_list.Match(url::Origin::Create(GURL(should_match))))
        << "OriginList should match url: " << should_match;
  }

  static constexpr const char* kShouldNotMatchList[] = {
      "http://example.com",     "https://subdomain.example.com",
      "https://notexample.com", "https://invalid.entry",
      "https://example.net",    "https://example.net:5432",
  };

  for (const char* should_not_match : kShouldNotMatchList) {
    EXPECT_FALSE(
        origins_list.Match(url::Origin::Create(GURL(should_not_match))))
        << "OriginList should not match url: " << should_not_match;
  }
}

TEST(OriginsList, SubdomainMatch) {
  OriginsList origins_list("*.example.com,*.example.net:1234");

  EXPECT_FALSE(origins_list.IsEmpty());

  static constexpr const char* kShouldMatchList[] = {
      "https://example.com", "https://test.example.com",
      "https://test.test2.example.com", "https://test.example.net:1234",
  };

  for (const char* should_match : kShouldMatchList) {
    EXPECT_TRUE(origins_list.Match(url::Origin::Create(GURL(should_match))))
        << "OriginList should match url: " << should_match;
  }

  static constexpr const char* kShouldNotMatchList[] = {
      "http://example.com", "https://notexample.com",
      "https://test.example.net", "https://test.example.net:5432",
  };

  for (const char* should_not_match : kShouldNotMatchList) {
    EXPECT_FALSE(
        origins_list.Match(url::Origin::Create(GURL(should_not_match))))
        << "OriginList should not match url: " << should_not_match;
  }
}

}  // namespace signed_exchange_utils
}  // namespace content
