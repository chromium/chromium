// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_impl.h"

#include <initializer_list>
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
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/event_attribution_report.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/storable_trigger.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
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
using ::testing::Expectation;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

using Checkpoint = ::testing::MockFunction<void(int step)>;

constexpr base::TimeDelta kExpiredReportOffset = base::Minutes(2);

class ConstantOfflineReportDelayPolicy : public AttributionPolicy {
 public:
  ConstantOfflineReportDelayPolicy() = default;
  ~ConstantOfflineReportDelayPolicy() override = default;

  absl::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const override {
    return OfflineReportDelayConfig{.min = kExpiredReportOffset,
                                    .max = kExpiredReportOffset};
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

  MOCK_METHOD(void,
              OnReportSent,
              (const EventAttributionReport& report, const SendResult& info),
              (override));

  MOCK_METHOD(void,
              OnReportDropped,
              (const AttributionStorage::CreateReportResult& result),
              (override));
};

// Time after impression that a conversion can first be sent. See
// AttributionStorageDelegateImpl::GetReportTimeForConversion().
constexpr base::TimeDelta kFirstReportingWindow = base::Days(2);

// Give impressions a sufficiently long expiry.
constexpr base::TimeDelta kImpressionExpiry = base::Days(30);

class MockNetworkSender : public AttributionManagerImpl::NetworkSender {
 public:
  // AttributionManagerImpl::NetworkSender:
  void SendReport(GURL report_url,
                  std::string report_body,
                  ReportSentCallback callback) override {
    calls_.emplace_back(std::move(report_url), std::move(report_body));
    callbacks_.push_back(std::move(callback));
  }

  using SendReportCalls = std::vector<std::pair<GURL, std::string>>;

  const SendReportCalls& calls() const { return calls_; }

  void RunCallback(size_t index, SendResult::Status status) {
    std::move(callbacks_[index])
        .Run(SendResult(status, /*http_response_code=*/0));
  }

  void Reset() {
    calls_.clear();
    callbacks_.clear();
  }

  void RunCallbacksAndReset(
      std::initializer_list<SendResult::Status> statuses) {
    ASSERT_THAT(callbacks_, SizeIs(statuses.size()));

    const auto* status_it = statuses.begin();

    for (auto& callback : callbacks_) {
      std::move(callback).Run(SendResult(*status_it, /*http_response_code=*/0));
      status_it++;
    }

    Reset();
  }

 private:
  SendReportCalls calls_;
  std::vector<ReportSentCallback> callbacks_;
};

}  // namespace

