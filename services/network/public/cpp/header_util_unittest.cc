// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/header_util.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(HeaderUtilTest, IsRequestHeaderSafe) {
  const struct HeaderKeyValuePair {
    const char* key;
    const char* value;
    bool is_safe;
  } kHeaders[] = {
      {"foo", "bar", true},

      {net::HttpRequestHeaders::kContentLength, "42", false},
      {net::HttpRequestHeaders::kHost, "foo.test", false},
      {"Trailer", "header-names", false},
      {"Upgrade", "websocket", false},
      {"Upgrade", "webbedsocket", false},
      {"hOsT", "foo.test", false},

      {net::HttpRequestHeaders::kConnection, "Upgrade", false},
      {net::HttpRequestHeaders::kConnection, "Close", true},
      {net::HttpRequestHeaders::kTransferEncoding, "Chunked", false},
      {net::HttpRequestHeaders::kTransferEncoding, "Chunky", false},
      {"cOnNeCtIoN", "uPgRaDe", false},

      {net::HttpRequestHeaders::kProxyAuthorization,
       "Basic Zm9vOmJhcg==", false},
      {"Proxy-Foo", "bar", false},
      {"PrOxY-FoO", "bar", false},

      {"dnt", "1", true},
  };

  for (const auto& header : kHeaders) {
    SCOPED_TRACE(header.key);
    SCOPED_TRACE(header.value);
    EXPECT_EQ(header.is_safe, IsRequestHeaderSafe(header.key, header.value));
  }
}

TEST(HeaderUtilTest, AreRequestHeadersSafe) {
  const struct HeaderKeyValuePair {
    const char* key;
    const char* value;
    bool is_safe;
  } kHeaders[] = {
      {"foo", "bar", true},

      {net::HttpRequestHeaders::kContentLength, "42", false},
      {net::HttpRequestHeaders::kHost, "foo.test", false},
      {"hOsT", "foo.test", false},
      {"Trailer", "header-names", false},
      {"Te", "deflate", false},
      {"Upgrade", "websocket", false},
      {"Upgrade", "webbedsocket", false},
      {"Cookie2", "tastiness=5", false},
      {"Keep-Alive", "timeout=5, max=1000", false},
      {net::HttpRequestHeaders::kTransferEncoding, "gzip", false},
      {"Set-Cookie", "foo=bar", false},

      {net::HttpRequestHeaders::kConnection, "Upgrade", false},
      {net::HttpRequestHeaders::kConnection, "Close", true},
      {net::HttpRequestHeaders::kTransferEncoding, "Chunked", false},
      {net::HttpRequestHeaders::kTransferEncoding, "Chunky", false},
      {"cOnNeCtIoN", "uPgRaDe", false},

      {net::HttpRequestHeaders::kProxyAuthorization,
       "Basic Zm9vOmJhcg==", false},
      {"Proxy-Foo", "bar", false},
      {"PrOxY-FoO", "bar", false},

      {"dnt", "1", true},
  };

  // Check each header in isolation, and all combinations of two header
  // key/value pairs that have different keys.
  for (const auto& header1 : kHeaders) {
    net::HttpRequestHeaders request_headers1;
    request_headers1.SetHeader(header1.key, header1.value);
    EXPECT_EQ(header1.is_safe, AreRequestHeadersSafe(request_headers1));

    for (const auto& header2 : kHeaders) {
      if (base::EqualsCaseInsensitiveASCII(header1.key, header2.key))
        continue;
      SCOPED_TRACE(header1.key);
      SCOPED_TRACE(header1.value);
      SCOPED_TRACE(header2.key);
      SCOPED_TRACE(header2.value);

      net::HttpRequestHeaders request_headers2;
      request_headers2.SetHeader(header1.key, header1.value);
      request_headers2.SetHeader(header2.key, header2.value);
      EXPECT_EQ(header1.is_safe && header2.is_safe,
                AreRequestHeadersSafe(request_headers2));
    }
  }
}

TEST(HeaderUtilTest, ParseReferrerPolicy) {
  struct TestCase {
    const char* referrer_policy_value;
    mojom::ReferrerPolicy expected_referrer_policy;
  };
  const TestCase kTests[] = {
      {"", mojom::ReferrerPolicy::kDefault},
      {"no-referrer", mojom::ReferrerPolicy::kNever},
      {"no-referrer-when-downgrade",
       mojom::ReferrerPolicy::kNoReferrerWhenDowngrade},
      {"origin", mojom::ReferrerPolicy::kOrigin},
      {"origin-when-cross-origin",
       mojom::ReferrerPolicy::kOriginWhenCrossOrigin},
      {"unsafe-url", mojom::ReferrerPolicy::kAlways},
      {"same-origin", mojom::ReferrerPolicy::kSameOrigin},
      {"strict-origin", mojom::ReferrerPolicy::kStrictOrigin},
      {"strict-origin-when-cross-origin",
       mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin},
      // Unknown value.
      {"unknown-value", mojom::ReferrerPolicy::kDefault},
      // Multiple values.
      {"no-referrer,unsafe-url", mojom::ReferrerPolicy::kAlways},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(::testing::Message() << "referrer_policy_value: \""
                                      << test.referrer_policy_value << "\"");

    auto headers = net::HttpResponseHeaders::TryToCreate(base::StringPrintf(
        "HTTP/1.1 200 OK\r\nReferrer-Policy: %s", test.referrer_policy_value));
    ASSERT_TRUE(headers);

    mojom::ReferrerPolicy parsed_policy = ParseReferrerPolicy(*headers);
    EXPECT_EQ(parsed_policy, test.expected_referrer_policy);
  }
}

}  // namespace network
