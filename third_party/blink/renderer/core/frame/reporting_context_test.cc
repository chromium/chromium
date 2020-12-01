// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/reporting_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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

class MockReportingServiceProxy : public mojom::blink::ReportingServiceProxy {
  using ReportingServiceProxy = mojom::blink::ReportingServiceProxy;

 public:
  MockReportingServiceProxy(BrowserInterfaceBrokerProxy& broker,
                            base::OnceClosure reached_callback)
      : broker_(broker), reached_callback_(std::move(reached_callback)) {
    broker_.SetBinderForTesting(
        ReportingServiceProxy::Name_,
        WTF::BindRepeating(&MockReportingServiceProxy::BindReceiver,
                           WTF::Unretained(this)));
  }

  ~MockReportingServiceProxy() override {
    broker_.SetBinderForTesting(ReportingServiceProxy::Name_, {});
  }

  base::Optional<base::Time> DeprecationReportAnticipatedRemoval() const {
    return deprecation_report_anticipated_removal_;
  }

 private:
  void BindReceiver(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(
        this, mojo::PendingReceiver<ReportingServiceProxy>(std::move(handle)));
  }

  void QueueDeprecationReport(const KURL& url,
                              const String& id,
                              base::Optional<base::Time> anticipated_removal,
                              const String& message,
                              const String& source_file,
                              int32_t line_number,
                              int32_t column_number) override {
    deprecation_report_anticipated_removal_ = anticipated_removal;

    if (reached_callback_)
      std::move(reached_callback_).Run();
  }

  void QueueInterventionReport(const KURL& url,
                               const String& id,
                               const String& message,
                               const String& source_file,
                               int32_t line_number,
                               int32_t column_number) override {
    if (reached_callback_)
      std::move(reached_callback_).Run();
  }

  void QueueCspViolationReport(const KURL& url,
                               const String& group,
                               const String& document_url,
                               const String& referrer,
                               const String& blocked_url,
                               const String& effective_directive,
                               const String& original_policy,
                               const String& source_file,
                               const String& script_sample,
                               const String& disposition,
                               uint16_t status_code,
                               int32_t line_number,
                               int32_t column_number) override {
    if (reached_callback_)
      std::move(reached_callback_).Run();
  }

  void QueueFeaturePolicyViolationReport(const KURL& url,
                                         const String& policy_id,
                                         const String& disposition,
                                         const String& message,
                                         const String& source_file,
                                         int32_t line_number,
                                         int32_t column_number) override {
    if (reached_callback_)
      std::move(reached_callback_).Run();
  }

  void QueueDocumentPolicyViolationReport(const KURL& url,
                                          const String& endpoint,
                                          const String& policy_id,
                                          const String& disposition,
                                          const String& message,
                                          const String& source_file,
                                          int32_t line_number,
                                          int32_t column_number) override {
    if (reached_callback_)
      std::move(reached_callback_).Run();
  }

  BrowserInterfaceBrokerProxy& broker_;
  mojo::ReceiverSet<ReportingServiceProxy> receivers_;
  base::OnceClosure reached_callback_;

  // Last reported values
  base::Optional<base::Time> deprecation_report_anticipated_removal_;
};

TEST_F(ReportingContextTest, CountQueuedReports) {
  HistogramTester tester;
  auto dummy_page_holder = std::make_unique<DummyPageHolder>();
  tester.ExpectTotalCount("Blink.UseCounter.Features.DeprecationReport", 0);
  // Checking the feature state with reporting intent should record a potential
  // violation.
  DeprecationReportBody* body = MakeGarbageCollected<DeprecationReportBody>(
      "FeatureId", base::Time::FromJsTime(2e9), "Test report");
  Report* report = MakeGarbageCollected<Report>(
      "deprecation", dummy_page_holder->GetDocument().Url().GetString(), body);

  // Send the deprecation report to the Reporting API and any
  // ReportingObservers.
  ReportingContext::From(dummy_page_holder->GetFrame().DomWindow())
      ->QueueReport(report);
  //  tester.ExpectTotalCount("Blink.UseCounter.Features.DeprecationReport", 1);
  // The potential violation for an already recorded violation does not count
  // again.
}

TEST_F(ReportingContextTest, DeprecationReportContent) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>();
  auto* win = dummy_page_holder->GetFrame().DomWindow();
  base::RunLoop run_loop;
  MockReportingServiceProxy reporting_service(win->GetBrowserInterfaceBroker(),
                                              run_loop.QuitClosure());

  auto* body = MakeGarbageCollected<DeprecationReportBody>(
      "FeatureId", base::Time::FromJsTime(1000), "Test report");
  auto* report = MakeGarbageCollected<Report>(
      "deprecation", win->document()->Url().GetString(), body);
  ReportingContext::From(win)->QueueReport(report);
  run_loop.Run();

  EXPECT_TRUE(reporting_service.DeprecationReportAnticipatedRemoval());
  // We had a bug that anticipatedRemoval had a wrong value only in mojo method
  // calls.
  EXPECT_EQ(base::Time::FromJsTime(1000),
            *reporting_service.DeprecationReportAnticipatedRemoval());
}

}  // namespace blink
