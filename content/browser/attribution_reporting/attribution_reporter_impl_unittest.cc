// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_reporter_impl.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "content/browser/attribution_reporting/conversion_manager.h"
#include "content/browser/attribution_reporting/conversion_test_utils.h"
#include "content/browser/attribution_reporting/sent_report_info.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

// Create a report which should be sent at |report_time|. Impression
// data/conversion data/conversion id are all the same for simplicity.
AttributionReport GetReport(base::Time conversion_time,
                            base::Time report_time,
                            AttributionReport::Id conversion_id) {
  // Construct impressions with a null impression time as it is not used for
  // reporting.
  return AttributionReport(ImpressionBuilder(base::Time()).Build(),
                           /*conversion_data=*/0, conversion_time, report_time,
                           /*priority=*/0, conversion_id);
}

// NetworkSender that keep track of the last sent report id.
class MockNetworkSender : public AttributionReporterImpl::NetworkSender {
 public:
  MockNetworkSender() = default;

  void SendReport(AttributionReport report,
                  ReportSentCallback sent_callback) override {
    last_sent_report_id_ = *report.conversion_id;
    num_reports_sent_++;
    std::move(sent_callback)
        .Run(SentReportInfo(std::move(report), SentReportInfo::Status::kSent,
                            /*http_response_code=*/200));
  }

  AttributionReport::Id last_sent_report_id() { return last_sent_report_id_; }

  size_t num_reports_sent() { return num_reports_sent_; }

  void Reset() {
    num_reports_sent_ = 0u;
    last_sent_report_id_ = AttributionReport::Id();
  }

 private:
  size_t num_reports_sent_ = 0u;
  AttributionReport::Id last_sent_report_id_;
};

}  // namespace

class AttributionReporterImplTest : public testing::Test {
 public:
  AttributionReporterImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        browser_context_(std::make_unique<TestBrowserContext>()),
        reporter_(std::make_unique<AttributionReporterImpl>(
            static_cast<StoragePartitionImpl*>(
                browser_context_->GetDefaultStoragePartition()),
            task_environment_.GetMockClock(),
            base::BindRepeating(&AttributionReporterImplTest::OnReportSent,
                                base::Unretained(this)))) {
    auto network_sender = std::make_unique<MockNetworkSender>();
    sender_ = network_sender.get();
    reporter_->SetNetworkSenderForTesting(std::move(network_sender));
    reporter_->SetNetworkConnectionTracker(
        network::TestNetworkConnectionTracker::GetInstance());
  }

  const base::Clock& clock() { return *task_environment_.GetMockClock(); }

  const absl::optional<SentReportInfo>& last_sent_report_info() const {
    return last_sent_report_info_;
  }

  void SetOffline(bool offline) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        offline ? network::mojom::ConnectionType::CONNECTION_NONE
                : network::mojom::ConnectionType::CONNECTION_UNKNOWN);
    task_environment_.RunUntilIdle();
  }

 protected:
  // |task_environment_| must be initialized first.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;

  std::unique_ptr<AttributionReporterImpl> reporter_;
  MockNetworkSender* sender_;

 private:
  absl::optional<SentReportInfo> last_sent_report_info_;

  void OnReportSent(SentReportInfo info) {
    last_sent_report_info_ = std::move(info);
  }
};

TEST_F(AttributionReporterImplTest,
       ReportAddedWithImmediateReportTime_ReportSent) {
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now(), AttributionReport::Id(1))});

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(1, *sender_->last_sent_report_id());
  EXPECT_EQ(1L, *last_sent_report_info()->report.conversion_id.value());
  EXPECT_EQ(200, last_sent_report_info()->http_response_code);
}

TEST_F(AttributionReporterImplTest,
       ReportWithReportTimeBeforeCurrentTime_ReportSent) {
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now() - base::Hours(10),
                 AttributionReport::Id(1))});

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(1, *sender_->last_sent_report_id());
  EXPECT_EQ(1L, *last_sent_report_info()->report.conversion_id.value());
  EXPECT_EQ(200, last_sent_report_info()->http_response_code);
}

TEST_F(AttributionReporterImplTest,
       ReportWithReportTimeBeforeCurrentTime_DeletedReportNotSent) {
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now() - base::Hours(10),
                 AttributionReport::Id(1))});

  reporter_->RemoveAllReportsFromQueue();

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0u, sender_->num_reports_sent());
  EXPECT_EQ(SentReportInfo::Status::kRemovedFromQueue,
            last_sent_report_info()->status);
}

TEST_F(AttributionReporterImplTest,
       ReportWithDelayedReportTime_NotSentUntilDelay) {
  const base::TimeDelta delay = base::Minutes(30);

  reporter_->AddReportsToQueue({GetReport(clock().Now(), clock().Now() + delay,
                                          AttributionReport::Id(1))});
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0u, sender_->num_reports_sent());

  task_environment_.FastForwardBy(delay - base::Seconds(1));
  EXPECT_EQ(0u, sender_->num_reports_sent());

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1u, sender_->num_reports_sent());
}

TEST_F(AttributionReporterImplTest, DuplicateReportScheduled_Ignored) {
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now() + base::Minutes(1),
                 AttributionReport::Id(1))});

  // A duplicate report should not be scheduled.
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now() + base::Minutes(1),
                 AttributionReport::Id(1))});
  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(1u, sender_->num_reports_sent());
}

