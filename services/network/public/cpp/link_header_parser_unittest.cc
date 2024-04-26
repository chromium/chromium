// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/link_header_parser.h"

#include "base/memory/scoped_refptr.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

const GURL kBaseUrl = GURL("https://example.com/foo?bar=baz");

}  // namespace

TEST(LinkHeaderParserTest, NoLinkHeader) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 0UL);
}

TEST(LinkHeaderParserTest, InvalidValue) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  // The value is invalid because it misses a semicolon after `rel=preload`.
  headers->AddHeader("link", "</script.js>; rel=preload as=script");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 0UL);
}

TEST(LinkHeaderParserTest, UndefinedAttribute) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  // `unknownattr` is not pre-defined.
  headers->AddHeader("link",
                     "</style.css>; rel=preload; as=style; unknownattr");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 0UL);
}

TEST(LinkHeaderParserTest, UndefinedAttributeValue) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "</foo>; rel=preload; as=unknown-as");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 0UL);
}

TEST(LinkHeaderParserTest, UnknownMimeType) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "</foo>; rel=preload; type=unknown-type");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 0UL);
}

TEST(LinkHeaderParserTest, NoRelAttribute) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  // `rel` must be present.
  headers->AddHeader("link", "</foo>");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 0UL);
}

TEST(LinkHeaderParserTest, AttributesAppearTwice) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "</foo>; rel=preload; rel=prefetch");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 1UL);
  // The parser should use the first one.
  EXPECT_EQ(parsed_headers[0]->rel, mojom::LinkRelAttribute::kPreload);

  // TODO(crbug.com/40170852): Add tests for other attributes if the behavior is
  // reasonable.
}

TEST(LinkHeaderParserTest, RelAttributeModulePreload) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "</foo.mjs>; rel=modulepreload");
  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 1UL);
  EXPECT_EQ(parsed_headers[0]->rel, mojom::LinkRelAttribute::kModulePreload);
}

TEST(LinkHeaderParserTest, RelAttributeDnsPrefetch) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "<https://cdn.example.com>; rel=dns-prefetch");
  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 1UL);
  EXPECT_EQ(parsed_headers[0]->href, GURL("https://cdn.example.com"));
  EXPECT_EQ(parsed_headers[0]->rel, mojom::LinkRelAttribute::kDnsPrefetch);
}

TEST(LinkHeaderParserTest, RelAttributePreconnect) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "<https://cdn.example.com>; rel=preconnect");
  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 1UL);
  EXPECT_EQ(parsed_headers[0]->href, GURL("https://cdn.example.com"));
  EXPECT_EQ(parsed_headers[0]->rel, mojom::LinkRelAttribute::kPreconnect);
}

TEST(LinkHeaderParserTest, LinkAsAttribute) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "</foo>; rel=preload");
  headers->AddHeader("link", "</font.woff2>; rel=preload; as=font");
  headers->AddHeader("link", "</image.jpg>; rel=preload; as=image");
  headers->AddHeader("link", "</script.js>; rel=preload; as=script");
  headers->AddHeader("link", "</style.css>; rel=preload; as=style");
  headers->AddHeader("link", "</foo.json>; rel=preload; as=fetch; crossorigin");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 6UL);
  EXPECT_EQ(parsed_headers[0]->as, mojom::LinkAsAttribute::kUnspecified);
  EXPECT_EQ(parsed_headers[1]->as, mojom::LinkAsAttribute::kFont);
  EXPECT_EQ(parsed_headers[2]->as, mojom::LinkAsAttribute::kImage);
  EXPECT_EQ(parsed_headers[3]->as, mojom::LinkAsAttribute::kScript);
  EXPECT_EQ(parsed_headers[4]->as, mojom::LinkAsAttribute::kStyleSheet);
  EXPECT_EQ(parsed_headers[5]->as, mojom::LinkAsAttribute::kFetch);
}

TEST(LinkHeaderParserTest, CrossOriginAttribute) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "<https://cross.example.com/>; rel=preload");
  headers->AddHeader("link",
                     "<https://cross.example.com/>; rel=preload; crossorigin");
  headers->AddHeader(
      "link",
      "<https://cross.example.com/>; rel=preload; crossorigin=anonymous");
  headers->AddHeader(
      "link",
      "<https://cross.example.com/>; rel=preload; crossorigin=use-credentials");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 4UL);
  EXPECT_EQ(parsed_headers[0]->cross_origin,
            mojom::CrossOriginAttribute::kUnspecified);
  EXPECT_EQ(parsed_headers[1]->cross_origin,
            mojom::CrossOriginAttribute::kAnonymous);
  EXPECT_EQ(parsed_headers[2]->cross_origin,
            mojom::CrossOriginAttribute::kAnonymous);
  EXPECT_EQ(parsed_headers[3]->cross_origin,
            mojom::CrossOriginAttribute::kUseCredentials);
}

