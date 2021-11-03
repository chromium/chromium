// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/sent_report_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/storable_trigger.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using CreateReportStatus =
    ::content::AttributionStorage::CreateReportResult::Status;

using ::testing::ElementsAre;

constexpr base::TimeDelta kExpiredReportOffset = base::Minutes(2);

class ConstantStartupDelayPolicy : public AttributionPolicy {
 public:
  ConstantStartupDelayPolicy() = default;
  ~ConstantStartupDelayPolicy() override = default;

  base::Time GetReportTimeForReportPastSendTime(base::Time now) const override {
    return now + kExpiredReportOffset;
  }
};

// Mock reporter that tracks reports being queued by the AttributionManager.
class TestAttributionReporter
    : public AttributionManagerImpl::AttributionReporter {
 public:
  TestAttributionReporter() = default;
  ~TestAttributionReporter() override = default;

  // AttributionManagerImpl::AttributionReporter
  void AddReportsToQueue(std::vector<AttributionReport> reports) override {
    if (reports.empty())
      return;

    num_reports_ += reports.size();
    last_report_time_ = reports.back().report_time;

    for (auto& report : reports) {
      SentReportInfo info(std::move(report), sent_report_info_status_,
                          /*http_response_code=*/0);

      if (should_run_report_sent_callbacks_) {
        report_sent_callback_.Run(std::move(info));
      } else {
        deferred_callbacks_.push_back(std::move(info));
      }
    }

    if (quit_closure_ && num_reports_ >= expected_num_reports_)
      std::move(quit_closure_).Run();
  }

  void RunDeferredCallbacks() {
    for (auto& deferred_callback : deferred_callbacks_) {
      report_sent_callback_.Run(std::move(deferred_callback));
    }
    deferred_callbacks_.clear();
  }

  void RemoveAllReportsFromQueue() override {
    for (auto& deferred_callback : deferred_callbacks_) {
      deferred_callback.status = SentReportInfo::Status::kRemovedFromQueue;
    }
    RunDeferredCallbacks();
  }

  void ShouldRunReportSentCallbacks(bool should_run_report_sent_callbacks) {
    should_run_report_sent_callbacks_ = should_run_report_sent_callbacks;
  }

  void SetSentReportInfoStatus(SentReportInfo::Status status) {
    sent_report_info_status_ = status;
  }

  size_t num_reports() { return num_reports_; }

  base::Time last_report_time() { return last_report_time_; }

  void WaitForNumReports(size_t expected_num_reports) {
    if (num_reports_ >= expected_num_reports)
      return;

    expected_num_reports_ = expected_num_reports;
    base::RunLoop wait_loop;
    quit_closure_ = wait_loop.QuitClosure();
    wait_loop.Run();
  }

  void SetReportSentCallback(
      base::RepeatingCallback<void(SentReportInfo)> report_sent_callback) {
    report_sent_callback_ = std::move(report_sent_callback);
  }

 private:
  base::RepeatingCallback<void(SentReportInfo)> report_sent_callback_;
  bool should_run_report_sent_callbacks_ = false;
  SentReportInfo::Status sent_report_info_status_ =
      SentReportInfo::Status::kSent;
  size_t expected_num_reports_ = 0u;
  size_t num_reports_ = 0u;
  base::Time last_report_time_;
  base::OnceClosure quit_closure_;

  std::vector<SentReportInfo> deferred_callbacks_;
};

// Time after impression that a conversion can first be sent. See
// AttributionStorageDelegateImpl::GetReportTimeForConversion().
constexpr base::TimeDelta kFirstReportingWindow = base::Days(2);

// Give impressions a sufficiently long expiry.
constexpr base::TimeDelta kImpressionExpiry = base::Days(30);

const size_t kMaxSentReportsToStore = 3;

}  // namespace

