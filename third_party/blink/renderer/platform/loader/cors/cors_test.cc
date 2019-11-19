// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/cors/cors.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

class CorsExposedHeadersTest : public testing::Test {
 public:
  using CredentialsMode = network::mojom::CredentialsMode;

  WebHTTPHeaderSet Parse(CredentialsMode credentials_mode,
                         const AtomicString& header) const {
    ResourceResponse response;
    response.AddHttpHeaderField("access-control-expose-headers", header);

    return cors::ExtractCorsExposedHeaderNamesList(credentials_mode, response);
  }
};

TEST_F(CorsExposedHeadersTest, ValidInput) {
  EXPECT_EQ(Parse(CredentialsMode::kOmit, "valid"),
            WebHTTPHeaderSet({"valid"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a,b"), WebHTTPHeaderSet({"a", "b"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "   a ,  b "),
            WebHTTPHeaderSet({"a", "b"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, " \t   \t\t a"),
            WebHTTPHeaderSet({"a"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a , "), WebHTTPHeaderSet({"a", ""}));
}

TEST_F(CorsExposedHeadersTest, DuplicatedEntries) {
  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a, a"), WebHTTPHeaderSet{"a"});

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a, a, b"),
            WebHTTPHeaderSet({"a", "b"}));
}

TEST_F(CorsExposedHeadersTest, InvalidInput) {
  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "not valid").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "///").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "/a/").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, ",").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, " , ").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, " , a").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, " ").empty());

  // U+0141 which is 'A' (0x41) + 0x100.
  EXPECT_TRUE(
      Parse(CredentialsMode::kOmit, AtomicString(String::FromUTF8("\xC5\x81")))
          .empty());
}

TEST_F(CorsExposedHeadersTest, Wildcard) {
  ResourceResponse response;
  response.AddHttpHeaderField("access-control-expose-headers", "a, b, *");
  response.AddHttpHeaderField("b", "-");
  response.AddHttpHeaderField("c", "-");
  response.AddHttpHeaderField("d", "-");
  response.AddHttpHeaderField("*", "-");

  EXPECT_EQ(
      cors::ExtractCorsExposedHeaderNamesList(CredentialsMode::kOmit, response),
      WebHTTPHeaderSet({"access-control-expose-headers", "b", "c", "d", "*"}));

  EXPECT_EQ(
      cors::ExtractCorsExposedHeaderNamesList(CredentialsMode::kSameOrigin,
                                              response),
      WebHTTPHeaderSet({"access-control-expose-headers", "b", "c", "d", "*"}));
}

TEST_F(CorsExposedHeadersTest, Asterisk) {
  ResourceResponse response;
  response.AddHttpHeaderField("access-control-expose-headers", "a, b, *");
  response.AddHttpHeaderField("b", "-");
  response.AddHttpHeaderField("c", "-");
  response.AddHttpHeaderField("d", "-");
  response.AddHttpHeaderField("*", "-");

  EXPECT_EQ(cors::ExtractCorsExposedHeaderNamesList(CredentialsMode::kInclude,
                                                    response),
            WebHTTPHeaderSet({"a", "b", "*"}));
}

// Keep this in sync with the CalculateResponseTainting test in
// services/network/cors/cors_url_loader_unittest.cc.
TEST(CorsTest, CalculateResponseTainting) {
  using network::mojom::FetchResponseType;
  using network::mojom::RequestMode;

  const KURL same_origin_url("https://example.com/");
  const KURL cross_origin_url("https://example2.com/");
  scoped_refptr<SecurityOrigin> origin_refptr =
      SecurityOrigin::Create(same_origin_url);
  const SecurityOrigin* origin = origin_refptr.get();
  const SecurityOrigin* no_origin = nullptr;

  // CORS flag is false, same-origin request
  EXPECT_EQ(
      FetchResponseType::kBasic,
      cors::CalculateResponseTainting(same_origin_url, RequestMode::kSameOrigin,
                                      origin, nullptr, CorsFlag::Unset));
  EXPECT_EQ(
      FetchResponseType::kBasic,
      cors::CalculateResponseTainting(same_origin_url, RequestMode::kNoCors,
                                      origin, nullptr, CorsFlag::Unset));
  EXPECT_EQ(FetchResponseType::kBasic,
            cors::CalculateResponseTainting(same_origin_url, RequestMode::kCors,
                                            origin, nullptr, CorsFlag::Unset));
  EXPECT_EQ(FetchResponseType::kBasic,
            cors::CalculateResponseTainting(
                same_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                nullptr, CorsFlag::Unset));
  EXPECT_EQ(
      FetchResponseType::kBasic,
      cors::CalculateResponseTainting(same_origin_url, RequestMode::kNavigate,
                                      origin, nullptr, CorsFlag::Unset));

  // CORS flag is false, cross-origin request
  EXPECT_EQ(
      FetchResponseType::kOpaque,
      cors::CalculateResponseTainting(cross_origin_url, RequestMode::kNoCors,
                                      origin, nullptr, CorsFlag::Unset));
  EXPECT_EQ(
      FetchResponseType::kBasic,
      cors::CalculateResponseTainting(cross_origin_url, RequestMode::kNavigate,
                                      origin, nullptr, CorsFlag::Unset));

  // CORS flag is true, same-origin request
  EXPECT_EQ(FetchResponseType::kCors,
            cors::CalculateResponseTainting(same_origin_url, RequestMode::kCors,
                                            origin, nullptr, CorsFlag::Set));
  EXPECT_EQ(FetchResponseType::kCors,
            cors::CalculateResponseTainting(
                same_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                nullptr, CorsFlag::Set));

  // CORS flag is true, cross-origin request
  EXPECT_EQ(FetchResponseType::kCors, cors::CalculateResponseTainting(
                                          cross_origin_url, RequestMode::kCors,
                                          origin, nullptr, CorsFlag::Set));
  EXPECT_EQ(FetchResponseType::kCors,
            cors::CalculateResponseTainting(
                cross_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                nullptr, CorsFlag::Set));

  // Origin is not provided.
  EXPECT_EQ(
      FetchResponseType::kBasic,
      cors::CalculateResponseTainting(same_origin_url, RequestMode::kNoCors,
                                      no_origin, nullptr, CorsFlag::Unset));
  EXPECT_EQ(
      FetchResponseType::kBasic,
      cors::CalculateResponseTainting(same_origin_url, RequestMode::kNavigate,
                                      no_origin, nullptr, CorsFlag::Unset));
  EXPECT_EQ(
      FetchResponseType::kBasic,
      cors::CalculateResponseTainting(cross_origin_url, RequestMode::kNoCors,
                                      no_origin, nullptr, CorsFlag::Unset));
  EXPECT_EQ(
      FetchResponseType::kBasic,
      cors::CalculateResponseTainting(cross_origin_url, RequestMode::kNavigate,
                                      no_origin, nullptr, CorsFlag::Unset));
}

}  // namespace

}  // namespace blink
