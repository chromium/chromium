// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/header_util.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
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

TEST(HeaderUtilTest, ContainsForbiddenSecurityHeader) {
  net::HttpRequestHeaders headers;

  // Normal case
  headers.SetHeader("Sec-CH-UA", "Normal Value");
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  std::string value;
  auto value_opt = headers.GetHeader("Sec-CH-UA");
  ASSERT_TRUE(value_opt.has_value());
  value = *value_opt;
  EXPECT_EQ(value, "Normal Value");

  // Truncation case
  std::string long_value(2000, 'a');
  headers.SetHeader("Sec-CH-UA-Long", long_value);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  value_opt = headers.GetHeader("Sec-CH-UA-Long");
  ASSERT_TRUE(value_opt.has_value());
  value = *value_opt;
  EXPECT_EQ(value.length(), 1024u);
  EXPECT_EQ(value, long_value.substr(0, 1024));

  // Boundary cases for Sec-CH-
  std::string value_1023(1023, 'a');
  headers.SetHeader("Sec-CH-UA-1023", value_1023);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(*headers.GetHeader("Sec-CH-UA-1023"), value_1023);

  std::string value_1024(1024, 'a');
  headers.SetHeader("Sec-CH-UA-1024", value_1024);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(*headers.GetHeader("Sec-CH-UA-1024"), value_1024);

  std::string value_1025(1025, 'a');
  headers.SetHeader("Sec-CH-UA-1025", value_1025);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(headers.GetHeader("Sec-CH-UA-1025")->length(), 1024u);

  // Non-Sec-CH- header should not be truncated even if long
  headers.SetHeader("X-Custom-Header", long_value);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  value_opt = headers.GetHeader("X-Custom-Header");
  ASSERT_TRUE(value_opt.has_value());
  value = *value_opt;
  EXPECT_EQ(value.length(), 2000u);

  // Sec-Shared-Storage-Data-Origin boundary cases
  net::HttpRequestHeaders origin_headers;
  std::string origin_267(267, 'a');
  origin_headers.SetHeader("Sec-Shared-Storage-Data-Origin", origin_267);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(origin_headers));

  std::string origin_268(268, 'a');
  origin_headers.SetHeader("Sec-Shared-Storage-Data-Origin", origin_268);
  EXPECT_TRUE(ContainsForbiddenSecurityHeader(origin_headers));

  // Forbidden Sec- header
  headers.SetHeader("Sec-Invalid", "value");
  EXPECT_TRUE(ContainsForbiddenSecurityHeader(headers));

  // Sec-Fetch- headers should be forbidden by default
  net::HttpRequestHeaders fetch_headers;
  fetch_headers.SetHeader("Sec-Fetch-Site", "same-origin");
  EXPECT_TRUE(ContainsForbiddenSecurityHeader(fetch_headers));

  // Sec-GPC cases
  net::HttpRequestHeaders gpc_headers;
  gpc_headers.SetHeader("Sec-GPC", "1");
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(gpc_headers));

  gpc_headers.SetHeader("Sec-GPC", "0");
  EXPECT_TRUE(ContainsForbiddenSecurityHeader(gpc_headers));

  gpc_headers.SetHeader("Sec-GPC", "2");
  EXPECT_TRUE(ContainsForbiddenSecurityHeader(gpc_headers));
}

