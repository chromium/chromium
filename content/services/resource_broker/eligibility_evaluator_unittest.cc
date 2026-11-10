// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/resource_broker/eligibility_evaluator.h"

#include <cstdint>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

scoped_refptr<net::HttpResponseHeaders> MakeHeaders(std::string_view raw) {
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw));
}

network::mojom::URLResponseHeadPtr MakeEligibleHead() {
  auto head = network::mojom::URLResponseHead::New();
  head->headers =
      MakeHeaders("HTTP/1.1 200 OK\r\nCache-Control: max-age=3600\r\n\r\n");
  return head;
}

// Evaluates a `200 OK` response with only `header_lines` and no request auth.
uint32_t EvaluateHeader(std::string_view header_lines) {
  network::mojom::URLResponseHead head;
  head.headers = MakeHeaders(
      base::StrCat({"HTTP/1.1 200 OK\r\n", header_lines, "\r\n\r\n"}));
  return EvaluateHeadEligibility(head,
                                 /*request_has_authorization_header=*/false);
}

// Evaluates a fresh (`max-age=3600`) `200 OK` response with `extra_headers`.
uint32_t EvaluateFreshHeader(std::string_view extra_headers) {
  return EvaluateHeader(
      base::StrCat({"Cache-Control: max-age=3600\r\n", extra_headers}));
}

TEST(EligibilityEvaluatorTest, PlainCacheableResponseIsEligible) {
  network::mojom::URLResponseHeadPtr head = MakeEligibleHead();
  EXPECT_EQ(EvaluateHeadEligibility(*head, false), kEligible);
  EXPECT_EQ(
      EvaluateEligibility(*head, false, /*body_size=*/2 * kMinEligibleBodySize),
      kEligible);
}

TEST(EligibilityEvaluatorTest, ResponseTypes) {
  network::mojom::URLResponseHeadPtr head = MakeEligibleHead();

  head->response_type = network::mojom::FetchResponseType::kOpaque;
  EXPECT_EQ(EvaluateHeadEligibility(*head, false), kEligible);

  head->response_type = network::mojom::FetchResponseType::kCors;
  EXPECT_EQ(EvaluateHeadEligibility(*head, false), kEligible);

  head->response_type = network::mojom::FetchResponseType::kOpaqueRedirect;
  EXPECT_EQ(EvaluateHeadEligibility(*head, false), kInvalidResponseType);

  head->response_type = network::mojom::FetchResponseType::kError;
  EXPECT_EQ(EvaluateHeadEligibility(*head, false), kInvalidResponseType);
}

TEST(EligibilityEvaluatorTest, BodySizeThreshold) {
  network::mojom::URLResponseHeadPtr head = MakeEligibleHead();
  EXPECT_EQ(EvaluateHeadEligibility(*head, false), kEligible);
  EXPECT_EQ(EvaluateBodyEligibility(kMinEligibleBodySize - 1), kBelowMinSize);
  EXPECT_EQ(EvaluateBodyEligibility(kMinEligibleBodySize), kEligible);
  EXPECT_EQ(EvaluateEligibility(*head, false, kMinEligibleBodySize - 1),
            kBelowMinSize);
  EXPECT_EQ(EvaluateEligibility(*head, false, kMinEligibleBodySize), kEligible);
}

TEST(EligibilityEvaluatorTest, PartialContent) {
  network::mojom::URLResponseHeadPtr range_request = MakeEligibleHead();
  range_request->has_range_requested = true;
  EXPECT_EQ(EvaluateHeadEligibility(*range_request, false), kPartialContent);

  network::mojom::URLResponseHeadPtr partial_response = MakeEligibleHead();
  partial_response->headers = MakeHeaders(
      "HTTP/1.1 206 Partial Content\r\nCache-Control: max-age=60\r\n\r\n");
  EXPECT_EQ(EvaluateHeadEligibility(*partial_response, false),
            kPartialContent | kNotCacheableStable);
}

TEST(EligibilityEvaluatorTest, NonOkOrMissingHeaders) {
  network::mojom::URLResponseHead no_headers;
  EXPECT_EQ(EvaluateHeadEligibility(no_headers, false), kNotCacheableStable);

  network::mojom::URLResponseHeadPtr not_modified = MakeEligibleHead();
  not_modified->headers = MakeHeaders("HTTP/1.1 304 Not Modified\r\n\r\n");
  EXPECT_EQ(EvaluateHeadEligibility(*not_modified, false), kNotCacheableStable);
}