TEST_F(AttributionReporterImplTest,
       NewReportWithPreviouslySeenConversionId_Scheduled) {
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now(), AttributionReport::Id(1))});
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(1u, sender_->num_reports_sent());

  // We should schedule the new report because the previous report has been
  // sent.
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now(), AttributionReport::Id(1))});
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(2u, sender_->num_reports_sent());
}

TEST_F(AttributionReporterImplTest, ManyReportsAddedAtOnce_SentInOrder) {
  std::vector<AttributionReport> reports;
  for (int i = 1; i < 10; i++) {
    reports.push_back(GetReport(clock().Now(), clock().Now() + base::Minutes(i),
                                AttributionReport::Id(i)));
  }
  reporter_->AddReportsToQueue(reports);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0u, sender_->num_reports_sent());

  for (int i = 1; i < 10; i++) {
    task_environment_.FastForwardBy(base::Minutes(1));

    EXPECT_EQ(static_cast<size_t>(i), sender_->num_reports_sent());
    EXPECT_EQ(static_cast<int64_t>(i), *sender_->last_sent_report_id());
    EXPECT_EQ(static_cast<int64_t>(i),
              *last_sent_report_info()->report.conversion_id.value());
  }
}

TEST_F(AttributionReporterImplTest, ManyReportsAddedSeparately_SentInOrder) {
  for (int i = 1; i < 10; i++) {
    reporter_->AddReportsToQueue(
        {GetReport(clock().Now(), clock().Now() + base::Minutes(i),
                   AttributionReport::Id(i))});
  }
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0u, sender_->num_reports_sent());

  for (int i = 1; i < 10; i++) {
    task_environment_.FastForwardBy(base::Minutes(1));

    EXPECT_EQ(static_cast<size_t>(i), sender_->num_reports_sent());
    EXPECT_EQ(static_cast<int64_t>(i), *sender_->last_sent_report_id());
    EXPECT_EQ(static_cast<int64_t>(i),
              *last_sent_report_info()->report.conversion_id.value());
  }
}

TEST_F(AttributionReporterImplTest,
       EmbedderDisallowsConversions_ReportNotSent) {
  ConversionDisallowingContentBrowserClient disallowed_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&disallowed_browser_client);
  reporter_->AddReportsToQueue(
      {GetReport(clock().Now(), clock().Now(), AttributionReport::Id(1))});

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(0u, sender_->num_reports_sent());
  EXPECT_EQ(1L, *last_sent_report_info()->report.conversion_id.value());
  // Verify that the report was not sent to the NetworkSender.
  EXPECT_EQ(0, last_sent_report_info()->http_response_code);
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(AttributionReporterImplTest, EmbedderDisallowedContext_ReportNotSent) {
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
    std::vector<AttributionReport> reports{
        AttributionReport(std::move(impression),
                          /*conversion_data=*/0, clock().Now(), clock().Now(),
                          /*priority=*/0, AttributionReport::Id(1))};
    reporter_->AddReportsToQueue(std::move(reports));

    // Fast forward by 0, as we yield the thread when a report is scheduled to
    // be sent.
    task_environment_.FastForwardBy(base::TimeDelta());
    EXPECT_EQ(static_cast<size_t>(test_case.report_allowed),
              sender_->num_reports_sent())
        << "impression_origin; " << test_case.impression_origin
        << ", conversion_origin: " << test_case.conversion_origin
        << ", reporting_origin: " << test_case.reporting_origin;
    EXPECT_EQ(1L, *last_sent_report_info()->report.conversion_id.value());
    EXPECT_EQ(test_case.report_allowed ? 200 : 0,
              last_sent_report_info()->http_response_code);

    sender_->Reset();
  }

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(AttributionReporterImplTest, NetworkConnectionTrackerSkipsSends) {
  SetOffline(true);

  reporter_->AddReportsToQueue({
      GetReport(clock().Now(), clock().Now() + base::Minutes(1),
                AttributionReport::Id(1)),
      GetReport(clock().Now(), clock().Now() + base::Minutes(2),
                AttributionReport::Id(2)),
  });

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(0u, sender_->num_reports_sent());
  EXPECT_EQ(AttributionReport::Id(1),
            *last_sent_report_info()->report.conversion_id);
  EXPECT_EQ(SentReportInfo::Status::kOffline, last_sent_report_info()->status);

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(0u, sender_->num_reports_sent());
  EXPECT_EQ(AttributionReport::Id(2),
            *last_sent_report_info()->report.conversion_id);
  EXPECT_EQ(SentReportInfo::Status::kOffline, last_sent_report_info()->status);

  reporter_->AddReportsToQueue({
      GetReport(clock().Now(), clock().Now() + base::Minutes(1),
                AttributionReport::Id(1)),
      GetReport(clock().Now(), clock().Now() + base::Minutes(2),
                AttributionReport::Id(2)),
  });

  SetOffline(false);

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(1u, sender_->num_reports_sent());
  EXPECT_EQ(AttributionReport::Id(1),
            *last_sent_report_info()->report.conversion_id);
  EXPECT_EQ(SentReportInfo::Status::kSent, last_sent_report_info()->status);

  SetOffline(true);

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(1u, sender_->num_reports_sent());
  EXPECT_EQ(AttributionReport::Id(2),
            *last_sent_report_info()->report.conversion_id);
  EXPECT_EQ(SentReportInfo::Status::kOffline, last_sent_report_info()->status);
}

}  // namespace content
