// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_reporter_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/sent_report.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Return;

using Checkpoint = ::testing::MockFunction<void(int step)>;

// Create a report which should be sent at |report_time|. Impression
// data/conversion data/conversion id are all the same for simplicity.
AttributionReport GetReport(base::Time report_time,
                            AttributionReport::Id conversion_id) {
  // Construct impressions with a null impression time as it is not used for
  // reporting.
  return ReportBuilder(SourceBuilder(base::Time()).Build())
      .SetReportTime(report_time)
      .SetReportId(conversion_id)
      .Build();
}

class MockNetworkSender : public AttributionReporterImpl::NetworkSender {
 public:
  MOCK_METHOD(void,
              SendReport,
              (AttributionReport report, ReportSentCallback callback),
              (override));
};

auto InvokeCallbackWith(SentReport::Status status,
                        int http_response_code = 200) {
  return
      [=](AttributionReport report,
          AttributionReporterImpl::NetworkSender::ReportSentCallback callback) {
        std::move(callback).Run(
            SentReport(std::move(report), status, http_response_code));
      };
}

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
            callback_.Get())) {
    auto network_sender = std::make_unique<MockNetworkSender>();
    sender_ = network_sender.get();
    reporter_->SetNetworkSenderForTesting(std::move(network_sender));
    reporter_->SetNetworkConnectionTracker(
        network::TestNetworkConnectionTracker::GetInstance());
  }

  const base::Clock& clock() { return *task_environment_.GetMockClock(); }

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

  base::MockCallback<AttributionReporterImpl::Callback> callback_;

  std::unique_ptr<AttributionReporterImpl> reporter_;
  raw_ptr<MockNetworkSender> sender_;
};

TEST_F(AttributionReporterImplTest,
       ReportAddedWithImmediateReportTime_ReportSent) {
  const auto report = GetReport(clock().Now(), AttributionReport::Id(1));

  EXPECT_CALL(*sender_, SendReport(report, _))
      .WillOnce(InvokeCallbackWith(SentReport::Status::kSent));

  EXPECT_CALL(callback_, Run(SentReport(report, SentReport::Status::kSent,
                                        /*http_response_code=*/200)));

  reporter_->AddReportsToQueue({report});

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionReporterImplTest,
       ReportWithReportTimeBeforeCurrentTime_ReportSent) {
  const auto report =
      GetReport(clock().Now() - base::Hours(10), AttributionReport::Id(1));

  EXPECT_CALL(*sender_, SendReport(report, _))
      .WillOnce(InvokeCallbackWith(SentReport::Status::kSent));

  EXPECT_CALL(callback_, Run(SentReport(report, SentReport::Status::kSent,
                                        /*http_response_code=*/200)));

  reporter_->AddReportsToQueue({report});

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionReporterImplTest,
       ReportWithReportTimeBeforeCurrentTime_DeletedReportNotSent) {
  const auto report =
      GetReport(clock().Now() - base::Hours(10), AttributionReport::Id(1));

  EXPECT_CALL(*sender_, SendReport).Times(0);
  EXPECT_CALL(callback_,
              Run(SentReport(report, SentReport::Status::kRemovedFromQueue,
                             /*http_response_code=*/0)));

  reporter_->AddReportsToQueue({report});

  reporter_->RemoveAllReportsFromQueue();

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionReporterImplTest,
       ReportWithDelayedReportTime_NotSentUntilDelay) {
  const base::TimeDelta delay = base::Minutes(30);
  const auto report =
      GetReport(clock().Now() + delay, AttributionReport::Id(1));

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*sender_, SendReport).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*sender_, SendReport).Times(0);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*sender_, SendReport(report, _));
  }

  reporter_->AddReportsToQueue({report});
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  task_environment_.FastForwardBy(delay - base::Seconds(1));
  checkpoint.Call(2);

  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_F(AttributionReporterImplTest, DuplicateReportScheduled_Sent) {
  const auto report =
      GetReport(clock().Now() + base::Minutes(1), AttributionReport::Id(1));

  // A duplicate report should be scheduled, as it is up to the manager to
  // perform deduplication.
  EXPECT_CALL(*sender_, SendReport(report, _)).Times(2);

  reporter_->AddReportsToQueue({report});
  reporter_->AddReportsToQueue({report});

  task_environment_.FastForwardBy(base::Minutes(1));
}

