// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

TEST(URLUtilTest, SerializeResponseUrlForReporting) {
  struct {
    const char* input;
    const char* expected;
  } kTestCases[] = {
      {"https://example.com/", "https://example.com/"},
      {"https://example.com/#fragment", "https://example.com/"},
      {"https://user:pass@example.com/", "https://example.com/"},
      {"https://user:pass@example.com/#fragment", "https://example.com/"},
      {"https://example.com/?query", "https://example.com/?query"},
      {"https://example.com/?query#fragment", "https://example.com/?query"},
      {"http://example.com/", "http://example.com/"},
      {"http://example.com/#fragment", "http://example.com/"},
      {"http://user:pass@example.com/", "http://example.com/"},
      {"http://user:pass@example.com/#fragment", "http://example.com/"},
      {"data:text/html,<html></html>", "data:text/html,<html></html>"},
      {"data:text/html,<html></html>#fragment", "data:text/html,<html></html>"},
      {"blob:https://example.com/uuid", "blob:https://example.com/uuid"},
      {"blob:https://example.com/uuid#fragment",
       "blob:https://example.com/uuid"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(SerializeResponseUrlForReporting(GURL(test_case.input)).spec(),
              test_case.expected)
        << "For input: " << test_case.input;
  }
}

}  // namespace network
