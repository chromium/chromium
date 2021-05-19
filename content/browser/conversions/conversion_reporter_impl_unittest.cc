// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_reporter_impl.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/conversions/sent_report_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Create a report which should be sent at |report_time|. Impression
// data/conversion data/conversion id are all the same for simplicity.
ConversionReport GetReport(base::Time conversion_time,
                           base::Time report_time,
                           int64_t conversion_id) {
  // Construct impressions with a null impression time as it is not used for
  // reporting.
  return ConversionReport(ImpressionBuilder(base::Time()).Build(),
                          /*conversion_data=*/"", conversion_time, report_time,
                          /*conversion_id=*/conversion_id);
}

// NetworkSender that keep track of the last sent report id.
class MockNetworkSender : public ConversionReporterImpl::NetworkSender {
 public:
  MockNetworkSender() = default;

  void SendReport(ConversionReport* conversion_report,
                  ReportSentCallback sent_callback) override {
    last_sent_report_id_ = *conversion_report->conversion_id;
    num_reports_sent_++;
    std::move(sent_callback).Run({.http_response_code = 200});
  }

  int64_t last_sent_report_id() { return last_sent_report_id_; }

  size_t num_reports_sent() { return num_reports_sent_; }

  void Reset() {
    num_reports_sent_ = 0u;
    last_sent_report_id_ = -1;
  }

 private:
  size_t num_reports_sent_ = 0u;
  int64_t last_sent_report_id_ = -1;
};

}  // namespace

class ConversionReporterImplTest : public testing::Test {
 public:
  ConversionReporterImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        browser_context_(std::make_unique<TestBrowserContext>()),
        reporter_(std::make_unique<ConversionReporterImpl>(
            browser_context_->GetDefaultStoragePartition(),
            task_environment_.GetMockClock())) {
    auto network_sender = std::make_unique<MockNetworkSender>();
    sender_ = network_sender.get();
    reporter_->SetNetworkSenderForTesting(std::move(network_sender));
  }

  const base::Clock& clock() { return *task_environment_.GetMockClock(); }

 protected:
  // |task_enviorment_| must be initialized first.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;

  std::unique_ptr<ConversionReporterImpl> reporter_;
  MockNetworkSender* sender_;
};

TEST_F(ConversionReporterImplTest,
       ReportAddedWithImmediateReportTime_ReportSent) {
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now(), /*conversion_id=*/1)},
      base::BindRepeating(
          [](int64_t conversion_id, absl::optional<SentReportInfo> info) {
            EXPECT_EQ(1L, conversion_id);
            EXPECT_TRUE(info.has_value());
            EXPECT_EQ(200, info->http_response_code);
          }));

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(1, sender_->last_sent_report_id());
}

TEST_F(ConversionReporterImplTest,
       ReportWithReportTimeBeforeCurrentTime_ReportSent) {
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now() - base::TimeDelta::FromHours(10),
                 /*conversion_id=*/1)},
      base::BindRepeating(
          [](int64_t conversion_id, absl::optional<SentReportInfo> info) {
            EXPECT_EQ(1L, conversion_id);
            EXPECT_TRUE(info.has_value());
            EXPECT_EQ(200, info->http_response_code);
          }));

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(1, sender_->last_sent_report_id());
}

TEST_F(ConversionReporterImplTest,
       ReportWithDelayedReportTime_NotSentUntilDelay) {
  const base::TimeDelta delay = base::TimeDelta::FromMinutes(30);

  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now() + delay, /*conversion_id=*/1)},
      base::DoNothing());
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0u, sender_->num_reports_sent());

  task_environment_.FastForwardBy(delay - base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(0u, sender_->num_reports_sent());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1u, sender_->num_reports_sent());
}

TEST_F(ConversionReporterImplTest, DuplicateReportScheduled_Ignored) {
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now() + base::TimeDelta::FromMinutes(1),
                 /*conversion_id=*/1)},
      base::DoNothing());

  // A duplicate report should not be scheduled.
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now() + base::TimeDelta::FromMinutes(1),
                 /*conversion_id=*/1)},
      base::DoNothing());
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1u, sender_->num_reports_sent());
}

TEST_F(ConversionReporterImplTest,
       NewReportWithPreviouslySeenConversionId_Scheduled) {
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now(), /*conversion_id=*/1)},
      base::DoNothing());
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(1u, sender_->num_reports_sent());

  // We should schedule the new report because the previous report has been
  // sent.
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now(), /*conversion_id=*/1)},
      base::DoNothing());
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(2u, sender_->num_reports_sent());
}