class AttributionManagerImplTest : public testing::Test {
 public:
  AttributionManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        mock_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    CreateManager();
  }

  void CreateManager() {
    auto reporter = std::make_unique<TestAttributionReporter>();
    test_reporter_ = reporter.get();
    attribution_manager_ = AttributionManagerImpl::CreateForTesting(
        std::move(reporter), std::make_unique<ConstantStartupDelayPolicy>(),
        task_environment_.GetMockClock(), dir_.GetPath(), mock_storage_policy_,
        kMaxSentReportsToStore);
    test_reporter_->SetReportSentCallback(
        base::BindRepeating(&AttributionManagerImpl::OnReportSent,
                            base::Unretained(attribution_manager_.get())));
  }

  void ExpectNumStoredImpressions(size_t expected_num_impressions) {
    // There should be one impression and one conversion.
    base::RunLoop impression_loop;
    auto get_impressions_callback = base::BindLambdaForTesting(
        [&](std::vector<StorableSource> impressions) {
          EXPECT_EQ(expected_num_impressions, impressions.size());
          impression_loop.Quit();
        });
    attribution_manager_->GetActiveSourcesForWebUI(
        std::move(get_impressions_callback));
    impression_loop.Run();
  }

  void ExpectNumStoredReports(size_t expected_num_reports) {
    base::RunLoop report_loop;
    auto reports_callback =
        base::BindLambdaForTesting([&](std::vector<AttributionReport> reports) {
          EXPECT_EQ(expected_num_reports, reports.size());
          report_loop.Quit();
        });
    attribution_manager_->GetPendingReportsForWebUI(std::move(reports_callback),
                                                    base::Time::Max());
    report_loop.Run();
  }

  const base::Clock& clock() { return *task_environment_.GetMockClock(); }

 protected:
  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AttributionManagerImpl> attribution_manager_;
  TestAttributionReporter* test_reporter_ = nullptr;
  scoped_refptr<storage::MockSpecialStoragePolicy> mock_storage_policy_;
};

TEST_F(AttributionManagerImplTest, ImpressionRegistered_ReturnedToWebUI) {
  auto impression = SourceBuilder(clock().Now())
                        .SetExpiry(kImpressionExpiry)
                        .SetSourceEventId(100)
                        .Build();
  attribution_manager_->HandleSource(impression);

  base::RunLoop run_loop;
  auto get_impressions_callback =
      base::BindLambdaForTesting([&](std::vector<StorableSource> impressions) {
        EXPECT_THAT(impressions, ElementsAre(impression));
        run_loop.Quit();
      });
  attribution_manager_->GetActiveSourcesForWebUI(
      std::move(get_impressions_callback));
  run_loop.Run();
}

TEST_F(AttributionManagerImplTest, ExpiredImpression_NotReturnedToWebUI) {
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetExpiry(kImpressionExpiry)
                                         .SetSourceEventId(100)
                                         .Build());
  task_environment_.FastForwardBy(2 * kImpressionExpiry);

  base::RunLoop run_loop;
  auto get_impressions_callback =
      base::BindLambdaForTesting([&](std::vector<StorableSource> impressions) {
        EXPECT_TRUE(impressions.empty());
        run_loop.Quit();
      });
  attribution_manager_->GetActiveSourcesForWebUI(
      std::move(get_impressions_callback));
  run_loop.Run();
}

TEST_F(AttributionManagerImplTest, ImpressionConverted_ReportReturnedToWebUI) {
  auto impression = SourceBuilder(clock().Now())
                        .SetExpiry(kImpressionExpiry)
                        .SetSourceEventId(100)
                        .Build();
  attribution_manager_->HandleSource(impression);

  auto conversion = DefaultTrigger();
  attribution_manager_->HandleTrigger(conversion);

  AttributionReport expected_report(
      impression, conversion.trigger_data(),
      /*conversion_time=*/clock().Now(),
      /*report_time=*/clock().Now() + kFirstReportingWindow,
      /*priority=*/0,
      /*conversion_id=*/absl::nullopt);

  base::RunLoop run_loop;
  auto reports_callback =
      base::BindLambdaForTesting([&](std::vector<AttributionReport> reports) {
        EXPECT_THAT(reports, ElementsAre(expected_report));
        run_loop.Quit();
      });
  attribution_manager_->GetPendingReportsForWebUI(std::move(reports_callback),
                                                  base::Time::Max());
  run_loop.Run();
}