TEST_F(AttributionReporterImplTest,
       NewReportWithPreviouslySeenConversionId_Scheduled) {
  const auto report = GetReport(clock().Now(), AttributionReport::Id(1));

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*sender_, SendReport(report, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*sender_, SendReport(report, _));
  }

  reporter_->AddReportsToQueue({report});
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  // We should schedule the new report because the previous report has been
  // sent.
  reporter_->AddReportsToQueue({report});
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionReporterImplTest, ManyReportsAddedAtOnce_SentInOrder) {
  std::vector<AttributionReport> reports;

  Checkpoint checkpoint;
  {
    EXPECT_CALL(*sender_, SendReport).Times(0);

    for (int i = 1; i < 10; i++) {
      EXPECT_CALL(checkpoint, Call(i));

      auto report =
          GetReport(clock().Now() + base::Minutes(i), AttributionReport::Id(i));

      EXPECT_CALL(*sender_, SendReport(report, _))
          .WillOnce(InvokeCallbackWith(SentReport::Status::kSent));

      EXPECT_CALL(callback_, Run(SentReport(report, SentReport::Status::kSent,
                                            /*http_response_code=*/200)));

      reports.push_back(std::move(report));
    }
  }

  reporter_->AddReportsToQueue(std::move(reports));
  task_environment_.FastForwardBy(base::TimeDelta());

  for (int i = 1; i < 10; i++) {
    checkpoint.Call(i);
    task_environment_.FastForwardBy(base::Minutes(1));
  }
}

TEST_F(AttributionReporterImplTest, ManyReportsAddedSeparately_SentInOrder) {
  std::vector<AttributionReport> reports;

  Checkpoint checkpoint;
  {
    EXPECT_CALL(*sender_, SendReport).Times(0);

    for (int i = 1; i < 10; i++) {
      EXPECT_CALL(checkpoint, Call(i));

      auto report =
          GetReport(clock().Now() + base::Minutes(i), AttributionReport::Id(i));

      EXPECT_CALL(*sender_, SendReport(report, _))
          .WillOnce(InvokeCallbackWith(SentReport::Status::kSent));

      EXPECT_CALL(callback_, Run(SentReport(report, SentReport::Status::kSent,
                                            /*http_response_code=*/200)));

      reports.push_back(std::move(report));
    }
  }

  for (auto& report : reports) {
    reporter_->AddReportsToQueue({std::move(report)});
  }

  task_environment_.FastForwardBy(base::TimeDelta());

  for (int i = 1; i < 10; i++) {
    checkpoint.Call(i);
    task_environment_.FastForwardBy(base::Minutes(1));
  }
}

TEST_F(AttributionReporterImplTest, EmbedderDisallowsReporting_ReportNotSent) {
  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(
      browser_client,
      IsConversionMeasurementOperationAllowed(
          _, ContentBrowserClient::ConversionMeasurementOperation::kReport,
          Pointee(url::Origin::Create(GURL("https://impression.test/"))),
          Pointee(url::Origin::Create(GURL("https://sub.conversion.test/"))),
          Pointee(url::Origin::Create(GURL("https://report.test/")))))
      .WillOnce(Return(false));
  ScopedContentBrowserClientSetting setting(&browser_client);

  const auto report = GetReport(clock().Now(), AttributionReport::Id(1));

  EXPECT_CALL(*sender_, SendReport).Times(0);

  EXPECT_CALL(callback_, Run(SentReport(report, SentReport::Status::kDropped,
                                        /*http_response_code=*/0)));

  reporter_->AddReportsToQueue({report});

  // Fast forward by 0, as we yield the thread when a report is scheduled to be
  // sent.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionReporterImplTest, NetworkConnectionTrackerSkipsSends) {
  const auto report1_1 =
      GetReport(clock().Now() + base::Minutes(1), AttributionReport::Id(1));
  const auto report2_1 =
      GetReport(clock().Now() + base::Minutes(2), AttributionReport::Id(2));

  const auto report1_2 =
      GetReport(clock().Now() + base::Minutes(3), AttributionReport::Id(1));
  const auto report2_2 =
      GetReport(clock().Now() + base::Minutes(4), AttributionReport::Id(2));

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*sender_, SendReport).Times(0);
    EXPECT_CALL(callback_,
                Run(SentReport(report1_1, SentReport::Status::kOffline,
                               /*http_response_code=*/0)));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*sender_, SendReport).Times(0);
    EXPECT_CALL(callback_,
                Run(SentReport(report2_1, SentReport::Status::kOffline,
                               /*http_response_code=*/0)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*sender_, SendReport)
        .WillOnce(InvokeCallbackWith(SentReport::Status::kSent));
    EXPECT_CALL(callback_, Run(SentReport(report1_2, SentReport::Status::kSent,
                                          /*http_response_code=*/200)));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*sender_, SendReport).Times(0);
    EXPECT_CALL(callback_,
                Run(SentReport(report2_2, SentReport::Status::kOffline,
                               /*http_response_code=*/0)));
  }

  SetOffline(true);

  reporter_->AddReportsToQueue({report1_1, report2_1});

  task_environment_.FastForwardBy(base::Minutes(1));
  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Minutes(1));
  checkpoint.Call(2);

  reporter_->AddReportsToQueue({report1_2, report2_2});

  SetOffline(false);

  task_environment_.FastForwardBy(base::Minutes(1));
  checkpoint.Call(3);

  SetOffline(true);

  task_environment_.FastForwardBy(base::Minutes(1));
}

}  // namespace content
