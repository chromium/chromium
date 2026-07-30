// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/web_transport_http_request_headers_mojom_traits.h"

#include <utility>
#include <vector>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/http_request_headers.mojom.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

using HeaderVector = std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>;

network::mojom::WebTransportHttpRequestHeadersPtr MakeInput(std::string key,
                                                            std::string value) {
  auto input = network::mojom::WebTransportHttpRequestHeaders::New();
  input->headers.push_back(network::mojom::HttpRequestHeaderKeyValuePair::New(
      std::move(key), std::move(value)));
  return input;
}

bool RoundTrip(network::mojom::WebTransportHttpRequestHeadersPtr input,
               HeaderVector& output) {
  return mojo::test::SerializeAndDeserialize<
      network::mojom::WebTransportHttpRequestHeaders>(input, output);
}

TEST(WebTransportHttpRequestHeadersTraitsTest, BenignHeaderRoundTrips) {
  auto input = MakeInput("x-custom", "value");
  HeaderVector output;
  ASSERT_TRUE(RoundTrip(std::move(input), output));
  ASSERT_EQ(output.size(), 1u);
  EXPECT_EQ(output[0].key, "x-custom");
  EXPECT_EQ(output[0].value, "value");
}

TEST(WebTransportHttpRequestHeadersTraitsTest, EmptyRoundTrips) {
  auto input = network::mojom::WebTransportHttpRequestHeaders::New();
  HeaderVector output;
  ASSERT_TRUE(RoundTrip(std::move(input), output));
  EXPECT_TRUE(output.empty());
}

TEST(WebTransportHttpRequestHeadersTraitsTest, PreservesDuplicateKeys) {
  auto input = network::mojom::WebTransportHttpRequestHeaders::New();
  input->headers.push_back(
      network::mojom::HttpRequestHeaderKeyValuePair::New("x-foo", "a"));
  input->headers.push_back(
      network::mojom::HttpRequestHeaderKeyValuePair::New("x-foo", "b"));
  HeaderVector output;
  ASSERT_TRUE(RoundTrip(std::move(input), output));
  ASSERT_EQ(output.size(), 2u);
  EXPECT_EQ(output[0].key, "x-foo");
  EXPECT_EQ(output[0].value, "a");
  EXPECT_EQ(output[1].key, "x-foo");
  EXPECT_EQ(output[1].value, "b");
}

TEST(WebTransportHttpRequestHeadersTraitsTest, RejectsInvalidHeaderName) {
  // RFC 7230 §3.2.6: header names must be tokens.
  auto input = MakeInput("foo bar", "v");
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

TEST(WebTransportHttpRequestHeadersTraitsTest, RejectsInvalidHeaderValueCRLF) {
  auto input = MakeInput("x-foo", "v\r\nInjection");
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

TEST(WebTransportHttpRequestHeadersTraitsTest, RejectsInvalidHeaderValueNUL) {
  auto input = MakeInput("x-foo", std::string("v\0bad", 5));
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

TEST(WebTransportHttpRequestHeadersTraitsTest, RejectsWtAvailableProtocols) {
  auto input = MakeInput("wt-available-protocols", "a");
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

TEST(WebTransportHttpRequestHeadersTraitsTest,
     RejectsWtAvailableProtocolsCaseInsensitive) {
  auto input = MakeInput("Wt-AVAILABLE-protocols", "a");
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

TEST(WebTransportHttpRequestHeadersTraitsTest, RejectsForbiddenCookie) {
  // https://fetch.spec.whatwg.org/#forbidden-request-header
  auto input = MakeInput("Cookie", "a=b");
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

TEST(WebTransportHttpRequestHeadersTraitsTest, RejectsSetCookie) {
  // https://fetch.spec.whatwg.org/#forbidden-request-header
  auto input = MakeInput("Set-Cookie", "a=b");
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

TEST(WebTransportHttpRequestHeadersTraitsTest, RejectsProxyPrefix) {
  auto input = MakeInput("Proxy-Authorization", "Bearer x");
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

TEST(WebTransportHttpRequestHeadersTraitsTest, RejectsSecPrefix) {
  auto input = MakeInput("Sec-Fetch-Mode", "navigate");
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

TEST(WebTransportHttpRequestHeadersTraitsTest, RejectsAnyBadInList) {
  auto input = network::mojom::WebTransportHttpRequestHeaders::New();
  input->headers.push_back(
      network::mojom::HttpRequestHeaderKeyValuePair::New("x-ok", "v"));
  input->headers.push_back(
      network::mojom::HttpRequestHeaderKeyValuePair::New("Cookie", "a=b"));
  HeaderVector output;
  EXPECT_FALSE(RoundTrip(std::move(input), output));
}

}  // namespace
}  // namespace mojo