TEST_F(AttributionManagerImplTest, ImpressionConverted_ReportQueued) {
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  // Reports are queued in intervals ahead of when they should be
  // sent. Make sure the report is not queued earlier than this.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval -
                                  base::Minutes(1));
  EXPECT_EQ(0u, test_reporter_->num_reports());

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(1u, test_reporter_->num_reports());
}

TEST_F(AttributionManagerImplTest, QueuedReportNotSent_NotQueuedAgain) {
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // If the report is not sent, it should not be added to the queue again as
  // long as the reporter is still handling it.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());
}

TEST_F(AttributionManagerImplTest,
       QueuedReportFailedWithShouldRetry_QueuedAgain) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);
  test_reporter_->SetSentReportInfoStatus(
      SentReportInfo::Status::kTransientFailure);

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  // This is 3 instead of 1 because the failed report is directly added back
  // into the queue 2 times.
  EXPECT_EQ(3u, test_reporter_->num_reports());

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest,
       QueuedReportFailedWithoutShouldRetry_NotQueuedAgain) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);
  test_reporter_->SetSentReportInfoStatus(SentReportInfo::Status::kFailure);

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // If the report indicated retry, it should be added to the queue again.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportAlwaysFails_StopsSending) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(false);
  test_reporter_->SetSentReportInfoStatus(
      SentReportInfo::Status::kTransientFailure);

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  base::Time expected_report_time = clock().Now() + kFirstReportingWindow;

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval -
                                  base::Milliseconds(1));
  EXPECT_EQ(base::Time(), test_reporter_->last_report_time());

  // The report is first in the queuing window.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(expected_report_time, test_reporter_->last_report_time());

  // Simulate the reporter sending the report only once the actual report time
  // has been reached.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  test_reporter_->RunDeferredCallbacks();

  test_reporter_->WaitForNumReports(2);
  // At this point, the report has been added directly to the reporter with the
  // updated report time of +5 minutes.
  expected_report_time += base::Minutes(5);
  EXPECT_EQ(expected_report_time, test_reporter_->last_report_time());

  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_EQ(expected_report_time, test_reporter_->last_report_time());
  test_reporter_->RunDeferredCallbacks();

  test_reporter_->WaitForNumReports(3);
  // At this point, the report has been added directly to the reporter with the
  // updated report time of +15 minutes.
  expected_report_time += base::Minutes(15);
  EXPECT_EQ(expected_report_time, test_reporter_->last_report_time());

  task_environment_.FastForwardBy(base::Minutes(15));
  EXPECT_EQ(expected_report_time, test_reporter_->last_report_time());
  test_reporter_->RunDeferredCallbacks();

  // At this point, the report has reached the maximum number of attempts and it
  // should no longer be present in the DB.
  ExpectNumStoredReports(0);

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportOffline_NoFailureIncrement) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);
  test_reporter_->SetSentReportInfoStatus(
      SentReportInfo::Status::kTransientFailure);

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  // This is 3 instead of 1 because the failed report is directly added back
  // into the queue 2 times.
  EXPECT_EQ(3u, test_reporter_->num_reports());

  test_reporter_->SetSentReportInfoStatus(SentReportInfo::Status::kOffline);
  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_EQ(3u, test_reporter_->num_reports());

  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_EQ(3u, test_reporter_->num_reports());

  // kFailed =1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest, ReportExpiredAtStartup_Sent) {
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  // Simulate shutdown.
  attribution_manager_.reset();

  // Fast-forward past the reporting window and past report expiry.
  task_environment_.FastForwardBy(kFirstReportingWindow);
  task_environment_.FastForwardBy(base::Days(100));

  // Simulate startup and ensure the report is sent before being expired.
  CreateManager();

  test_reporter_->WaitForNumReports(1);
  EXPECT_EQ(1u, test_reporter_->num_reports());
}

