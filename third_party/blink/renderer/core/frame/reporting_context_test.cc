// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/reporting_context.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/document_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/permissions_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class ReportingContextTest : public testing::Test {
 public:
  ReportingContextTest(const ReportingContextTest&) = delete;
  ReportingContextTest& operator=(const ReportingContextTest&) = delete;

 protected:
  ReportingContextTest() = default;
  ~ReportingContextTest() override = default;

 private:
  test::TaskEnvironment task_environment_;
};

class MockReportingServiceProxy : public mojom::blink::ReportingServiceProxy {
  using ReportingServiceProxy = mojom::blink::ReportingServiceProxy;

 public:
  MockReportingServiceProxy(const BrowserInterfaceBrokerProxy& broker,
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

  std::optional<base::Time> DeprecationReportAnticipatedRemoval() const {
    return deprecation_report_anticipated_removal_;
  }

  const String& LastMessage() const { return last_message_; }

 private:
  void BindReceiver(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(
        this, mojo::PendingReceiver<ReportingServiceProxy>(std::move(handle)));
  }

  void QueueDeprecationReport(const KURL& url,
                              const String& id,
                              std::optional<base::Time> anticipated_removal,
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

  void QueuePermissionsPolicyViolationReport(const KURL& url,
                                             const String& endpoint,
                                             const String& policy_id,
                                             const String& disposition,
                                             const String& message,
                                             const String& source_file,
                                             int32_t line_number,
                                             int32_t column_number) override {
    last_message_ = message;
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
    last_message_ = message;
    if (reached_callback_)
      std::move(reached_callback_).Run();
  }

  const BrowserInterfaceBrokerProxy& broker_;
  mojo::ReceiverSet<ReportingServiceProxy> receivers_;
  base::OnceClosure reached_callback_;

  // Last reported values
  std::optional<base::Time> deprecation_report_anticipated_removal_;

  // Last reported report's message.
  String last_message_;
};

TEST_F(ReportingContextTest, CountQueuedReports) {
  base::HistogramTester tester;
  auto dummy_page_holder = std::make_unique<DummyPageHolder>();
  tester.ExpectTotalCount("Blink.UseCounter.Features.DeprecationReport", 0);
  // Checking the feature state with reporting intent should record a potential
  // violation.
  DeprecationReportBody* body = MakeGarbageCollected<DeprecationReportBody>(
      "FeatureId", base::Time::FromMillisecondsSinceUnixEpoch(2e9),
      "Test report");
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
      "FeatureId", base::Time::FromSecondsSinceUnixEpoch(1), "Test report");
  auto* report = MakeGarbageCollected<Report>(
      "deprecation", win->document()->Url().GetString(), body);
  ReportingContext::From(win)->QueueReport(report);
  run_loop.Run();

  EXPECT_TRUE(reporting_service.DeprecationReportAnticipatedRemoval());
  // We had a bug that anticipatedRemoval had a wrong value only in mojo method
  // calls.
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1),
            *reporting_service.DeprecationReportAnticipatedRemoval());
}

TEST_F(ReportingContextTest, PermissionsPolicyViolationReportMessage) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>();
  auto* win = dummy_page_holder->GetFrame().DomWindow();

  base::RunLoop run_loop;
  MockReportingServiceProxy reporting_service(win->GetBrowserInterfaceBroker(),
                                              run_loop.QuitClosure());
  auto* body = MakeGarbageCollected<PermissionsPolicyViolationReportBody>(
      "FeatureId", "TestMessage1", "enforce");
  auto* report = MakeGarbageCollected<Report>(
      "permissions-policy-violation", win->document()->Url().GetString(), body);
  auto* reporting_context = ReportingContext::From(win);
  reporting_context->QueueReport(report);
  run_loop.Run();

  EXPECT_EQ(reporting_service.LastMessage(), body->message());
}

TEST_F(ReportingContextTest, DocumentPolicyViolationReportMessage) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>();
  auto* win = dummy_page_holder->GetFrame().DomWindow();

  base::RunLoop run_loop;
  MockReportingServiceProxy reporting_service(win->GetBrowserInterfaceBroker(),
                                              run_loop.QuitClosure());
  auto* body = MakeGarbageCollected<DocumentPolicyViolationReportBody>(
      "FeatureId", "TestMessage2", "enforce", "https://resource.com");
  auto* report = MakeGarbageCollected<Report>(
      "document-policy-violation", win->document()->Url().GetString(), body);
  auto* reporting_context = ReportingContext::From(win);
  reporting_context->QueueReport(report);
  run_loop.Run();

  EXPECT_EQ(reporting_service.LastMessage(), body->message());
}

}  // namespace blink