TEST_F(ConversionReporterImplTest, ManyReportsAddedAtOnce_SentInOrder) {
  std::vector<ConversionReport> reports;
  int64_t last_report_id = 0UL;
  for (int i = 1; i < 10; i++) {
    reports.push_back(GetReport(clock().Now(),
                                clock().Now() + base::TimeDelta::FromMinutes(i),
                                /*conversion_id=*/i));
  }
  reporter_->AddReportsToQueue(
      reports,
      base::BindLambdaForTesting(
          [&](int64_t conversion_id, absl::optional<SentReportInfo> info) {
            last_report_id = conversion_id;
          }));
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0u, sender_->num_reports_sent());

  for (int i = 1; i < 10; i++) {
    task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(1));

    EXPECT_EQ(static_cast<size_t>(i), sender_->num_reports_sent());
    EXPECT_EQ(static_cast<int64_t>(i), sender_->last_sent_report_id());
    EXPECT_EQ(static_cast<int64_t>(i), last_report_id);
  }
}

TEST_F(ConversionReporterImplTest, ManyReportsAddedSeparately_SentInOrder) {
  int64_t last_report_id = 0;
  auto report_sent_callback = base::BindLambdaForTesting(
      [&](int64_t conversion_id, absl::optional<SentReportInfo> info) {
        last_report_id = conversion_id;
      });
  for (int i = 1; i < 10; i++) {
    reporter_->AddReportsToQueue(
        {GetReport(clock().Now(),
                   clock().Now() + base::TimeDelta::FromMinutes(i),
                   /*conversion_id=*/i)},
        report_sent_callback);
  }
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0u, sender_->num_reports_sent());

  for (int i = 1; i < 10; i++) {
    task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(1));

    EXPECT_EQ(static_cast<size_t>(i), sender_->num_reports_sent());
    EXPECT_EQ(static_cast<int64_t>(i), sender_->last_sent_report_id());
    EXPECT_EQ(static_cast<int64_t>(i), last_report_id);
  }
}

TEST_F(ConversionReporterImplTest, EmbedderDisallowsConversions_ReportNotSent) {
  ConversionDisallowingContentBrowserClient disallowed_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&disallowed_browser_client);
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now(), /*conversion_id=*/1)},
      base::BindRepeating(
          [](int64_t conversion_id, absl::optional<SentReportInfo> info) {
            EXPECT_EQ(1L, conversion_id);
            EXPECT_FALSE(info.has_value());
          }));

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0u, sender_->num_reports_sent());
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ConversionReporterImplTest, EmbedderDisallowedContext_ReportNotSent) {
  ConfigurableConversionTestBrowserClient browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&browser_client);

  browser_client.BlockConversionMeasurementInContext(
      absl::make_optional(
          url::Origin::Create(GURL("https://impression.example"))),
      absl::make_optional(
          url::Origin::Create(GURL("https://conversion.example"))),
      absl::make_optional(
          url::Origin::Create(GURL("https://reporting.example"))));

  struct {
    GURL impression_origin;
    GURL conversion_origin;
    GURL reporting_origin;
    bool report_allowed;
  } kTestCases[] = {
      {GURL("https://impression.example"), GURL("https://conversion.example"),
       GURL("https://reporting.example"), false},
      {GURL("https://conversion.example"), GURL("https://impression.example"),
       GURL("https://reporting.example"), true},
      {GURL("https://impression.example"), GURL("https://conversion.example"),
       GURL("https://other.example"), true},
  };

  for (const auto& test_case : kTestCases) {
    auto impression =
        ImpressionBuilder(base::Time())
            .SetImpressionOrigin(
                url::Origin::Create(test_case.impression_origin))
            .SetConversionOrigin(
                url::Origin::Create(test_case.conversion_origin))
            .SetReportingOrigin(url::Origin::Create(test_case.reporting_origin))
            .Build();
    std::vector<ConversionReport> reports{
        ConversionReport(std::move(impression),
                         /*conversion_data=*/"", clock().Now(), clock().Now(),
                         /*conversion_id=*/1)};
    reporter_->AddReportsToQueue(
        std::move(reports),
        base::BindLambdaForTesting(
            [&](int64_t conversion_id, absl::optional<SentReportInfo> info) {
              EXPECT_EQ(1L, conversion_id);
              EXPECT_EQ(test_case.report_allowed, info.has_value());
            }));

    // Fast forward by 0, as we yield the thread when a report is scheduled to
    // be sent.
    task_environment_.FastForwardBy(base::TimeDelta());
    EXPECT_EQ(static_cast<size_t>(test_case.report_allowed),
              sender_->num_reports_sent())
        << "impression_origin; " << test_case.impression_origin
        << ", conversion_origin: " << test_case.conversion_origin
        << ", reporting_origin: " << test_case.reporting_origin;

    sender_->Reset();
  }

  SetBrowserClientForTesting(old_browser_client);
}

}  // namespace content