TEST_F(AttributionManagerImplTest, QueuedReportSent_NotQueuedAgain) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // The report should not be added to the queue again.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // kSent = 0.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 0, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportSent_SentReportInfoUpdated) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);

  test_reporter_->SetSentReportInfoStatus(SentReportInfo::Status::kSent);
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetSourceEventId(1)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  // This one shouldn't be stored, as its status is `kDropped`.
  test_reporter_->SetSentReportInfoStatus(SentReportInfo::Status::kDropped);
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetSourceEventId(2)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  test_reporter_->SetSentReportInfoStatus(SentReportInfo::Status::kSent);
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetSourceEventId(3)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  // This one shouldn't be stored, as it will be retried.
  test_reporter_->SetSentReportInfoStatus(
      SentReportInfo::Status::kTransientFailure);
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetSourceEventId(4)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  const auto& sent_reports =
      attribution_manager_->GetSessionStorage().GetSentReports();
  EXPECT_EQ(2u, sent_reports.size());
  EXPECT_EQ(1u, sent_reports[0].report.impression.source_event_id());
  EXPECT_EQ(3u, sent_reports[1].report.impression.source_event_id());

  // kSent = 0.
  histograms.ExpectBucketCount("Conversion.ReportSendOutcome", 0, 2);
  // kFailed = 1.
  histograms.ExpectBucketCount("Conversion.ReportSendOutcome", 1, 1);
  // kDropped = 2.
  histograms.ExpectBucketCount("Conversion.ReportSendOutcome", 2, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportSent_StoresLastN) {
  test_reporter_->ShouldRunReportSentCallbacks(true);

  // Process |kMaxSentReportsToStore + 1| reports.
  for (uint64_t i = 1; i <= 4; i++) {
    attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                           .SetSourceEventId(i)
                                           .SetExpiry(kImpressionExpiry)
                                           .Build());
    attribution_manager_->HandleTrigger(DefaultTrigger());
    task_environment_.FastForwardBy(kFirstReportingWindow -
                                    kAttributionManagerQueueReportsInterval);
  }

  // Only the last |kMaxSentReportsToStore| should be stored.
  const auto& sent_reports =
      attribution_manager_->GetSessionStorage().GetSentReports();
  EXPECT_EQ(3u, sent_reports.size());
  EXPECT_EQ(2u, sent_reports[0].report.impression.source_event_id());
  EXPECT_EQ(3u, sent_reports[1].report.impression.source_event_id());
  EXPECT_EQ(4u, sent_reports[2].report.impression.source_event_id());
}

TEST_F(AttributionManagerImplTest, DroppedReport_StoresLastN) {
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  ExpectNumStoredImpressions(1);

  // `kNavigation` sources can have 3 reports, so none of these should result in
  // a dropped report.
  for (int i = 1; i <= 3; i++) {
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(i).Build());
    ExpectNumStoredReports(i);
    EXPECT_EQ(
        0u,
        attribution_manager_->GetSessionStorage().GetDroppedReports().size());
  }

  {
    // This should replace the report with priority 1.
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(4).Build());
    ExpectNumStoredReports(3);
    const auto& dropped_reports =
        attribution_manager_->GetSessionStorage().GetDroppedReports();
    EXPECT_EQ(1u, dropped_reports.size());
    EXPECT_EQ(1, dropped_reports[0].dropped_report()->priority);
    EXPECT_EQ(CreateReportStatus::kSuccessDroppedLowerPriority,
              dropped_reports[0].status());
  }

  {
    // This should be dropped, as it has a lower priority than all stored
    // reports.
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(-5).Build());
    ExpectNumStoredReports(3);
    const auto& dropped_reports =
        attribution_manager_->GetSessionStorage().GetDroppedReports();
    EXPECT_EQ(2u, dropped_reports.size());
    EXPECT_EQ(1, dropped_reports[0].dropped_report()->priority);
    EXPECT_EQ(CreateReportStatus::kSuccessDroppedLowerPriority,
              dropped_reports[0].status());
    EXPECT_EQ(-5, dropped_reports[1].dropped_report()->priority);
    EXPECT_EQ(CreateReportStatus::kPriorityTooLow, dropped_reports[1].status());
  }

  {
    // These should replace the reports with priority 2 and 3 and pop the report
    // with priority 1 from the session storage, as only
    // `kMaxSentReportsToStore` should be stored.
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(5).Build());
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(6).Build());
    ExpectNumStoredReports(3);
    const auto& dropped_reports =
        attribution_manager_->GetSessionStorage().GetDroppedReports();
    EXPECT_EQ(3u, dropped_reports.size());
    EXPECT_EQ(-5, dropped_reports[0].dropped_report()->priority);
    EXPECT_EQ(CreateReportStatus::kPriorityTooLow, dropped_reports[0].status());
    EXPECT_EQ(2, dropped_reports[1].dropped_report()->priority);
    EXPECT_EQ(CreateReportStatus::kSuccessDroppedLowerPriority,
              dropped_reports[1].status());
    EXPECT_EQ(3, dropped_reports[2].dropped_report()->priority);
    EXPECT_EQ(CreateReportStatus::kSuccessDroppedLowerPriority,
              dropped_reports[2].status());
  }
}

