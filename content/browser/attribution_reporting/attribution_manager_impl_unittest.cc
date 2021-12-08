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
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/sent_report.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/storable_trigger.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using CreateReportResult = ::content::AttributionStorage::CreateReportResult;
using CreateReportStatus =
    ::content::AttributionStorage::CreateReportResult::Status;
using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Property;
using ::testing::SizeIs;

using Checkpoint = ::testing::MockFunction<void(int step)>;

constexpr base::TimeDelta kExpiredReportOffset = base::Minutes(2);

class ConstantStartupDelayPolicy : public AttributionPolicy {
 public:
  ConstantStartupDelayPolicy() = default;
  ~ConstantStartupDelayPolicy() override = default;

  base::Time GetReportTimeForReportPastSendTime(base::Time now) const override {
    return now + kExpiredReportOffset;
  }
};

class MockAttributionManagerObserver : public AttributionManager::Observer {
 public:
  MOCK_METHOD(void, OnSourcesChanged, (), (override));

  MOCK_METHOD(void, OnReportsChanged, (), (override));

  MOCK_METHOD(void,
              OnSourceDeactivated,
              (const DeactivatedSource& source),
              (override));

  MOCK_METHOD(void, OnReportSent, (const SentReport& info), (override));

  MOCK_METHOD(void,
              OnReportDropped,
              (const AttributionStorage::CreateReportResult& result),
              (override));
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

    for (auto& report : reports) {
      added_reports_.push_back(report);
      SentReport info(std::move(report), sent_report_status_,
                      /*http_response_code=*/0);

      if (should_run_report_sent_callbacks_) {
        report_sent_callback_.Run(std::move(info));
      } else {
        deferred_callbacks_.push_back(std::move(info));
      }
    }

    if (quit_closure_ && added_reports_.size() >= expected_num_reports_)
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
      deferred_callback.status = SentReport::Status::kRemovedFromQueue;
    }
    RunDeferredCallbacks();
  }

  void ShouldRunReportSentCallbacks(bool should_run_report_sent_callbacks) {
    should_run_report_sent_callbacks_ = should_run_report_sent_callbacks;
  }

  void SetSentReportStatus(SentReport::Status status) {
    sent_report_status_ = status;
  }

  const std::vector<AttributionReport>& added_reports() const {
    return added_reports_;
  }

  void WaitForNumReports(size_t expected_num_reports) {
    if (added_reports_.size() >= expected_num_reports)
      return;

    expected_num_reports_ = expected_num_reports;
    base::RunLoop wait_loop;
    quit_closure_ = wait_loop.QuitClosure();
    wait_loop.Run();
  }

  void SetReportSentCallback(
      base::RepeatingCallback<void(SentReport)> report_sent_callback) {
    report_sent_callback_ = std::move(report_sent_callback);
  }

 private:
  base::RepeatingCallback<void(SentReport)> report_sent_callback_;
  bool should_run_report_sent_callbacks_ = false;
  SentReport::Status sent_report_status_ = SentReport::Status::kSent;
  size_t expected_num_reports_ = 0u;
  std::vector<AttributionReport> added_reports_;
  base::OnceClosure quit_closure_;
  std::vector<SentReport> deferred_callbacks_;
};

// Time after impression that a conversion can first be sent. See
// AttributionStorageDelegateImpl::GetReportTimeForConversion().
constexpr base::TimeDelta kFirstReportingWindow = base::Days(2);