TEST(HeaderUtilTest,
     ContainsForbiddenSecurityHeader_SecSpeculationTags_Truncation) {
  net::HttpRequestHeaders headers;

  // Normal case (< 2048)
  std::string normal_value = "tag1,tag2";
  headers.SetHeader("Sec-Speculation-Tags", normal_value);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  auto value_opt = headers.GetHeader("Sec-Speculation-Tags");
  ASSERT_TRUE(value_opt.has_value());
  EXPECT_EQ(*value_opt, normal_value);

  // Truncation case with comma
  std::string long_value = std::string(2000, 'a') + "," + std::string(100, 'b');
  // total size = 2000 + 1 + 100 = 2101 > 2048
  headers.SetHeader("Sec-Speculation-Tags", long_value);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  value_opt = headers.GetHeader("Sec-Speculation-Tags");
  ASSERT_TRUE(value_opt.has_value());
  std::string value = *value_opt;
  EXPECT_LE(value.length(), 2048u);
  // It should be truncated at the last comma before 2048.
  // 2000 'a's + 1 comma = 2001 bytes. The next is 'b'.
  // So it should truncate at the comma.
  EXPECT_EQ(value, std::string(2000, 'a'));

  // Truncation case without comma (fallback to 2048)
  std::string very_long_tag(2500, 'c');
  headers.SetHeader("Sec-Speculation-Tags", very_long_tag);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  value_opt = headers.GetHeader("Sec-Speculation-Tags");
  ASSERT_TRUE(value_opt.has_value());
  value = *value_opt;
  EXPECT_EQ(value.length(), 2048u);
  EXPECT_EQ(value, very_long_tag.substr(0, 2048));

  // Boundary cases for Sec-Speculation-Tags
  std::string value_2047(2047, 'a');
  headers.SetHeader("Sec-Speculation-Tags", value_2047);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(*headers.GetHeader("Sec-Speculation-Tags"), value_2047);

  std::string value_2048(2048, 'a');
  headers.SetHeader("Sec-Speculation-Tags", value_2048);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(*headers.GetHeader("Sec-Speculation-Tags"), value_2048);

  std::string value_2049(2049, 'a');
  headers.SetHeader("Sec-Speculation-Tags", value_2049);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(headers.GetHeader("Sec-Speculation-Tags")->length(), 2048u);

  // Boundary cases with comma
  std::string comma_at_2047 = std::string(2047, 'a') + ",b";
  headers.SetHeader("Sec-Speculation-Tags", comma_at_2047);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(*headers.GetHeader("Sec-Speculation-Tags"), std::string(2047, 'a'));

  std::string comma_at_2048 = std::string(2048, 'a') + ",b";
  headers.SetHeader("Sec-Speculation-Tags", comma_at_2048);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(*headers.GetHeader("Sec-Speculation-Tags"), std::string(2048, 'a'));
}

TEST(HeaderUtilTest, ContainsForbiddenSecurityHeader_SecAdAuction_Truncation) {
  net::HttpRequestHeaders headers;

  // Normal case (< 2048)
  std::string normal_value = "auction-signal-data";
  headers.SetHeader("Sec-Ad-Auction-Signals", normal_value);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  auto value_opt = headers.GetHeader("Sec-Ad-Auction-Signals");
  ASSERT_TRUE(value_opt.has_value());
  EXPECT_EQ(*value_opt, normal_value);

  // Truncation case
  std::string long_value(2500, 'a');
  headers.SetHeader("Sec-Ad-Auction-Signals", long_value);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  value_opt = headers.GetHeader("Sec-Ad-Auction-Signals");
  ASSERT_TRUE(value_opt.has_value());
  std::string value = *value_opt;
  EXPECT_EQ(value.length(), 2048u);
  EXPECT_EQ(value, long_value.substr(0, 2048));

  // Boundary cases for Sec-Ad-Auction-Signals
  std::string value_2047(2047, 'a');
  headers.SetHeader("Sec-Ad-Auction-Signals", value_2047);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(*headers.GetHeader("Sec-Ad-Auction-Signals"), value_2047);

  std::string value_2048(2048, 'a');
  headers.SetHeader("Sec-Ad-Auction-Signals", value_2048);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(*headers.GetHeader("Sec-Ad-Auction-Signals"), value_2048);

  std::string value_2049(2049, 'a');
  headers.SetHeader("Sec-Ad-Auction-Signals", value_2049);
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));
  EXPECT_EQ(headers.GetHeader("Sec-Ad-Auction-Signals")->length(), 2048u);

  // Sec-Ad-Auction-Fetch should still be strictly checked
  headers.SetHeader("Sec-Ad-Auction-Fetch", "?1");
  EXPECT_FALSE(ContainsForbiddenSecurityHeader(headers));

  headers.SetHeader("Sec-Ad-Auction-Fetch", "?0");
  EXPECT_TRUE(ContainsForbiddenSecurityHeader(headers));
}

}  // namespace network