// Add a conversion to storage and reset the manager to mimic a report being
// available at startup.
TEST_F(AttributionManagerImplTest, ExpiredReportsAtStartup_Queued) {
  // Create a report that will be reported at t= 2 days.
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  // Create another conversion that will be reported at t=
  // (kFirstReportingWindow + 2 * kAttributionManagerQueueReportsInterval).
  task_environment_.FastForwardBy(2 * kAttributionManagerQueueReportsInterval);
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  EXPECT_EQ(0u, test_reporter_->num_reports());

  // Reset the manager to simulate shutdown.
  attribution_manager_.reset();

  // Fast forward past the expected report time of the first conversion, t =
  // (kFirstReportingWindow+ 1 minute).
  task_environment_.FastForwardBy(
      kFirstReportingWindow - (2 * kAttributionManagerQueueReportsInterval) +
      base::Minutes(1));

  // Create the manager and check that the first report is queued immediately.
  CreateManager();
  test_reporter_->ShouldRunReportSentCallbacks(true);
  test_reporter_->WaitForNumReports(1);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // The second report is still queued at the correct time.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  EXPECT_EQ(2u, test_reporter_->num_reports());
}

// This functionality is tested more thoroughly in the AttributionStorageSql
// unit tests. Here, just test to make sure the basic control flow is working.
TEST_F(AttributionManagerImplTest, ClearData) {
  for (bool match_url : {true, false}) {
    base::Time start = clock().Now();
    attribution_manager_->HandleSource(
        SourceBuilder(start).SetExpiry(kImpressionExpiry).Build());
    attribution_manager_->HandleTrigger(DefaultTrigger());

    base::RunLoop run_loop;
    attribution_manager_->ClearData(
        start, start + base::Minutes(1),
        base::BindLambdaForTesting(
            [match_url](const url::Origin& _) { return match_url; }),
        run_loop.QuitClosure());
    run_loop.Run();

    task_environment_.FastForwardBy(kFirstReportingWindow -
                                    kAttributionManagerQueueReportsInterval);
    size_t expected_reports = match_url ? 0u : 1u;
    EXPECT_EQ(expected_reports, test_reporter_->num_reports());
  }
}

TEST_F(AttributionManagerImplTest, ClearData_ClearsSentReports) {
  test_reporter_->ShouldRunReportSentCallbacks(true);

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  EXPECT_FALSE(
      attribution_manager_->GetSessionStorage().GetSentReports().empty());

  attribution_manager_->ClearData(clock().Now(), clock().Now(),
                                  base::NullCallback(), base::DoNothing());
  EXPECT_TRUE(
      attribution_manager_->GetSessionStorage().GetSentReports().empty());
}

