// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/opaque_response_blocking.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

// ResourceType::kImage from resource_load_info.mojom
constexpr mojom::RequestDestination kDefaultRequestDestination =
    mojom::RequestDestination::kImage;

const char kDefaultRequestUrl[] = "https://target.example.com/foo";

struct TestInput {
  GURL request_url;
  base::Optional<url::Origin> request_initiator;
  mojom::RequestMode request_mode;
  mojom::RequestDestination request_destination;
  mojom::URLResponseHeadPtr response;
};

class TestInputBuilder {
 public:
  TestInputBuilder() = default;

  // No copy constructor or assignment operator.
  TestInputBuilder(const TestInputBuilder&) = delete;
  TestInputBuilder& operator=(const TestInputBuilder&) = delete;

  TestInput Build() const {
    std::string raw_headers = base::ReplaceStringPlaceholders(
        "$1\nContent-Type: $2\n", {http_status_line_, mime_type_}, nullptr);
    if (no_sniff_)
      raw_headers += "X-Content-Type-Options: nosniff\n";

    mojom::URLResponseHeadPtr response = mojom::URLResponseHead::New();
    response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(raw_headers));
    response->was_fetched_via_service_worker = false;
    response->response_type = mojom::FetchResponseType::kDefault;

    return TestInput{
        .request_url = request_url_,
        .request_initiator = request_initiator_,
        .request_mode = request_mode_,
        .request_destination = kDefaultRequestDestination,
        .response = std::move(response),
    };
  }

  TestInputBuilder& WithHttpStatus(std::string status_line) {
    http_status_line_ = status_line;
    return *this;
  }

  TestInputBuilder& WithMimeType(std::string mime_type) {
    mime_type_ = mime_type;
    return *this;
  }

  TestInputBuilder& WithInitiator(
      const base::Optional<url::Origin>& initiator) {
    request_initiator_ = initiator;
    return *this;
  }

  TestInputBuilder& WithMode(const mojom::RequestMode& mode) {
    request_mode_ = mode;
    return *this;
  }

  TestInputBuilder& WithNoSniff() {
    no_sniff_ = true;
    return *this;
  }

 private:
  std::string http_status_line_ = "HTTP/1.1 200 OK";
  std::string mime_type_ = "application/octet-stream";
  GURL request_url_ = GURL(kDefaultRequestUrl);
  base::Optional<url::Origin> request_initiator_ =
      url::Origin::Create(GURL("https://initiator.example.com"));
  mojom::RequestMode request_mode_ = mojom::RequestMode::kNoCors;
  bool no_sniff_ = false;
};

void LogUmaForOpaqueResponseBlocking(const TestInput& test_input) {
  network::LogUmaForOpaqueResponseBlocking(
      test_input.request_url, test_input.request_initiator,
      test_input.request_mode, test_input.request_destination,
      *test_input.response);
}

void LogUmaForOpaqueResponseBlocking(
    const TestInputBuilder& test_input_builder) {
  LogUmaForOpaqueResponseBlocking(test_input_builder.Build());
}

void TestUma(const TestInput& test_input,
             const ResponseHeadersHeuristicForUma& expected_uma_decision) {
  base::HistogramTester histograms;
  LogUmaForOpaqueResponseBlocking(test_input);

  // Verify that the ...Heuristic.Decision UMA has only a single/unique bucket
  // (for `expected_uma_decision`) and that only 1 sample has been logged for
  // this bucket.
  histograms.ExpectUniqueSample(
      "SiteIsolation.ORB.ResponseHeadersHeuristic.Decision",
      expected_uma_decision, 1 /* expected_count */);
}

void TestUma(const TestInputBuilder& test_input_builder,
             const ResponseHeadersHeuristicForUma& expected_uma_decision) {
  TestUma(test_input_builder.Build(), expected_uma_decision);
}

TEST(OpaqueResponseBlocking, DefaultTestInput) {
  // Verify properties of the default test input: renderer-initiated,
  // cross-origin, no-cors request with a successful application/octet-stream
  // response.
  TestInput default_test_input = TestInputBuilder().Build();
  EXPECT_TRUE(default_test_input.request_initiator.has_value());
  EXPECT_FALSE(default_test_input.request_initiator->IsSameOriginWith(
      url::Origin::Create(default_test_input.request_url)));
  EXPECT_EQ(mojom::RequestMode::kNoCors, default_test_input.request_mode);
  EXPECT_TRUE(default_test_input.response);
  EXPECT_TRUE(default_test_input.response->headers);
  EXPECT_EQ(200, default_test_input.response->headers->response_code());
  std::string mime_type;
  EXPECT_TRUE(default_test_input.response->headers->GetMimeType(&mime_type));
  EXPECT_EQ("application/octet-stream", mime_type);

  // Verify that the right UMA is logged for the default test input.
  TestUma(default_test_input,
          ResponseHeadersHeuristicForUma::kRequiresJavascriptParsing);
}