// Give impressions a sufficiently long expiry.
constexpr base::TimeDelta kImpressionExpiry = base::Days(30);

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
        task_environment_.GetMockClock(), dir_.GetPath(), mock_storage_policy_);
    test_reporter_->SetReportSentCallback(
        base::BindRepeating(&AttributionManagerImpl::OnReportSent,
                            base::Unretained(attribution_manager_.get())));
  }

  std::vector<StorableSource> StoredSources() {
    std::vector<StorableSource> result;
    base::RunLoop loop;
    attribution_manager_->GetActiveSourcesForWebUI(
        base::BindLambdaForTesting([&](std::vector<StorableSource> sources) {
          result = std::move(sources);
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  std::vector<AttributionReport> StoredReports() {
    std::vector<AttributionReport> result;
    base::RunLoop loop;
    attribution_manager_->GetPendingReportsForWebUI(
        base::BindLambdaForTesting([&](std::vector<AttributionReport> reports) {
          result = std::move(reports);
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  const base::Clock& clock() { return *task_environment_.GetMockClock(); }

 protected:
  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AttributionManagerImpl> attribution_manager_;
  raw_ptr<TestAttributionReporter> test_reporter_ = nullptr;
  scoped_refptr<storage::MockSpecialStoragePolicy> mock_storage_policy_;
};

TEST_F(AttributionManagerImplTest, ImpressionRegistered_ReturnedToWebUI) {
  auto impression = SourceBuilder(clock().Now())
                        .SetExpiry(kImpressionExpiry)
                        .SetSourceEventId(100)
                        .Build();
  attribution_manager_->HandleSource(impression);

  EXPECT_THAT(StoredSources(), ElementsAre(impression));
}

TEST_F(AttributionManagerImplTest, ExpiredImpression_NotReturnedToWebUI) {
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetExpiry(kImpressionExpiry)
                                         .SetSourceEventId(100)
                                         .Build());
  task_environment_.FastForwardBy(2 * kImpressionExpiry);

  EXPECT_THAT(StoredSources(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, ImpressionConverted_ReportReturnedToWebUI) {
  auto impression = SourceBuilder(clock().Now())
                        .SetExpiry(kImpressionExpiry)
                        .SetSourceEventId(100)
                        .Build();
  attribution_manager_->HandleSource(impression);

  auto conversion = DefaultTrigger();
  attribution_manager_->HandleTrigger(conversion);

  AttributionReport expected_report =
      ReportBuilder(impression)
          .SetTriggerData(conversion.trigger_data())
          .SetConversionTime(clock().Now())
          .SetReportTime(clock().Now() + kFirstReportingWindow)
          .Build();

  // The external report ID is randomly generated by the storage delegate,
  // so zero it out here to avoid flakiness.
  std::vector<AttributionReport> reports = StoredReports();
  for (auto& report : reports) {
    report.external_report_id = DefaultExternalReportID();
  }
  EXPECT_THAT(reports, ElementsAre(expected_report));
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
  EXPECT_THAT(test_reporter_->added_reports(), IsEmpty());

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, QueuedReportNotSent_NotQueuedAgain) {
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));

  // If the report is not sent, it should not be added to the queue again as
  // long as the reporter is still handling it.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest,
       QueuedReportFailedWithShouldRetry_QueuedAgain) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);
  test_reporter_->SetSentReportStatus(SentReport::Status::kTransientFailure);

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  // This is 3 instead of 1 because the failed report is directly added back
  // into the queue 2 times.
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(3));

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest,
       QueuedReportFailedWithoutShouldRetry_NotQueuedAgain) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);
  test_reporter_->SetSentReportStatus(SentReport::Status::kFailure);

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(StoredReports(), SizeIs(1));

  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  // Ensure that observers are notified after the report is deleted.
  EXPECT_CALL(observer, OnSourcesChanged).Times(0);
  EXPECT_CALL(observer, OnReportsChanged);

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));

  // If the report indicated retry, it should be added to the queue again.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportAlwaysFails_StopsSending) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(false);
  test_reporter_->SetSentReportStatus(SentReport::Status::kTransientFailure);

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  base::Time expected_report_time = clock().Now() + kFirstReportingWindow;

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval -
                                  base::Milliseconds(1));
  EXPECT_THAT(test_reporter_->added_reports(), IsEmpty());

  // The report is first in the queuing window.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(test_reporter_->added_reports(),
              ElementsAre(Field(&AttributionReport::report_time,
                                expected_report_time)));

  // Simulate the reporter sending the report only once the actual report time
  // has been reached.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  test_reporter_->RunDeferredCallbacks();

  test_reporter_->WaitForNumReports(2);
  // At this point, the report has been added directly to the reporter with the
  // updated report time of +5 minutes.
  expected_report_time += base::Minutes(5);
  EXPECT_THAT(test_reporter_->added_reports(),
              ElementsAre(_, Field(&AttributionReport::report_time,
                                   expected_report_time)));

  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_THAT(test_reporter_->added_reports(),
              ElementsAre(_, Field(&AttributionReport::report_time,
                                   expected_report_time)));
  test_reporter_->RunDeferredCallbacks();

  test_reporter_->WaitForNumReports(3);
  // At this point, the report has been added directly to the reporter with the
  // updated report time of +15 minutes.
  expected_report_time += base::Minutes(15);
  EXPECT_THAT(
      test_reporter_->added_reports(),
      ElementsAre(
          _, _, Field(&AttributionReport::report_time, expected_report_time)));

  task_environment_.FastForwardBy(base::Minutes(15));
  EXPECT_THAT(
      test_reporter_->added_reports(),
      ElementsAre(
          _, _, Field(&AttributionReport::report_time, expected_report_time)));
  test_reporter_->RunDeferredCallbacks();

  // At this point, the report has reached the maximum number of attempts and it
  // should no longer be present in the DB.
  EXPECT_THAT(StoredReports(), IsEmpty());

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportOffline_NoFailureIncrement) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);
  test_reporter_->SetSentReportStatus(SentReport::Status::kTransientFailure);

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  // This is 3 instead of 1 because the failed report is directly added back
  // into the queue 2 times.
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(3));

  test_reporter_->SetSentReportStatus(SentReport::Status::kOffline);
  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(3));

  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(3));

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
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, QueuedReportSent_NotQueuedAgain) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));

  // The report should not be added to the queue again.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));

  // kSent = 0.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 0, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportSent_ObserversNotified) {
  base::HistogramTester histograms;
  test_reporter_->ShouldRunReportSentCallbacks(true);

  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer,
              OnReportSent(Field(
                  &SentReport::report,
                  Field(&AttributionReport::impression,
                        Property(&StorableSource::source_event_id, 1u)))));
  EXPECT_CALL(observer,
              OnReportSent(Field(
                  &SentReport::report,
                  Field(&AttributionReport::impression,
                        Property(&StorableSource::source_event_id, 2u)))));
  EXPECT_CALL(observer,
              OnReportSent(Field(
                  &SentReport::report,
                  Field(&AttributionReport::impression,
                        Property(&StorableSource::source_event_id, 3u)))));

  test_reporter_->SetSentReportStatus(SentReport::Status::kSent);
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetSourceEventId(1)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  // This one should be stored, as its status is `kDropped`.
  test_reporter_->SetSentReportStatus(SentReport::Status::kDropped);
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetSourceEventId(2)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  test_reporter_->SetSentReportStatus(SentReport::Status::kSent);
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetSourceEventId(3)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  // This one shouldn't be stored, as it will be retried.
  test_reporter_->SetSentReportStatus(SentReport::Status::kTransientFailure);
  attribution_manager_->HandleSource(SourceBuilder(clock().Now())
                                         .SetSourceEventId(4)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  // kSent = 0.
  histograms.ExpectBucketCount("Conversion.ReportSendOutcome", 0, 2);
  // kFailed = 1.
  histograms.ExpectBucketCount("Conversion.ReportSendOutcome", 1, 1);
  // kDropped = 2.
  histograms.ExpectBucketCount("Conversion.ReportSendOutcome", 2, 1);
}

