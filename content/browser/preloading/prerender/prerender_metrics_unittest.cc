// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

void TestHeaders(const std::string& prerender_header_str,
                 const std::string& potential_header_str) {
  PrerenderCancellationReason reason = PrerenderCancellationReason::
      CreateCandidateReasonForActivationParameterMismatch();
  net::HttpRequestHeaders prerender_headers;
  prerender_headers.AddHeadersFromString(prerender_header_str);
  net::HttpRequestHeaders potential_headers;
  potential_headers.AddHeadersFromString(potential_header_str);
  ASSERT_FALSE(PrerenderHost::IsActivationHeaderMatch(
      potential_headers, prerender_headers, reason));
  reason.ReportMetrics(".ForTesting");
}

// Tests to ensure the ReportHeaderMismatch implementation is aligned with the
// enum generator.
TEST(PrerenderMetricsTest, NavigationHeaderMismatchMetric) {
  const char kMetricName[] =
      "Prerender.Experimental.ActivationHeadersMismatch.ForTesting";
  {
    base::HistogramTester histogram_tester;
    TestHeaders("Content-Type: ab\r\n If-Match: xy",
                "Content-Type: cd\r\n If-Match: xy");
    // label="content-type: value mismatch"
    histogram_tester.ExpectUniqueSample(kMetricName, 808179719, 1);
  }
  {
    base::HistogramTester histogram_tester;
    TestHeaders("Content-Type: ab", "Content-Type: ab\r\n If-Match: xys");
    // label="if-match: missing in prerendering request's headers"
    histogram_tester.ExpectUniqueSample(kMetricName, 667272509, 1);
  }
  {
    base::HistogramTester histogram_tester;
    TestHeaders("service-worker: xyz", "");
    // label="service-worker: missing in activation request's headers"
    histogram_tester.ExpectUniqueSample(kMetricName, -578377770, 1);
  }
}

}  // namespace
}  // namespace content