TEST(OpaqueResponseBlocking, ResourceTypeUma) {
  // RequiresJavascriptParsing case.
  {
    base::HistogramTester histograms;
    TestUma(TestInputBuilder(),
            ResponseHeadersHeuristicForUma::kRequiresJavascriptParsing);
    histograms.ExpectTotalCount(
        "SiteIsolation.ORB.ResponseHeadersHeuristic.ProcessedBasedOnHeaders",
        0);
    histograms.ExpectUniqueSample(
        "SiteIsolation.ORB.ResponseHeadersHeuristic.RequiresJavascriptParsing",
        kDefaultRequestDestination,
        1 /* `expected_count` of `kDefaultRequestDestination` */);
  }

  // ProcessedBasedOnHeaders case.
  {
    base::HistogramTester histograms;
    TestUma(TestInputBuilder().WithNoSniff(),
            ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
    histograms.ExpectUniqueSample(
        "SiteIsolation.ORB.ResponseHeadersHeuristic.ProcessedBasedOnHeaders",
        kDefaultRequestDestination,
        1 /* `expected_count` of `kDefaultRequestDestination` */);
    histograms.ExpectTotalCount(
        "SiteIsolation.ORB.ResponseHeadersHeuristic.RequiresJavascriptParsing",
        0);
  }

  // Non-opaque case..
  {
    base::HistogramTester histograms;
    LogUmaForOpaqueResponseBlocking(
        TestInputBuilder().WithInitiator(base::nullopt));
    histograms.ExpectTotalCount(
        "SiteIsolation.ORB.ResponseHeadersHeuristic.ProcessedBasedOnHeaders",
        0);
    histograms.ExpectTotalCount(
        "SiteIsolation.ORB.ResponseHeadersHeuristic.RequiresJavascriptParsing",
        0);
  }

  LogUmaForOpaqueResponseBlocking(TestInputBuilder().WithNoSniff());
}

TEST(OpaqueResponseBlocking, NonOpaqueRequests) {
  // Browser-initiated request.
  TestUma(TestInputBuilder().WithInitiator(base::nullopt),
          ResponseHeadersHeuristicForUma::kNonOpaqueResponse);

  // Mode != no-cors.
  TestUma(TestInputBuilder().WithMode(mojom::RequestMode::kCors),
          ResponseHeadersHeuristicForUma::kNonOpaqueResponse);
  TestUma(TestInputBuilder().WithMode(mojom::RequestMode::kNavigate),
          ResponseHeadersHeuristicForUma::kNonOpaqueResponse);
}

TEST(OpaqueResponseBlocking, SameOrigin) {
  TestUma(TestInputBuilder().WithInitiator(
              url::Origin::Create(GURL(kDefaultRequestUrl))),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
}

TEST(OpaqueResponseBlocking, NoSniff) {
  TestUma(TestInputBuilder().WithNoSniff(),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
}

TEST(OpaqueResponseBlocking, MimeTypes) {
  // Unrecognized type:
  TestUma(TestInputBuilder().WithMimeType("application/octet-stream"),
          ResponseHeadersHeuristicForUma::kRequiresJavascriptParsing);

  // opaque-safelisted MIME types:
  TestUma(TestInputBuilder().WithMimeType("application/dash+xml"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
  TestUma(TestInputBuilder().WithMimeType("application/javascript"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
  TestUma(TestInputBuilder().WithMimeType("image/svg+xml"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
  TestUma(TestInputBuilder().WithMimeType("text/css"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);

  // Example of a opaque-blocklisted-never-sniffed MIME type:
  TestUma(TestInputBuilder().WithMimeType("application/pdf"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);

  // Future multimedia types:
  TestUma(TestInputBuilder().WithMimeType("audio/some-future-type"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
  TestUma(TestInputBuilder().WithMimeType("image/some-future-type"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
  TestUma(TestInputBuilder().WithMimeType("video/some-future-type"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
}

TEST(OpaqueResponseBlocking, HttpStatusCodes) {
  TestUma(TestInputBuilder().WithHttpStatus("HTTP/1.1 200 OK"),
          ResponseHeadersHeuristicForUma::kRequiresJavascriptParsing);

  TestUma(TestInputBuilder().WithHttpStatus("HTTP/1.1 206 Range"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
  TestUma(TestInputBuilder().WithHttpStatus("HTTP/1.1 300 Redirect"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
  TestUma(TestInputBuilder().WithHttpStatus("HTTP/1.1 400 NotFound"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
  TestUma(TestInputBuilder().WithHttpStatus("HTTP/1.1 500 ServerError"),
          ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders);
}

}  // namespace
}  // namespace network
