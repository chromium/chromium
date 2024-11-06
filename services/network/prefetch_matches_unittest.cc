// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/prefetch_matches.h"

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

TEST(PrefetchMatchesTest, EmptyMatchesEmpty) {
  EXPECT_TRUE(PrefetchMatches(ResourceRequest(), ResourceRequest()));
}

// We do not test every field, as we assume if the generic logic works for one
// field it will work for another.

TEST(PrefetchMatchesTest, Method) {
  ResourceRequest prefetch;
  ResourceRequest real;
  prefetch.method = "GET";
  real.method = "POST";
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, URL) {
  ResourceRequest prefetch;
  ResourceRequest real;
  prefetch.url = GURL("https://example.com/a");
  real.url = GURL("https://example.com/a");
  EXPECT_TRUE(PrefetchMatches(prefetch, real));
  real.url = GURL("https://example.com/b");
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, SiteForCookies) {
  ResourceRequest prefetch;
  ResourceRequest real;
  prefetch.site_for_cookies =
      net::SiteForCookies::FromUrl(GURL("https://example.com/"));
  real.site_for_cookies = net::SiteForCookies();
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, RequestInitiator) {
  ResourceRequest prefetch;
  ResourceRequest real;
  prefetch.request_initiator =
      url::Origin::Create(GURL("https://example.com/"));
  real.request_initiator = url::Origin::Create(GURL("http://example.com/"));
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, ReferrerPolicy) {
  ResourceRequest prefetch;
  ResourceRequest real;
  real.referrer_policy = net::ReferrerPolicy::ORIGIN;
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, HeadersPurposeDiffers) {
  ResourceRequest prefetch;
  ResourceRequest real;
  prefetch.headers.AddHeadersFromString(
      "User-Agent: Mozilla/1.0\r\n"
      "Purpose: prefetch\r\n"
      "Referer: https://www.example.com/\r\n");
  real.headers.AddHeadersFromString(
      "User-Agent: Mozilla/1.0\r\n"
      "Referer: https://www.example.com/\r\n");
  EXPECT_TRUE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, HeadersOrderDoesntMatter) {
  ResourceRequest prefetch;
  ResourceRequest real;
  prefetch.headers.AddHeadersFromString(
      "User-Agent: Mozilla/1.0\r\n"
      "Purpose: prefetch\r\n"
      "Referer: https://www.example.com/\r\n");
  real.headers.AddHeadersFromString(
      "Referer: https://www.example.com/\r\n"
      "User-Agent: Mozilla/1.0\r\n");
  EXPECT_TRUE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, HeadersOriginDiffers) {
  ResourceRequest prefetch;
  ResourceRequest real;
  prefetch.headers.AddHeadersFromString(
      "User-Agent: Mozilla/1.0\r\n"
      "Purpose: prefetch\r\n"
      "Origin: https://www.example.com/\r\n");
  real.headers.AddHeadersFromString(
      "User-Agent: Mozilla/1.0\r\n"
      "Referer: https://www.example.com/\r\n"
      "Origin: https://www2.example/\r\n");
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, CorsExemptHeadersPurposeDiffers) {
  ResourceRequest prefetch;
  ResourceRequest real;
  // The "Purpose" header is not ignored when it is a field other than
  // "headers".
  prefetch.cors_exempt_headers.AddHeadersFromString(
      "User-Agent: Mozilla/1.0\r\n"
      "Purpose: prefetch\r\n"
      "Referer: https://www.example.com/\r\n");
  real.cors_exempt_headers.AddHeadersFromString(
      "User-Agent: Mozilla/1.0\r\n"
      "Referer: https://www.example.com/\r\n");
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, RequestBodySameBytes) {
  constexpr char kBytes[] = "Some bytes";
  ResourceRequest prefetch;
  ResourceRequest real;
  // Set the bodies to different objects with the same contents.
  prefetch.request_body =
      ResourceRequestBody::CreateFromBytes(kBytes, sizeof(kBytes));
  real.request_body =
      ResourceRequestBody::CreateFromBytes(kBytes, sizeof(kBytes));
  EXPECT_TRUE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, RequestBodyDifferentBytes) {
  constexpr char kPrefetchBytes[] = "Some bytes";
  constexpr char kRealBytes[] = "Some different bytes";
  ResourceRequest prefetch;
  ResourceRequest real;
  // Set the bodies to different objects with the same contents.
  prefetch.request_body = ResourceRequestBody::CreateFromBytes(
      kPrefetchBytes, sizeof(kPrefetchBytes));
  real.request_body =
      ResourceRequestBody::CreateFromBytes(kRealBytes, sizeof(kRealBytes));
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, RequestBodyDifferentType) {
  constexpr char kBytes[] = "Some bytes";
  ResourceRequest prefetch;
  ResourceRequest real;
  // Set the bodies to different objects with the same contents.
  prefetch.request_body =
      ResourceRequestBody::CreateFromBytes(kBytes, sizeof(kBytes));
  real.request_body = base::MakeRefCounted<ResourceRequestBody>();
  real.request_body->AppendFileRange(base::FilePath::FromASCII("path"), 0, 7,
                                     base::Time::Now());
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

// A ResourceRequestBody consists of zero or more DataElements. If the number is
// different, they should not compare equal.
TEST(PrefetchMatchesTest, RequestBodyDifferentLength) {
  constexpr std::string_view kBytes = "Some bytes";
  constexpr std::string_view kSplit1 = kBytes.substr(0, 5);
  constexpr std::string_view kSplit2 = kBytes.substr(5);
  ResourceRequest prefetch;
  ResourceRequest real;
  // Set the bodies to different objects with the same contents.
  prefetch.request_body =
      ResourceRequestBody::CreateFromBytes(kBytes.data(), kBytes.size());
  real.request_body =
      ResourceRequestBody::CreateFromBytes(kSplit1.data(), kSplit1.size());
  real.request_body->AppendBytes(kSplit2.data(), kSplit2.size());
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, FetchWindowIdIgnored) {
  ResourceRequest prefetch;
  ResourceRequest real;
  real.fetch_window_id = base::UnguessableToken::Create();
  EXPECT_TRUE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, WebBundleTokenParamsMatch) {
  ResourceRequest prefetch;
  ResourceRequest real;
  const GURL bundle_url("https://example.com/bundle");
  const auto token = base::UnguessableToken::Create();
  constexpr int kProcessId1 = 1;
  constexpr int kProcessId2 = 2;
  prefetch.web_bundle_token_params =
      ResourceRequest::WebBundleTokenParams(bundle_url, token, kProcessId1);
  real.web_bundle_token_params =
      ResourceRequest::WebBundleTokenParams(bundle_url, token, kProcessId2);
  EXPECT_TRUE(PrefetchMatches(prefetch, real));
}

TEST(PrefetchMatchesTest, WebBundleTokenParamsMismatch) {
  ResourceRequest prefetch;
  ResourceRequest real;
  const GURL bundle_url("https://example.com/bundle");
  const auto token1 = base::UnguessableToken::Create();
  const auto token2 = base::UnguessableToken::Create();
  constexpr int kProcessId1 = 1;
  constexpr int kProcessId2 = 2;
  prefetch.web_bundle_token_params =
      ResourceRequest::WebBundleTokenParams(bundle_url, token1, kProcessId1);
  real.web_bundle_token_params =
      ResourceRequest::WebBundleTokenParams(bundle_url, token2, kProcessId2);
  EXPECT_FALSE(PrefetchMatches(prefetch, real));
}

}  // namespace

}  // namespace network
