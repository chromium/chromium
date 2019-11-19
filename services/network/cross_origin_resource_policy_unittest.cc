// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/cross_origin_resource_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

CrossOriginResourcePolicy::ParsedHeader ParseHeader(
    const std::string& test_headers) {
  std::string all_headers = "HTTP/1.1 200 OK\n" + test_headers + "\n";
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(all_headers));
  return CrossOriginResourcePolicy::ParseHeaderForTesting(headers.get());
}

// This test is somewhat redundant with
// wpt/fetch/cross-origin-resource-policy/syntax.any.js
// The delta in coverage is mostly around testing case insensitivity of the
// header name.
TEST(CrossOriginResourcePolicyTest, ParseHeader) {
  // Basic tests.
  EXPECT_EQ(CrossOriginResourcePolicy::kNoHeader, ParseHeader(""));
  EXPECT_EQ(CrossOriginResourcePolicy::kSameOrigin,
            ParseHeader("Cross-Origin-Resource-Policy: same-origin"));
  EXPECT_EQ(CrossOriginResourcePolicy::kSameSite,
            ParseHeader("Cross-Origin-Resource-Policy: same-site"));

  // Header names are case-insensitive.
  EXPECT_EQ(CrossOriginResourcePolicy::kSameOrigin,
            ParseHeader("Cross-Origin-RESOURCE-Policy: same-origin"));
  EXPECT_EQ(CrossOriginResourcePolicy::kSameSite,
            ParseHeader("Cross-ORIGIN-Resource-Policy: same-site"));

  // Header values are case-sensitive.
  EXPECT_EQ(CrossOriginResourcePolicy::kParsingError,
            ParseHeader("Cross-Origin-Resource-Policy: sAme-origin"));
  EXPECT_EQ(CrossOriginResourcePolicy::kParsingError,
            ParseHeader("Cross-Origin-Resource-Policy: saMe-site"));

  // Specific origins are not yet part of the spec.  See also:
  // https://github.com/whatwg/fetch/issues/760
  EXPECT_EQ(
      CrossOriginResourcePolicy::kParsingError,
      ParseHeader("Cross-Origin-Resource-Policy: https://www.example.com"));

  // Parsing failures explicitly called out in the note for step 3:
  // https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  //
  //   > This means that `Cross-Origin-Resource-Policy: same-site, same-origin`
  //   > ends up as allowed below as it will never match anything. Two or more
  //   > `Cross-Origin-Resource-Policy` headers will have the same effect.
  //
  EXPECT_EQ(
      CrossOriginResourcePolicy::kParsingError,
      ParseHeader("Cross-Origin-Resource-Policy: same-site, same-origin"));
  EXPECT_EQ(CrossOriginResourcePolicy::kParsingError,
            ParseHeader("Cross-Origin-Resource-Policy: same-site\n"
                        "Cross-Origin-Resource-Policy: same-origin"));
}

TEST(CrossOriginResourcePolicyTest, CrossSiteHeaderWithCOEP) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCrossOriginIsolation);
  EXPECT_EQ(CrossOriginResourcePolicy::kCrossOrigin,
            ParseHeader("Cross-Origin-Resource-Policy: cross-origin"));
}

TEST(CrossOriginResourcePolicyTest, CrossSiteHeaderWithoutCOEP) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kCrossOriginIsolation);
  EXPECT_EQ(CrossOriginResourcePolicy::kParsingError,
            ParseHeader("Cross-Origin-Resource-Policy: cross-origin"));
}

bool ShouldAllowSameSite(const std::string& initiator,
                         const std::string& target) {
  return CrossOriginResourcePolicy::ShouldAllowSameSiteForTesting(
      url::Origin::Create(GURL(initiator)), url::Origin::Create(GURL(target)));
}