class AttributionManagerImplTest : public testing::Test {
 public:
  AttributionManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        browser_context_(std::make_unique<TestBrowserContext>()),
        mock_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()),
        network_sender_(new MockNetworkSender()) {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    CreateManager();
  }

  void CreateManager() {
    attribution_manager_ = absl::WrapUnique(new AttributionManagerImpl(
        static_cast<StoragePartitionImpl*>(
            browser_context_->GetDefaultStoragePartition()),
        network::TestNetworkConnectionTracker::GetInstance(), dir_.GetPath(),
        std::make_unique<ConstantOfflineReportDelayPolicy>(),
        mock_storage_policy_, absl::WrapUnique(network_sender_.get())));
  }

  void ShutdownManager() {
    // Allow the network sender to be reused across `CreateManager()`
    // invocations by ensuring that the manager doesn't destroy it.
    if (attribution_manager_) {
      attribution_manager_->network_sender_.release();
      attribution_manager_.reset();
    }
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

  std::vector<EventAttributionReport> StoredReports() {
    std::vector<EventAttributionReport> result;
    base::RunLoop loop;
    attribution_manager_->GetPendingReportsForWebUI(base::BindLambdaForTesting(
        [&](std::vector<EventAttributionReport> reports) {
          result = std::move(reports);
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  void ForceGetReportsToSend() { attribution_manager_->GetReportsToSend(); }

  void SetOfflineAndWaitForObserversToBeNotified(bool offline) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        offline ? network::mojom::ConnectionType::CONNECTION_NONE
                : network::mojom::ConnectionType::CONNECTION_UNKNOWN);
    // Ensure that the network connection observers have been notified before
    // this call returns.
    task_environment_.RunUntilIdle();
  }

 protected:
  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<storage::MockSpecialStoragePolicy> mock_storage_policy_;
  const raw_ptr<MockNetworkSender> network_sender_;

  std::unique_ptr<AttributionManagerImpl> attribution_manager_;
};

TEST_F(AttributionManagerImplTest, ImpressionRegistered_ReturnedToWebUI) {
  auto impression = SourceBuilder()
                        .SetExpiry(kImpressionExpiry)
                        .SetSourceEventId(100)
                        .Build();
  attribution_manager_->HandleSource(impression);

  EXPECT_THAT(StoredSources(), ElementsAre(impression));
}

TEST_F(AttributionManagerImplTest, ExpiredImpression_NotReturnedToWebUI) {
  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetSourceEventId(100)
                                         .Build());
  task_environment_.FastForwardBy(2 * kImpressionExpiry);

  EXPECT_THAT(StoredSources(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, ImpressionConverted_ReportReturnedToWebUI) {
  auto impression = SourceBuilder()
                        .SetExpiry(kImpressionExpiry)
                        .SetSourceEventId(100)
                        .Build();
  attribution_manager_->HandleSource(impression);

  auto conversion = DefaultTrigger();
  attribution_manager_->HandleTrigger(conversion);

  EventAttributionReport expected_report =
      ReportBuilder(impression)
          .SetTriggerData(conversion.trigger_data())
          .SetConversionTime(base::Time::Now())
          .SetReportTime(base::Time::Now() + kFirstReportingWindow)
          .Build();

  // The external report ID is randomly generated by the storage delegate,
  // so zero it out here to avoid flakiness.
  std::vector<EventAttributionReport> reports = StoredReports();
  for (auto& report : reports) {
    report.SetExternalReportIdForTesting(DefaultExternalReportID());
  }
  EXPECT_THAT(reports, ElementsAre(expected_report));
}

TEST_F(AttributionManagerImplTest, ImpressionConverted_ReportSent) {
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  // Make sure the report is not sent earlier than its report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Microseconds(1));
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  task_environment_.FastForwardBy(base::Microseconds(1));
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest,
       MultipleReportsWithSameReportTime_AllSentSimultaneously) {
  const GURL url_a(
      "https://a.example/.well-known/attribution-reporting/report-attribution");
  const GURL url_b(
      "https://b.example/.well-known/attribution-reporting/report-attribution");
  const GURL url_c(
      "https://c.example/.well-known/attribution-reporting/report-attribution");

  const auto origin_a = url::Origin::Create(url_a);
  const auto origin_b = url::Origin::Create(url_b);
  const auto origin_c = url::Origin::Create(url_c);

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_a)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_a).Build());

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_b)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_b).Build());

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_c)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_c).Build());

  EXPECT_THAT(StoredReports(), SizeIs(3));

  // Make sure the reports are not sent earlier than their report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Microseconds(1));
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  task_environment_.FastForwardBy(base::Microseconds(1));

  // The 3 reports can be sent in any order due to the `base::RandomShuffle()`
  // in `AttributionManagerImpl::OnGetReportsToSend()`.
  EXPECT_THAT(
      network_sender_->calls(),
      UnorderedElementsAre(Pair(url_a, _), Pair(url_b, _), Pair(url_c, _)));
}

TEST_F(AttributionManagerImplTest,
       MultipleReportsWithDifferentReportTimes_SentInSequence) {
  const GURL url_a(
      "https://a.example/.well-known/attribution-reporting/report-attribution");
  const GURL url_b(
      "https://b.example/.well-known/attribution-reporting/report-attribution");

  const auto origin_a = url::Origin::Create(url_a);
  const auto origin_b = url::Origin::Create(url_b);

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_a)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_a).Build());

  task_environment_.FastForwardBy(base::Microseconds(1));

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_b)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_b).Build());

  EXPECT_THAT(StoredReports(), SizeIs(2));

  // Make sure the reports are not sent earlier than their report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Microseconds(2));
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  task_environment_.FastForwardBy(base::Microseconds(1));
  EXPECT_THAT(network_sender_->calls(), ElementsAre(Pair(url_a, _)));
  network_sender_->Reset();

  task_environment_.FastForwardBy(base::Microseconds(1));
  EXPECT_THAT(network_sender_->calls(), ElementsAre(Pair(url_b, _)));
}

