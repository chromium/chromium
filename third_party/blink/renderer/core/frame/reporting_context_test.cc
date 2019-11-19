// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/reporting_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"

namespace blink {

class ReportingContextTest : public testing::Test {
 protected:
  ReportingContextTest() = default;

  ~ReportingContextTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReportingContextTest);
};

TEST_F(ReportingContextTest, CountQueuedReports) {
  HistogramTester tester;
  auto dummy_page_holder = std::make_unique<DummyPageHolder>();
  tester.ExpectTotalCount("Blink.UseCounter.Features.DeprecationReport", 0);
  // Checking the feature state with reporting intent should record a potential
  // violation.
  DeprecationReportBody* body = MakeGarbageCollected<DeprecationReportBody>(
      "FeatureId", 2e9, "Test report");
  Report* report = MakeGarbageCollected<Report>(
      "deprecation", dummy_page_holder->GetDocument().Url().GetString(), body);

  // Send the deprecation report to the Reporting API and any
  // ReportingObservers.
  ReportingContext::From(&dummy_page_holder->GetDocument())
      ->QueueReport(report);
  //  tester.ExpectTotalCount("Blink.UseCounter.Features.DeprecationReport", 1);
  // The potential violation for an already recorded violation does not count
  // again.
}

}  // namespace blink