TEST(LinkHeaderParserTest, FetchPriorityAttribute) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "</default>; rel=preload");
  headers->AddHeader("link", "</auto>; rel=preload; fetchpriority=auto");
  headers->AddHeader("link", "</high>; rel=preload; fetchpriority=high");
  headers->AddHeader("link", "</low>; rel=preload; fetchpriority=low");
  headers->AddHeader("link", "</invalid>; rel=preload; fetchpriority=invalid");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 5UL);
  EXPECT_EQ(parsed_headers[0]->fetch_priority,
            mojom::FetchPriorityAttribute::kAuto);
  EXPECT_EQ(parsed_headers[1]->fetch_priority,
            mojom::FetchPriorityAttribute::kAuto);
  EXPECT_EQ(parsed_headers[2]->fetch_priority,
            mojom::FetchPriorityAttribute::kHigh);
  EXPECT_EQ(parsed_headers[3]->fetch_priority,
            mojom::FetchPriorityAttribute::kLow);
  EXPECT_EQ(parsed_headers[4]->fetch_priority,
            mojom::FetchPriorityAttribute::kAuto);
}

TEST(LinkHeaderParserTest, TwoHeaders) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link", "</image.jpg>; rel=preload; as=image");
  headers->AddHeader("link",
                     "<https://cross.example.com/font.woff2>; rel=preload; "
                     "as=font; crossorigin; type=font/woff2");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 2UL);

  EXPECT_EQ(parsed_headers[0]->href, kBaseUrl.Resolve("/image.jpg"));
  EXPECT_EQ(parsed_headers[0]->rel, mojom::LinkRelAttribute::kPreload);
  EXPECT_EQ(parsed_headers[0]->as, mojom::LinkAsAttribute::kImage);
  EXPECT_EQ(parsed_headers[0]->cross_origin,
            mojom::CrossOriginAttribute::kUnspecified);
  EXPECT_FALSE(parsed_headers[0]->mime_type.has_value());

  EXPECT_EQ(parsed_headers[1]->href,
            GURL("https://cross.example.com/font.woff2"));
  EXPECT_EQ(parsed_headers[1]->rel, mojom::LinkRelAttribute::kPreload);
  EXPECT_EQ(parsed_headers[1]->as, mojom::LinkAsAttribute::kFont);
  EXPECT_EQ(parsed_headers[1]->cross_origin,
            mojom::CrossOriginAttribute::kAnonymous);
  EXPECT_EQ(parsed_headers[1]->mime_type, "font/woff2");
}

TEST(LinkHeaderParserTest, UpperCaseCharacters) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("link",
                     "</image.jpg>; REL=preload; as=IMAGE; fetchpriority=HIGH");
  headers->AddHeader("link",
                     "<https://cross.example.com/font.woff2>; rel=PRELOAD; "
                     "AS=font; CROSSORIGIN=USE-CREDENTIALS; TYPE=font/woff2; "
                     "FETCHPRIORITY=low");

  std::vector<mojom::LinkHeaderPtr> parsed_headers =
      ParseLinkHeaders(*headers, kBaseUrl);
  ASSERT_EQ(parsed_headers.size(), 2UL);

  EXPECT_EQ(parsed_headers[0]->href, kBaseUrl.Resolve("/image.jpg"));
  EXPECT_EQ(parsed_headers[0]->rel, mojom::LinkRelAttribute::kPreload);
  EXPECT_EQ(parsed_headers[0]->as, mojom::LinkAsAttribute::kImage);
  EXPECT_EQ(parsed_headers[0]->fetch_priority,
            mojom::FetchPriorityAttribute::kHigh);

  EXPECT_EQ(parsed_headers[1]->href,
            GURL("https://cross.example.com/font.woff2"));
  EXPECT_EQ(parsed_headers[1]->rel, mojom::LinkRelAttribute::kPreload);
  EXPECT_EQ(parsed_headers[1]->as, mojom::LinkAsAttribute::kFont);
  EXPECT_EQ(parsed_headers[1]->cross_origin,
            mojom::CrossOriginAttribute::kUseCredentials);
  EXPECT_EQ(parsed_headers[1]->mime_type, "font/woff2");
  EXPECT_EQ(parsed_headers[1]->fetch_priority,
            mojom::FetchPriorityAttribute::kLow);
}

}  // namespace network