TEST_F(AttributionManagerImplTest, SenderStillHandlingReport_NotSentAgain) {
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->Reset();

  ForceGetReportsToSend();
  // The sender hasn't invoked the callback, so the manager shouldn't try to
  // send the report again.
  EXPECT_THAT(network_sender_->calls(), IsEmpty());
}

TEST_F(AttributionManagerImplTest,
       QueuedReportFailedWithShouldRetry_QueuedAgain) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->RunCallbacksAndReset(
      {SendResult::Status::kTransientFailure});

  // First report delay.
  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->RunCallbacksAndReset(
      {SendResult::Status::kTransientFailure});

  // Second report delay.
  task_environment_.FastForwardBy(base::Minutes(15));
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->RunCallbacksAndReset(
      {SendResult::Status::kTransientFailure});

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest, RetryLogicOverridesGetReportTimer) {
  const GURL url_a(
      "https://a.example/.well-known/attribution-reporting/report-attribution");
  const GURL url_b(
      "https://b.example/.well-known/attribution-reporting/report-attribution");

  const auto origin_a = url::Origin::Create(url_a);
  const auto origin_b = url::Origin::Create(url_b);

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_a)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_a).Build());

  task_environment_.FastForwardBy(base::Minutes(10));
  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_b)
                                         .Build());
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_b).Build());

  EXPECT_THAT(StoredReports(), SizeIs(2));

  task_environment_.FastForwardBy(kFirstReportingWindow - base::Minutes(10));
  EXPECT_THAT(network_sender_->calls(), ElementsAre(Pair(url_a, _)));
  // Because this report will be retried at its original report time + 5
  // minutes, the get-reports timer, which was originally scheduled to run at
  // the second report's report time, should be overridden to run earlier.
  network_sender_->RunCallbacksAndReset(
      {SendResult::Status::kTransientFailure});

  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_THAT(network_sender_->calls(), ElementsAre(Pair(url_a, _)));
}

TEST_F(AttributionManagerImplTest,
       QueuedReportFailedWithoutShouldRetry_NotQueuedAgain) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(StoredReports(), SizeIs(1));

  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  // Ensure that observers are notified after the report is deleted.
  EXPECT_CALL(observer, OnSourcesChanged).Times(0);
  EXPECT_CALL(observer, OnReportsChanged);

  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->RunCallbacksAndReset({SendResult::Status::kFailure});

  EXPECT_THAT(StoredReports(), IsEmpty());

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportAlwaysFails_StopsSending) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Milliseconds(1));
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  // The report is sent at its expected report time.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->RunCallbacksAndReset(
      {SendResult::Status::kTransientFailure});

  // The report is sent at the first retry time of +5 minutes.
  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->RunCallbacksAndReset(
      {SendResult::Status::kTransientFailure});

  // The report is sent at the second retry time of +15 minutes.
  task_environment_.FastForwardBy(base::Minutes(15));
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->RunCallbacksAndReset(
      {SendResult::Status::kTransientFailure});

  // At this point, the report has reached the maximum number of attempts and it
  // should no longer be present in the DB.
  EXPECT_THAT(StoredReports(), IsEmpty());

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 1, 1);
}

