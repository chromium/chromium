// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/header_util.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"
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

}  // namespace network