TEST_F(AttributionManagerImplTest, ConversionsSentFromUI_ReportedImmediately) {
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_EQ(0u, test_reporter_->num_reports());

  attribution_manager_->SendReportsForWebUI(base::DoNothing());
  task_environment_.FastForwardBy(base::Minutes(0));
  EXPECT_EQ(1u, test_reporter_->num_reports());
}

// TODO(crbug.com/1088449): Flaky on Linux and Android.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#define MAYBE_ExpiredReportsAtStartup_Delayed \
  DISABLED_ExpiredReportsAtStartup_Delayed
#else
#define MAYBE_ExpiredReportsAtStartup_Delayed ExpiredReportsAtStartup_Delayed
#endif
TEST_F(AttributionManagerImplTest, MAYBE_ExpiredReportsAtStartup_Delayed) {
  // Create a report that will be reported at t= 2 days.
  base::Time start_time = clock().Now();
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_EQ(0u, test_reporter_->num_reports());

  // Reset the manager to simulate shutdown.
  attribution_manager_.reset();

  // Fast forward past the expected report time of the first conversion, t =
  // (kFirstReportingWindow+ 1 minute).
  task_environment_.FastForwardBy(kFirstReportingWindow + base::Minutes(1));

  CreateManager();
  test_reporter_->WaitForNumReports(1);

  // Ensure that the expired report is delayed based on the time the browser
  // started.
  EXPECT_EQ(start_time + kFirstReportingWindow + base::Minutes(1) +
                kExpiredReportOffset,
            test_reporter_->last_report_time());
}

TEST_F(AttributionManagerImplTest,
       NonExpiredReportsQueuedAtStartup_NotDelayed) {
  // Create a report that will be reported at t= 2 days.
  base::Time start_time = clock().Now();
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_EQ(0u, test_reporter_->num_reports());

  // Reset the manager to simulate shutdown.
  attribution_manager_.reset();

  // Fast forward just before the expected report time.
  task_environment_.FastForwardBy(kFirstReportingWindow - base::Minutes(1));

  // Ensure that this report does not receive additional delay.
  CreateManager();
  test_reporter_->WaitForNumReports(1);
  EXPECT_EQ(1u, test_reporter_->num_reports());
  EXPECT_EQ(start_time + kFirstReportingWindow,
            test_reporter_->last_report_time());
}

TEST_F(AttributionManagerImplTest, SessionOnlyOrigins_DataDeletedAtShutdown) {
  GURL session_only_origin("https://sessiononly.example");
  auto impression =
      SourceBuilder(clock().Now())
          .SetImpressionOrigin(url::Origin::Create(session_only_origin))
          .Build();

  mock_storage_policy_->AddSessionOnly(session_only_origin);

  attribution_manager_->HandleSource(impression);
  attribution_manager_->HandleTrigger(DefaultTrigger());

  ExpectNumStoredImpressions(1u);
  ExpectNumStoredReports(1u);

  // Reset the manager to simulate shutdown.
  attribution_manager_.reset();
  CreateManager();

  ExpectNumStoredImpressions(0u);
  ExpectNumStoredReports(0u);
}

TEST_F(AttributionManagerImplTest,
       SessionOnlyOrigins_DeletedIfAnyOriginMatches) {
  url::Origin session_only_origin =
      url::Origin::Create(GURL("https://sessiononly.example"));
  // Create impressions which each have the session only origin as one of
  // impression/conversion/reporting origin.
  auto impression1 = SourceBuilder(clock().Now())
                         .SetImpressionOrigin(session_only_origin)
                         .Build();
  auto impression2 = SourceBuilder(clock().Now())
                         .SetReportingOrigin(session_only_origin)
                         .Build();
  auto impression3 = SourceBuilder(clock().Now())
                         .SetConversionOrigin(session_only_origin)
                         .Build();

  // Create one  impression which is not session only.
  auto impression4 = SourceBuilder(clock().Now()).Build();

  mock_storage_policy_->AddSessionOnly(session_only_origin.GetURL());

  attribution_manager_->HandleSource(impression1);
  attribution_manager_->HandleSource(impression2);
  attribution_manager_->HandleSource(impression3);
  attribution_manager_->HandleSource(impression4);

  ExpectNumStoredImpressions(4u);

  // Reset the manager to simulate shutdown.
  attribution_manager_.reset();
  CreateManager();

  // All session-only impressions should be deleted.
  ExpectNumStoredImpressions(1u);
}