TEST(EligibilityEvaluatorTest, CacheControlAndExpiresHeaders) {
  EXPECT_EQ(EvaluateHeader("Cache-Control: private, max-age=3600"), kEligible);
  EXPECT_EQ(EvaluateHeader("Cache-Control: max-age=3600\r\nExpires: 0"),
            kEligible);
  // Freshness is `Expires - Date` rather than relative to wall-clock `Now()`.
  EXPECT_EQ(EvaluateHeader("Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n"
                           "Expires: Sun, 06 Nov 1994 09:49:37 GMT"),
            kEligible);
  EXPECT_EQ(EvaluateHeader("Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n"
                           "Last-Modified: Sat, 05 Nov 1994 08:49:37 GMT"),
            kEligible);

  // Bare `200 OK` without freshness headers requires revalidation.
  EXPECT_EQ(EvaluateHeader(""), kNotCacheableStable);
  EXPECT_EQ(EvaluateHeader("Cache-Control: no-store"), kNotCacheableStable);
  EXPECT_EQ(EvaluateHeader("Cache-Control: no-cache"), kNotCacheableStable);
  EXPECT_EQ(EvaluateHeader("Cache-Control: private, max-age=0"),
            kNotCacheableStable);
  EXPECT_EQ(EvaluateHeader("Cache-Control: max-age=00"), kNotCacheableStable);
  EXPECT_EQ(EvaluateHeader("Cache-Control: max-age= 0"), kNotCacheableStable);
  EXPECT_EQ(EvaluateHeader("Cache-Control: private\r\nExpires: 0"),
            kNotCacheableStable);
  EXPECT_EQ(EvaluateHeader("Expires: -1"), kNotCacheableStable);
  EXPECT_EQ(EvaluateHeader("Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n"
                           "Expires: Thu, 01 Jan 1970 00:00:00 GMT"),
            kNotCacheableStable);
  EXPECT_EQ(EvaluateHeader("Pragma: no-cache"), kNotCacheableStable);
}

TEST(EligibilityEvaluatorTest, VaryHeaders) {
  EXPECT_EQ(
      EvaluateFreshHeader("Vary: accept-encoding, Accept, Origin, "
                          "Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site"),
      kEligible);
  EXPECT_EQ(EvaluateFreshHeader("Vary: Accept-Encoding, "), kEligible);
  EXPECT_EQ(EvaluateFreshHeader("Vary: "), kEligible);

  EXPECT_EQ(EvaluateFreshHeader("Vary: *"), kNotCacheableStable);
  EXPECT_EQ(EvaluateFreshHeader("Vary: Cookie"), kNotCacheableStable);
  EXPECT_EQ(EvaluateFreshHeader("Vary: Accept-Encoding, Cookie"),
            kNotCacheableStable);
}

TEST(EligibilityEvaluatorTest, AuthenticationIsNotStable) {
  network::mojom::URLResponseHeadPtr head = MakeEligibleHead();
  EXPECT_EQ(
      EvaluateHeadEligibility(*head, /*request_has_authorization_header=*/true),
      kNotCacheableStable);

  head->did_use_server_http_auth = true;
  EXPECT_EQ(EvaluateHeadEligibility(*head,
                                    /*request_has_authorization_header=*/false),
            kNotCacheableStable);

  EXPECT_EQ(EvaluateFreshHeader("WWW-Authenticate: Basic"),
            kNotCacheableStable);
  EXPECT_EQ(EvaluateFreshHeader("Proxy-Authenticate: Basic"),
            kNotCacheableStable);
}

TEST(EligibilityEvaluatorTest, ReasonsCombine) {
  network::mojom::URLResponseHead head;
  head.response_type = network::mojom::FetchResponseType::kError;
  head.headers = MakeHeaders("HTTP/1.1 206 Partial Content\r\n\r\n");
  EXPECT_EQ(EvaluateEligibility(head, false, /*body_size=*/1),
            kInvalidResponseType | kPartialContent | kNotCacheableStable |
                kBelowMinSize);
}

}  // namespace

}  // namespace content