TEST(CrossOriginResourcePolicyTest, ShouldAllowSameSite) {
  // Basic tests.
  EXPECT_TRUE(ShouldAllowSameSite("https://foo.com", "https://foo.com"));
  EXPECT_FALSE(ShouldAllowSameSite("https://foo.com", "https://bar.com"));

  // Subdomains.
  EXPECT_TRUE(ShouldAllowSameSite("https://foo.a.com", "https://a.com"));
  EXPECT_TRUE(ShouldAllowSameSite("https://a.com", "https://bar.a.com"));
  EXPECT_TRUE(ShouldAllowSameSite("https://foo.a.com", "https://bar.a.com"));
  EXPECT_FALSE(ShouldAllowSameSite("https://foo.a.com", "https://b.com"));
  EXPECT_FALSE(ShouldAllowSameSite("https://a.com", "https://bar.b.com"));
  EXPECT_FALSE(ShouldAllowSameSite("https://foo.a.com", "https://bar.b.com"));

  // Same host, different HTTPS vs HTTP scheme.
  //
  // The intent here is that HTTPS response shouldn't be exposed to a page
  // served over HTTP (which might leak it), but serving an HTTP response isn't
  // secret so we don't need to block it from HTTPS pages.  This behavior should
  // hopefully help with adoption on sites that still need to use mixed
  // http/https content.
  EXPECT_TRUE(ShouldAllowSameSite(
      /* initiator = */ "https://foo.com", /* target = */ "http://foo.com"));
  EXPECT_FALSE(ShouldAllowSameSite(
      /* initiator = */ "http://foo.com", /* target = */ "https://foo.com"));

  // IP addresses.
  //
  // Different sites might be served from the same IP address - they should
  // still be considered to be different sites - see also
  // https://url.spec.whatwg.org/#host-same-site which excludes IP addresses by
  // imposing the requirement that one of the addresses has to have a non-null
  // registrable domain.
  EXPECT_FALSE(ShouldAllowSameSite("http://127.0.0.1", "http://127.0.0.1"));
}

TEST(CrossOriginResourcePolicyTest, WithCOEP) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCrossOriginIsolation);

  ResourceResponseInfo corp_none;
  ResourceResponseInfo corp_same_origin;
  ResourceResponseInfo corp_cross_origin;

  corp_same_origin.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 200 OK\n"
          "cross-origin-resource-policy: same-origin\n"));

  corp_cross_origin.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 200 OK\n"
          "cross-origin-resource-policy: cross-origin\n"));

  GURL destination("https://www.example.com/");

  url::Origin destination_origin =
      url::Origin::Create(GURL("https://www.example.com"));
  url::Origin another_origin =
      url::Origin::Create(GURL("https://www2.example.com"));

  constexpr auto kAllow = CrossOriginResourcePolicy::kAllow;
  constexpr auto kBlock = CrossOriginResourcePolicy::kBlock;
  using mojom::RequestMode;

  struct TestCase {
    const RequestMode request_mode;
    const url::Origin origin;
    const ResourceResponseInfo response_info;
    const CrossOriginResourcePolicy::VerificationResult
        expectation_with_coep_none;
    const CrossOriginResourcePolicy::VerificationResult
        expectation_with_coep_require_corp;
  } test_cases[] = {
      // We don't have a cross-origin-resource-policy header on a response. That
      // leads to kBlock when COEP: kRequireCorp is used.
      {RequestMode::kNoCors, another_origin, corp_none, kAllow, kBlock},
      // We have "cross-origin-resource-policy: same-origin", so regardless of
      // COEP the response is blocked.
      {RequestMode::kNoCors, another_origin, corp_same_origin, kBlock, kBlock},
      // We have "cross-origin-resource-policy: cross-origin", so regardless of
      // COEP the response is allowed.
      {RequestMode::kNoCors, another_origin, corp_cross_origin, kAllow, kAllow},
      // The origin of the request URL and request's origin match, so regardless
      // of COEP the response is allowed.
      {RequestMode::kNoCors, destination_origin, corp_same_origin, kAllow,
       kAllow},
      // The request mode is "cors", so so regardless of COEP the response is
      // allowed.
      {RequestMode::kCors, another_origin, corp_same_origin, kAllow, kAllow},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expectation_with_coep_none,
              CrossOriginResourcePolicy::Verify(
                  destination, test_case.origin, test_case.response_info,
                  test_case.request_mode, test_case.origin,
                  mojom::CrossOriginEmbedderPolicy::kNone));

    EXPECT_EQ(test_case.expectation_with_coep_require_corp,
              CrossOriginResourcePolicy::Verify(
                  destination, test_case.origin, test_case.response_info,
                  test_case.request_mode, test_case.origin,
                  mojom::CrossOriginEmbedderPolicy::kRequireCorp));
  }
}
}  // namespace network
