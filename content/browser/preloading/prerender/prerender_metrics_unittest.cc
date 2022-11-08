// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// Tests to ensure the ReportHeaderMismatch implementation is aligned with the
// enum generator.
TEST(PrerenderMetricsTest, NavigationHeaderMismatchMetric) {
  const char kMetricName[] =
      "Prerender.Experimental.ActivationHeadersMismatch.Embedder_ForTesting";
  {
    base::HistogramTester histogram_tester;
    net::HttpRequestHeaders prerender_headers;
    prerender_headers.AddHeadersFromString("Content-Type: ab\r\n If-Match: xy");
    net::HttpRequestHeaders potential_headers;
    potential_headers.AddHeadersFromString("Content-Type: cd\r\n If-Match: xy");
    AnalyzePrerenderActivationHeader(potential_headers, prerender_headers,
                                     PrerenderTriggerType::kEmbedder,
                                     "ForTesting");
    // label="content-type: value mismatch"
    histogram_tester.ExpectUniqueSample(kMetricName, 808179719, 1);
  }
  {
    base::HistogramTester histogram_tester;
    net::HttpRequestHeaders prerender_headers;
    prerender_headers.AddHeadersFromString("Content-Type: ab");
    net::HttpRequestHeaders potential_headers;
    potential_headers.AddHeadersFromString(
        "Content-Type: ab\r\n If-Match: xys");
    AnalyzePrerenderActivationHeader(potential_headers, prerender_headers,
                                     PrerenderTriggerType::kEmbedder,
                                     "ForTesting");
    // label="if-match: missing in prerendering request's headers"
    histogram_tester.ExpectUniqueSample(kMetricName, 667272509, 1);
  }
  {
    base::HistogramTester histogram_tester;
    net::HttpRequestHeaders prerender_headers;
    prerender_headers.AddHeadersFromString("service-worker: xys");
    net::HttpRequestHeaders potential_headers;
    potential_headers.AddHeadersFromString("");
    AnalyzePrerenderActivationHeader(potential_headers, prerender_headers,
                                     PrerenderTriggerType::kEmbedder,
                                     "ForTesting");
    // label="service-worker: missing in activation request's headers"
    histogram_tester.ExpectUniqueSample(kMetricName, -578377770, 1);
  }
  {
    base::HistogramTester histogram_tester;
    net::HttpRequestHeaders prerender_headers;
    prerender_headers.AddHeadersFromString("Content-Type: ab");
    net::HttpRequestHeaders potential_headers;
    potential_headers.AddHeadersFromString("content-type: ab");
    AnalyzePrerenderActivationHeader(potential_headers, prerender_headers,
                                     PrerenderTriggerType::kEmbedder,
                                     "ForTesting");
    // label="unexpected: everything matched"
    histogram_tester.ExpectUniqueSample(kMetricName, 1349923684, 1);
  }
}

}  // namespace
}  // namespace content
