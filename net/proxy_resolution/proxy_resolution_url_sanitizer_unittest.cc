// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolution_url_sanitizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

TEST(ProxyResolutionUrlSanitizerTest, SanitizeUrlForProxyResolution) {
  struct {
    const char* raw_url;
    const char* sanitized_url;
  } kTests[] = {
      // Credentials and the fragment are stripped from all URLs.
      {"http://user:pass@example.test/path?query#ref",
       "http://example.test/path?query"},
      {"ftp://user:pass@example.test/path?query#ref",
       "ftp://example.test/path?query"},
      // The path and query are additionally stripped from cryptographic
      // schemes.
      {"https://user:pass@example.test:8080/path?query#ref",
       "https://example.test:8080/"},
      {"wss://user:pass@example.test/path?query#ref", "wss://example.test/"},
      // Already-sanitized URLs are unchanged.
      {"http://example.test/", "http://example.test/"},
      {"https://example.test/", "https://example.test/"},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.raw_url);
    EXPECT_EQ(GURL(test.sanitized_url),
              SanitizeUrlForProxyResolution(GURL(test.raw_url)));
  }
}

}  // namespace
}  // namespace net