TEST_F(AttributionManagerImplTest, ReportExpiredAtStartup_Sent) {
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  ShutdownManager();

  // Fast-forward past the reporting window and past report expiry.
  task_environment_.FastForwardBy(kFirstReportingWindow);
  task_environment_.FastForwardBy(base::Days(100));
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  // Simulate startup and ensure the report is sent before being expired.
  CreateManager();
  task_environment_.FastForwardBy(kExpiredReportOffset);
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, ReportSent_Deleted) {
  base::HistogramTester histograms;
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->RunCallbacksAndReset({SendResult::Status::kSent});

  EXPECT_THAT(StoredReports(), IsEmpty());
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  // kSent = 0.
  histograms.ExpectUniqueSample("Conversion.ReportSendOutcome", 0, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportSent_ObserversNotified) {
  base::HistogramTester histograms;

  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(
      observer,
      OnReportSent(Property(&EventAttributionReport::source,
                            Property(&StorableSource::source_event_id, 1u)),
                   _));
  EXPECT_CALL(
      observer,
      OnReportSent(Property(&EventAttributionReport::source,
                            Property(&StorableSource::source_event_id, 2u)),
                   _));
  EXPECT_CALL(
      observer,
      OnReportSent(Property(&EventAttributionReport::source,
                            Property(&StorableSource::source_event_id, 3u)),
                   _));

  attribution_manager_->HandleSource(
      SourceBuilder().SetSourceEventId(1).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow);

  // This one should be stored, as its status is `kDropped`.
  attribution_manager_->HandleSource(
      SourceBuilder().SetSourceEventId(2).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow);

  attribution_manager_->HandleSource(
      SourceBuilder().SetSourceEventId(3).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow);

  // This one shouldn't be stored, as it will be retried.
  attribution_manager_->HandleSource(
      SourceBuilder().SetSourceEventId(4).SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  task_environment_.FastForwardBy(kFirstReportingWindow);

  EXPECT_THAT(network_sender_->calls(), SizeIs(4));
  network_sender_->RunCallbacksAndReset(
      {SendResult::Status::kSent, SendResult::Status::kDropped,
       SendResult::Status::kSent, SendResult::Status::kTransientFailure});

  // kSent = 0.
  histograms.ExpectBucketCount("Conversion.ReportSendOutcome", 0, 2);
  // kFailed = 1.
  histograms.ExpectBucketCount("Conversion.ReportSendOutcome", 1, 0);
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
        OnReportDropped(AllOf(
            Property(&CreateReportResult::dropped_report,
                     Optional(Property(&EventAttributionReport::priority, 1))),
            Property(&CreateReportResult::status,
                     CreateReportStatus::kSuccessDroppedLowerPriority))));

    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(
        observer,
        OnReportDropped(AllOf(
            Property(&CreateReportResult::dropped_report,
                     Optional(Property(&EventAttributionReport::priority, -5))),
            Property(&CreateReportResult::status,
                     CreateReportStatus::kPriorityTooLow))));

    EXPECT_CALL(checkpoint, Call(3));

    EXPECT_CALL(
        observer,
        OnReportDropped(AllOf(
            Property(&CreateReportResult::dropped_report,
                     Optional(Property(&EventAttributionReport::priority, 2))),
            Property(&CreateReportResult::status,
                     CreateReportStatus::kSuccessDroppedLowerPriority))));
    EXPECT_CALL(
        observer,
        OnReportDropped(AllOf(
            Property(&CreateReportResult::dropped_report,
                     Optional(Property(&EventAttributionReport::priority, 3))),
            Property(&CreateReportResult::status,
                     CreateReportStatus::kSuccessDroppedLowerPriority))));
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
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

// This functionality is tested more thoroughly in the AttributionStorageSql
// unit tests. Here, just test to make sure the basic control flow is working.
TEST_F(AttributionManagerImplTest, ClearData) {
  for (bool match_url : {true, false}) {
    base::Time start = base::Time::Now();
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

    size_t expected_reports = match_url ? 0u : 1u;
    EXPECT_THAT(StoredReports(), SizeIs(expected_reports));
  }
}

TEST_F(AttributionManagerImplTest, ConversionsSentFromUI_ReportedImmediately) {
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  std::vector<EventAttributionReport> reports = StoredReports();
  EXPECT_THAT(reports, SizeIs(1));
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  attribution_manager_->SendReportsForWebUI({*reports.front().report_id()},
                                            base::DoNothing());
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest,
       ConversionsSentFromUI_CallbackInvokedWhenAllDone) {
  size_t callback_calls = 0;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  std::vector<EventAttributionReport> reports = StoredReports();
  EXPECT_THAT(reports, SizeIs(2));
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  attribution_manager_->SendReportsForWebUI(
      {*reports.front().report_id(), *reports.back().report_id()},
      base::BindLambdaForTesting([&]() { callback_calls++; }));
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_THAT(network_sender_->calls(), SizeIs(2));
  EXPECT_EQ(callback_calls, 0u);

  network_sender_->RunCallback(0, SendResult::Status::kSent);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(callback_calls, 0u);

  network_sender_->RunCallback(1, SendResult::Status::kTransientFailure);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(callback_calls, 1u);
}

TEST_F(AttributionManagerImplTest, ExpiredReportsAtStartup_Delayed) {
  base::Time start_time = base::Time::Now();

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  ShutdownManager();

  // Fast forward past the expected report time of the first conversion.
  task_environment_.FastForwardBy(kFirstReportingWindow +
                                  base::Milliseconds(1));

  CreateManager();

  // Ensure that the expired report is delayed based on the time the browser
  // started.
  EXPECT_THAT(
      StoredReports(),
      ElementsAre(Property(&EventAttributionReport::report_time,
                           start_time + kFirstReportingWindow +
                               base::Milliseconds(1) + kExpiredReportOffset)));

  EXPECT_THAT(network_sender_->calls(), IsEmpty());
}

TEST_F(AttributionManagerImplTest,
       NonExpiredReportsQueuedAtStartup_NotDelayed) {
  base::Time start_time = base::Time::Now();

  // Create a report that will be reported at t= 2 days.
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  ShutdownManager();

  // Fast forward just before the expected report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Milliseconds(1));

  CreateManager();

  // Ensure that this report does not receive additional delay.
  EXPECT_THAT(StoredReports(),
              ElementsAre(Property(&EventAttributionReport::report_time,
                                   start_time + kFirstReportingWindow)));

  EXPECT_THAT(network_sender_->calls(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, SessionOnlyOrigins_DataDeletedAtShutdown) {
  GURL session_only_origin("https://sessiononly.example");
  auto impression =
      SourceBuilder()
          .SetImpressionOrigin(url::Origin::Create(session_only_origin))
          .Build();

  mock_storage_policy_->AddSessionOnly(session_only_origin);

  attribution_manager_->HandleSource(impression);
  attribution_manager_->HandleTrigger(DefaultTrigger());

  EXPECT_THAT(StoredSources(), SizeIs(1));
  EXPECT_THAT(StoredReports(), SizeIs(1));

  ShutdownManager();
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
  auto impression1 =
      SourceBuilder().SetImpressionOrigin(session_only_origin).Build();
  auto impression2 =
      SourceBuilder().SetReportingOrigin(session_only_origin).Build();
  auto impression3 =
      SourceBuilder().SetConversionOrigin(session_only_origin).Build();

  // Create one  impression which is not session only.
  auto impression4 = SourceBuilder().Build();

  mock_storage_policy_->AddSessionOnly(session_only_origin.GetURL());

  attribution_manager_->HandleSource(impression1);
  attribution_manager_->HandleSource(impression2);
  attribution_manager_->HandleSource(impression3);
  attribution_manager_->HandleSource(impression4);

  EXPECT_THAT(StoredSources(), SizeIs(4));

  ShutdownManager();
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
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(base::Days(7)).Build());
  EXPECT_THAT(StoredSources(), SizeIs(1));

  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(1).Build());
  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(1).Build());
  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(1).Build());
  EXPECT_THAT(StoredReports(), SizeIs(3));

  task_environment_.FastForwardBy(base::Days(7) - base::Minutes(30));
  EXPECT_THAT(network_sender_->calls(), SizeIs(3));
  network_sender_->RunCallbacksAndReset({SendResult::Status::kSent,
                                         SendResult::Status::kSent,
                                         SendResult::Status::kSent});

  task_environment_.FastForwardBy(base::Minutes(5));
  attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(2).Build());
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_THAT(network_sender_->calls(), IsEmpty());
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
  base::HistogramTester histograms;
  attribution_manager_->HandleSource(SourceBuilder().Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(StoredReports(), SizeIs(1));

  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  // Ensure that deleting a report notifies observers.
  EXPECT_CALL(observer, OnSourcesChanged).Times(0);
  EXPECT_CALL(observer, OnReportsChanged);

  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
  network_sender_->RunCallbacksAndReset({SendResult::Status::kSent});
  EXPECT_THAT(StoredReports(), IsEmpty());

  static constexpr char kMetric[] = "Conversions.DeleteSentReportOperation";
  histograms.ExpectTotalCount(kMetric, 2);
  histograms.ExpectBucketCount(
      kMetric, AttributionManagerImpl::DeleteEvent::kStarted, 1);
  histograms.ExpectBucketCount(
      kMetric, AttributionManagerImpl::DeleteEvent::kSucceeded, 1);
}