TEST_F(AttributionManagerImplTest, DroppedReport_ObserversNotified) {
  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(observer, OnReportDropped).Times(0);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(
        observer,
        OnReportDropped(
            AllOf(Property(&CreateReportResult::dropped_report,
                           Optional(Field(&AttributionReport::priority, 1))),
                  Property(&CreateReportResult::status,
                           CreateReportStatus::kSuccessDroppedLowerPriority))));

    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(observer,
                OnReportDropped(AllOf(
                    Property(&CreateReportResult::dropped_report,
                             Optional(Field(&AttributionReport::priority, -5))),
                    Property(&CreateReportResult::status,
                             CreateReportStatus::kPriorityTooLow))));

    EXPECT_CALL(checkpoint, Call(3));

    EXPECT_CALL(
        observer,
        OnReportDropped(
            AllOf(Property(&CreateReportResult::dropped_report,
                           Optional(Field(&AttributionReport::priority, 2))),
                  Property(&CreateReportResult::status,
                           CreateReportStatus::kSuccessDroppedLowerPriority))));
    EXPECT_CALL(
        observer,
        OnReportDropped(
            AllOf(Property(&CreateReportResult::dropped_report,
                           Optional(Field(&AttributionReport::priority, 3))),
                  Property(&CreateReportResult::status,
                           CreateReportStatus::kSuccessDroppedLowerPriority))));
  }

  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  EXPECT_THAT(StoredSources(), SizeIs(1));

  // `kNavigation` sources can have 3 reports, so none of these should result in
  // a dropped report.
  for (int i = 1; i <= 3; i++) {
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(i).Build());
    EXPECT_THAT(StoredReports(), SizeIs(i));
  }

  checkpoint.Call(1);

  {
    // This should replace the report with priority 1.
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(4).Build());
    EXPECT_THAT(StoredReports(), SizeIs(3));
  }

  checkpoint.Call(2);

  {
    // This should be dropped, as it has a lower priority than all stored
    // reports.
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(-5).Build());
    EXPECT_THAT(StoredReports(), SizeIs(3));
  }

  checkpoint.Call(3);

  {
    // These should replace the reports with priority 2 and 3.
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(5).Build());
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(6).Build());
    EXPECT_THAT(StoredReports(), SizeIs(3));
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

  EXPECT_THAT(test_reporter_->added_reports(), IsEmpty());

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
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));

  // The second report is still queued at the correct time.
  task_environment_.FastForwardBy(kAttributionManagerQueueReportsInterval);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(2));
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
    EXPECT_THAT(test_reporter_->added_reports(), SizeIs(expected_reports));
  }
}