// Tests that trigger priority cannot result in more than the maximum number of
// reports being sent. A report will never be queued for the expiry window while
// the source is active given we only queue reports which are reported within
// the next 30 minutes, and the expiry window is one hour after expiry time.
// This ensures that a queued report cannot be overwritten by a new, higher
// priority trigger.
TEST_F(AttributionManagerImplTest, ConversionPrioritization_OneReportSent) {
  test_reporter_->ShouldRunReportSentCallbacks(true);
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(base::Days(7)).Build());
  ExpectNumStoredImpressions(1u);

  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(1).Build());
  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(1).Build());
  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(1).Build());
  ExpectNumStoredReports(3u);

  task_environment_.FastForwardBy(base::Days(7) - base::Minutes(30));
  EXPECT_EQ(3u, test_reporter_->num_reports());

  task_environment_.FastForwardBy(base::Minutes(5));
  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(2).Build());
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(3u, test_reporter_->num_reports());
}

TEST_F(AttributionManagerImplTest, HandleTrigger_RecordsMetric) {
  base::HistogramTester histograms;
  attribution_manager_->HandleTrigger(DefaultTrigger());
  ExpectNumStoredReports(0);
  histograms.ExpectUniqueSample(
      "Conversions.CreateReportStatus",
      AttributionStorage::CreateReportResult::Status::kNoMatchingImpressions,
      1);
}

TEST_F(AttributionManagerImplTest, OnReportSent_RecordsDeleteEventMetric) {
  test_reporter_->ShouldRunReportSentCallbacks(true);
  base::HistogramTester histograms;
  attribution_manager_->HandleSource(SourceBuilder(clock().Now()).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  ExpectNumStoredReports(1);
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  ExpectNumStoredReports(0);

  static constexpr char kMetric[] = "Conversions.DeleteSentReportOperation";
  histograms.ExpectTotalCount(kMetric, 2);
  histograms.ExpectBucketCount(
      kMetric, AttributionManagerImpl::DeleteEvent::kStarted, 1);
  histograms.ExpectBucketCount(
      kMetric, AttributionManagerImpl::DeleteEvent::kSucceeded, 1);
}

TEST_F(AttributionManagerImplTest, ClearData_RequeuesReports) {
  const auto origin_a = url::Origin::Create(GURL("https://a.example/"));
  const auto origin_b = url::Origin::Create(GURL("https://b.example/"));

  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_a)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_a).Build());

  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_b)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_b).Build());

  EXPECT_EQ(0u, test_reporter_->num_reports());

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  test_reporter_->WaitForNumReports(2);
  EXPECT_EQ(2u, test_reporter_->num_reports());

  attribution_manager_->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindLambdaForTesting(
          [&](const url::Origin& origin) { return origin == origin_a; }),
      base::DoNothing());

  test_reporter_->WaitForNumReports(3);
  EXPECT_EQ(3u, test_reporter_->num_reports());
}

TEST_F(AttributionManagerImplTest, ClearData_NoDeleteForRemovedFromQueue) {
  const auto origin_a = url::Origin::Create(GURL("https://a.example/"));
  const auto origin_b = url::Origin::Create(GURL("https://b.example/"));

  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_a)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_a).Build());

  ExpectNumStoredReports(1u);

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  test_reporter_->WaitForNumReports(1);
  EXPECT_EQ(1u, test_reporter_->num_reports());
  ExpectNumStoredReports(1u);

  attribution_manager_->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindLambdaForTesting(
          [&](const url::Origin& origin) { return origin == origin_b; }),
      base::DoNothing());

  ExpectNumStoredReports(1u);
}

}  // namespace content