TEST_F(AttributionManagerImplTest, HandleSource_NotifiesObservers) {
  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  auto source1 =
      SourceBuilder().SetExpiry(kImpressionExpiry).SetSourceEventId(7).Build();

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

  auto source2 =
      SourceBuilder().SetExpiry(kImpressionExpiry).SetSourceEventId(9).Build();
  attribution_manager_->HandleSource(source2);
  EXPECT_THAT(StoredSources(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, HandleTrigger_NotifiesObservers) {
  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  auto source1 =
      SourceBuilder().SetExpiry(kImpressionExpiry).SetSourceEventId(7).Build();

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
  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(network_sender_->calls(), SizeIs(3));
  network_sender_->RunCallbacksAndReset({SendResult::Status::kSent,
                                         SendResult::Status::kSent,
                                         SendResult::Status::kSent});
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

TEST_F(AttributionManagerImplTest, EmbedderDisallowsReporting_ReportNotSent) {
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

  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(StoredReports(), SizeIs(1));

  MockAttributionManagerObserver observer;
  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation(&observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer, OnReportSent(_, Field(&SendResult::status,
                                              SendResult::Status::kDropped)));

  task_environment_.FastForwardBy(kFirstReportingWindow);

  EXPECT_THAT(StoredReports(), IsEmpty());
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  // kDropped = 2.
  histograms.ExpectBucketCount("Conversion.ReportSendOutcome", 2, 1);
}

TEST_F(AttributionManagerImplTest, Offline_NoReportSent) {
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());
  EXPECT_THAT(StoredReports(), SizeIs(1));

  SetOfflineAndWaitForObserversToBeNotified(true);
  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  SetOfflineAndWaitForObserversToBeNotified(false);
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, TimeFromConversionToReportSendHistogram) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));

  histograms.ExpectUniqueSample("Conversions.TimeFromConversionToReportSend",
                                kFirstReportingWindow.InHours(), 1);
}

TEST_F(AttributionManagerImplTest, SendReport_RecordsExtraReportDelay2) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  // Prevent the report from being sent until after its original report time.
  SetOfflineAndWaitForObserversToBeNotified(true);
  task_environment_.FastForwardBy(kFirstReportingWindow + base::Days(3));
  EXPECT_THAT(network_sender_->calls(), IsEmpty());

  SetOfflineAndWaitForObserversToBeNotified(false);
  task_environment_.FastForwardBy(kExpiredReportOffset);
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));

  histograms.ExpectUniqueTimeSample("Conversions.ExtraReportDelay2",
                                    base::Days(3) + kExpiredReportOffset, 1);
}

TEST_F(AttributionManagerImplTest, SendReportsFromWebUI_DoesNotRecordMetrics) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build());
  attribution_manager_->HandleTrigger(DefaultTrigger());

  attribution_manager_->SendReportsForWebUI({EventAttributionReport::Id(1)},
                                            base::DoNothing());
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_THAT(network_sender_->calls(), SizeIs(1));

  histograms.ExpectTotalCount("Conversions.ExtraReportDelay2", 0);
  histograms.ExpectTotalCount("Conversions.TimeFromConversionToReportSend", 0);
}

}  // namespace content