TEST_F(AttributionManagerImplTest, ConversionsSentFromUI_ReportedImmediately) {
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(test_reporter_->added_reports(), IsEmpty());

  attribution_manager_->SendReportsForWebUI(base::DoNothing());
  task_environment_.FastForwardBy(base::Minutes(0));
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));
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
  EXPECT_THAT(test_reporter_->added_reports(), IsEmpty());

  // Reset the manager to simulate shutdown.
  attribution_manager_.reset();

  // Fast forward past the expected report time of the first conversion, t =
  // (kFirstReportingWindow+ 1 minute).
  task_environment_.FastForwardBy(kFirstReportingWindow + base::Minutes(1));

  CreateManager();
  test_reporter_->WaitForNumReports(1);

  // Ensure that the expired report is delayed based on the time the browser
  // started.
  EXPECT_THAT(test_reporter_->added_reports(),
              ElementsAre(Field(&AttributionReport::report_time,
                                start_time + kFirstReportingWindow +
                                    base::Minutes(1) + kExpiredReportOffset)));
}

TEST_F(AttributionManagerImplTest,
       NonExpiredReportsQueuedAtStartup_NotDelayed) {
  // Create a report that will be reported at t= 2 days.
  base::Time start_time = clock().Now();
  attribution_manager_->HandleSource(
      SourceBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(test_reporter_->added_reports(), IsEmpty());

  // Reset the manager to simulate shutdown.
  attribution_manager_.reset();

  // Fast forward just before the expected report time.
  task_environment_.FastForwardBy(kFirstReportingWindow - base::Minutes(1));

  // Ensure that this report does not receive additional delay.
  CreateManager();
  test_reporter_->WaitForNumReports(1);
  EXPECT_THAT(test_reporter_->added_reports(),
              ElementsAre(Field(&AttributionReport::report_time,
                                start_time + kFirstReportingWindow)));
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

  EXPECT_THAT(StoredSources(), SizeIs(1));
  EXPECT_THAT(StoredReports(), SizeIs(1));

  // Reset the manager to simulate shutdown.
  attribution_manager_.reset();
  CreateManager();

  EXPECT_THAT(StoredSources(), IsEmpty());
  EXPECT_THAT(StoredReports(), IsEmpty());
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

  EXPECT_THAT(StoredSources(), SizeIs(4));

  // Reset the manager to simulate shutdown.
  attribution_manager_.reset();
  CreateManager();

  // All session-only impressions should be deleted.
  EXPECT_THAT(StoredSources(), SizeIs(1));
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
  EXPECT_THAT(StoredSources(), SizeIs(1));

  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(1).Build());
  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(1).Build());
  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(1).Build());
  EXPECT_THAT(StoredReports(), SizeIs(3));

  task_environment_.FastForwardBy(base::Days(7) - base::Minutes(30));
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(3));

  task_environment_.FastForwardBy(base::Minutes(5));
  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(2).Build());
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(3));
}

