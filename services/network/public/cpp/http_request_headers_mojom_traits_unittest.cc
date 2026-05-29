// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/http_request_headers_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/http_request_headers.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(HttpRequestHeadersMojomTraitsTest, HeaderKeyValuePair) {
  net::HttpRequestHeaders::HeaderKeyValuePair in;
  in.key = "Content-Type";
  in.value = "text/plain";

  net::HttpRequestHeaders::HeaderKeyValuePair out;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::HttpRequestHeaderKeyValuePair>(
          in, out));

  EXPECT_EQ(in.key, out.key);
  EXPECT_EQ(in.value, out.value);
}

TEST(HttpRequestHeadersMojomTraitsTest, HeaderKeyValuePairInvalidKey) {
  net::HttpRequestHeaders::HeaderKeyValuePair in;
  in.key = "Invalid Key";
  in.value = "text/plain";

  net::HttpRequestHeaders::HeaderKeyValuePair out;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::HttpRequestHeaderKeyValuePair>(
          in, out));
}

TEST(HttpRequestHeadersMojomTraitsTest, HeaderKeyValuePairInvalidValue) {
  net::HttpRequestHeaders::HeaderKeyValuePair in;
  in.key = "Content-Type";
  in.value = "Invalid\nValue";

  net::HttpRequestHeaders::HeaderKeyValuePair out;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::HttpRequestHeaderKeyValuePair>(
          in, out));
}

TEST(HttpRequestHeadersMojomTraitsTest, HttpRequestHeaders) {
  net::HttpRequestHeaders in;
  in.SetHeader("Content-Type", "text/plain");
  in.SetHeader("Content-Length", "10");
  in.SetHeader("X-Test-Header", "test");

  net::HttpRequestHeaders out;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::HttpRequestHeaders>(in, out));

  EXPECT_EQ(in.GetHeaderVector(), out.GetHeaderVector());
}

TEST(HttpRequestHeadersMojomTraitsTest, HttpRequestHeadersInvalidKey) {
  mojom::HttpRequestHeadersPtr in = mojom::HttpRequestHeaders::New();
  auto pair = mojom::HttpRequestHeaderKeyValuePair::New();
  pair->key = "Invalid Key";
  pair->value = "valid-value";
  in->headers.push_back(std::move(pair));

  net::HttpRequestHeaders out;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::HttpRequestHeaders>(in, out));
}

TEST(HttpRequestHeadersMojomTraitsTest, HttpRequestHeadersInvalidValue) {
  mojom::HttpRequestHeadersPtr in = mojom::HttpRequestHeaders::New();
  auto pair = mojom::HttpRequestHeaderKeyValuePair::New();
  pair->key = "Valid-Key";
  pair->value = "Invalid\nValue";
  in->headers.push_back(std::move(pair));

  net::HttpRequestHeaders out;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::HttpRequestHeaders>(in, out));
}

TEST(HttpRequestHeadersMojomTraitsTest, HttpRequestHeadersUpdateParams) {
  HttpRequestHeadersUpdateParams in;
  in.removed_headers = {"Content-Length"};
  in.modified_headers.SetHeader("Content-Type", "text/html");
  in.modified_cors_exempt_headers.SetHeader("X-Cors-Exempt", "true");

  HttpRequestHeadersUpdateParams out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::HttpRequestHeadersUpdateParams>(in, out));

  EXPECT_EQ(in.removed_headers, out.removed_headers);
  EXPECT_EQ(in.modified_headers.GetHeaderVector(),
            out.modified_headers.GetHeaderVector());
  EXPECT_EQ(in.modified_cors_exempt_headers.GetHeaderVector(),
            out.modified_cors_exempt_headers.GetHeaderVector());
}

}  // namespace
}  // namespace network