TEST_F(AttributionManagerImplTest, HandleTrigger_RecordsMetric) {
  base::HistogramTester histograms;
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(StoredReports(), IsEmpty());
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
  EXPECT_THAT(StoredReports(), SizeIs(1));

  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  // Ensure that deleting a report notifies observers.
  EXPECT_CALL(observer, OnSourcesChanged).Times(0);
  EXPECT_CALL(observer, OnReportsChanged);

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  EXPECT_THAT(StoredReports(), IsEmpty());

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

  EXPECT_THAT(test_reporter_->added_reports(), IsEmpty());

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  test_reporter_->WaitForNumReports(2);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(2));

  attribution_manager_->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindLambdaForTesting(
          [&](const url::Origin& origin) { return origin == origin_a; }),
      base::DoNothing());

  test_reporter_->WaitForNumReports(3);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(3));
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

  EXPECT_THAT(StoredReports(), SizeIs(1));

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);

  test_reporter_->WaitForNumReports(1);
  EXPECT_THAT(test_reporter_->added_reports(), SizeIs(1));
  EXPECT_THAT(StoredReports(), SizeIs(1));

  attribution_manager_->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindLambdaForTesting(
          [&](const url::Origin& origin) { return origin == origin_b; }),
      base::DoNothing());

  EXPECT_THAT(StoredReports(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, HandleSource_NotifiesObservers) {
  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  auto source1 = SourceBuilder(clock().Now())
                     .SetExpiry(kImpressionExpiry)
                     .SetSourceEventId(7)
                     .Build();

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged).Times(0);
    EXPECT_CALL(observer, OnSourceDeactivated).Times(0);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged);
    EXPECT_CALL(observer, OnSourceDeactivated).Times(0);

    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged).Times(0);
    EXPECT_CALL(
        observer,
        OnSourceDeactivated(DeactivatedSource{
            source1, DeactivatedSource::Reason::kReplacedByNewerSource}));
  }

  attribution_manager_->HandleSource(source1);
  EXPECT_THAT(StoredSources(), SizeIs(1));
  checkpoint.Call(1);

  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(StoredReports(), SizeIs(1));
  checkpoint.Call(2);

  auto source2 = SourceBuilder(clock().Now())
                     .SetExpiry(kImpressionExpiry)
                     .SetSourceEventId(9)
                     .Build();
  attribution_manager_->HandleSource(source2);
  EXPECT_THAT(StoredSources(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, HandleTrigger_NotifiesObservers) {
  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  test_reporter_->ShouldRunReportSentCallbacks(true);

  auto source1 = SourceBuilder(clock().Now())
                     .SetExpiry(kImpressionExpiry)
                     .SetSourceEventId(7)
                     .Build();

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged).Times(0);
    EXPECT_CALL(observer, OnSourceDeactivated).Times(0);

    EXPECT_CALL(checkpoint, Call(1));

    // Each stored report should notify sources changed one time.
    for (size_t i = 1; i <= 3; i++) {
      EXPECT_CALL(observer, OnSourcesChanged);
      EXPECT_CALL(observer, OnReportsChanged);
    }
    EXPECT_CALL(observer, OnSourceDeactivated).Times(0);

    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(observer, OnReportsChanged).Times(3);
    EXPECT_CALL(checkpoint, Call(3));

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged);
    EXPECT_CALL(
        observer,
        OnSourceDeactivated(DeactivatedSource{
            source1, DeactivatedSource::Reason::kReachedAttributionLimit}));
  }

  attribution_manager_->HandleSource(source1);
  EXPECT_THAT(StoredSources(), SizeIs(1));
  checkpoint.Call(1);

  // Store the maximum number of reports for the source.
  for (size_t i = 1; i <= 3; i++) {
    attribution_manager_->HandleTrigger(DefaultTrigger());
    EXPECT_THAT(StoredReports(), SizeIs(i));
  }

  checkpoint.Call(2);

  // Simulate the reports being sent and removed from storage.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kAttributionManagerQueueReportsInterval);
  EXPECT_THAT(StoredReports(), IsEmpty());
  checkpoint.Call(3);

  // The next report should cause the source to be deactivated; the report
  // itself shouldn't be stored as we've already reached the maximum number of
  // conversions per source.
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(StoredReports(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, ClearData_NotifiesObservers) {
  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer, OnSourcesChanged);
  EXPECT_CALL(observer, OnReportsChanged);

  base::RunLoop run_loop;
  attribution_manager_->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating([](const url::Origin& _) { return false; }),
      run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace content
