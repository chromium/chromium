// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_impl.h"

#include <stddef.h>

#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/debug_types.mojom.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/registrar.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_features.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_sender.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_resolver.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
#include "content/browser/attribution_reporting/test/mock_attribution_observer.h"
#include "content/browser/attribution_reporting/test/mock_content_browser_client.h"
#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::AggregatableDebugReportingConfig;
using ::attribution_reporting::AggregatableValues;
using ::attribution_reporting::AggregatableValuesValue;
using ::attribution_reporting::OsRegistrationItem;
using ::attribution_reporting::SourceAggregatableDebugReportingConfig;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::DebugDataType;
using ::attribution_reporting::mojom::OsRegistrationResult;

using SentResult = ::content::SendResult::Sent::Result;

using ::testing::_;
using ::testing::AllOf;
using ::testing::An;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Expectation;
using ::testing::Field;
using ::testing::Ge;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Le;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

using AttributionReportingOperation =
    ::content::ContentBrowserClient::AttributionReportingOperation;

using Checkpoint = ::testing::MockFunction<void(int step)>;

using DebugReportSentCallback =
    ::content::AttributionReportSender::DebugReportSentCallback;
using ReportSentCallback =
    ::content::AttributionReportSender::ReportSentCallback;
using AggregatableDebugReportSentCallback =
    ::content::AttributionReportSender::AggregatableDebugReportSentCallback;

constexpr size_t kMaxPendingEvents = 5;
constexpr size_t kMaxPendingReportsTimings = 50;

const GlobalRenderFrameHostId kFrameId = {0, 1};
constexpr attribution_reporting::Registrar kRegistrar =
    attribution_reporting::Registrar::kWeb;

constexpr AttributionResolverDelegate::OfflineReportDelayConfig
    kDefaultOfflineReportDelay{
        .min = base::Minutes(0),
        .max = base::Minutes(1),
    };

const base::TimeDelta kPrivacySandboxAttestationsTimeout = base::Minutes(5);

constexpr char kPendingAndBrowserWentOfflineTimeSinceCreation[] =
    "Conversions.AggregatableReport.PendingAndBrowserWentOffline."
    "TimeSinceCreation";
constexpr char kPendingAndBrowserWentOfflineTimeUntilReportTime[] =
    "Conversions.AggregatableReport.PendingAndBrowserWentOffline."
    "TimeUntilReportTime";

constexpr char kSentVerboseDebugReportTypeMetric[] =
    "Conversions.SentVerboseDebugReportType4";

auto InvokeReportSentCallback(SentResult result) {
  return [=](AttributionReport report, bool is_debug_report,
             ReportSentCallback callback) {
    std::move(callback).Run(std::move(report),
                            SendResult::Sent(result, /*status=*/0));
  };
}

AggregatableReport CreateExampleAggregatableReport() {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/std::nullopt);
  payloads.emplace_back(/*payload=*/kEFGH5678AsBytes,
                        /*key_id=*/"key_2",
                        /*debug_cleartext_payload=*/std::nullopt);

  base::Value::Dict additional_fields;
  additional_fields.Set("source_registration_time", "1234569600");
  additional_fields.Set(
      "attribution_destination",
      url::Origin::Create(GURL("https://example.destination")).Serialize());
  AggregatableReportSharedInfo shared_info(
      base::Time::FromMillisecondsSinceUnixEpoch(1234567890123),
      DefaultExternalReportID(),
      /*reporting_origin=*/
      url::Origin::Create(GURL("https://example.reporting")),
      AggregatableReportSharedInfo::DebugMode::kDisabled,
      std::move(additional_fields),
      /*api_version=*/"",
      /*api_identifier=*/"attribution-reporting");

  return AggregatableReport(std::move(payloads), shared_info.SerializeAsJson(),
                            /*debug_key=*/std::nullopt,
                            /*additional_fields=*/{},
                            /*aggregation_coordinator_origin=*/std::nullopt);
}

AggregatableReport CreateExampleAggregatableDebugReport() {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/std::nullopt);
  payloads.emplace_back(/*payload=*/kEFGH5678AsBytes,
                        /*key_id=*/"key_2",
                        /*debug_cleartext_payload=*/std::nullopt);

  base::Value::Dict additional_fields;
  additional_fields.Set(
      "attribution_destination",
      url::Origin::Create(GURL("https://example.destination")).Serialize());
  AggregatableReportSharedInfo shared_info(
      base::Time::FromMillisecondsSinceUnixEpoch(1234567890123),
      DefaultExternalReportID(),
      /*reporting_origin=*/
      url::Origin::Create(GURL("https://example.reporting")),
      AggregatableReportSharedInfo::DebugMode::kDisabled,
      std::move(additional_fields),
      /*api_version=*/"0.1",
      /*api_identifier=*/"attribution-reporting-debug");

  return AggregatableReport(std::move(payloads), shared_info.SerializeAsJson(),
                            /*debug_key=*/std::nullopt,
                            /*additional_fields=*/{},
                            /*aggregation_coordinator_origin=*/std::nullopt);
}

// Time after impression that a conversion can first be sent. See
// AttributionResolverDelegateImpl::GetReportTimeForConversion().
constexpr base::TimeDelta kFirstReportingWindow = base::Days(2);

// Give impressions a sufficiently long expiry.
constexpr base::TimeDelta kImpressionExpiry = base::Days(30);

class MockReportSender : public AttributionReportSender {
 public:
  MOCK_METHOD(void,
              SendReport,
              (AttributionReport report,
               bool is_debug_report,
               ReportSentCallback callback),
              (override));

  MOCK_METHOD(void,
              SendReport,
              (AttributionDebugReport report, DebugReportSentCallback callback),
              (override));

  MOCK_METHOD(void,
              SendReport,
              (AggregatableDebugReport,
               base::Value::Dict report_body,
               AggregatableDebugReportSentCallback),
              (override));
};

class MockCookieChecker : public AttributionCookieChecker {
 public:
  ~MockCookieChecker() override { EXPECT_THAT(callbacks_, IsEmpty()); }

  void AddOriginWithDebugCookieSet(url::Origin origin) {
    origins_with_debug_cookie_set_.insert(std::move(origin));
  }

  void DeferCallbacks(bool defer = true) { defer_callbacks_ = defer; }

  void RunNextDeferredCallback(bool is_debug_cookie_set) {
    if (!callbacks_.empty()) {
      Callback callback = std::move(callbacks_.front());
      callbacks_.pop_front();
      std::move(callback).Run(is_debug_cookie_set);
    }
  }

 private:
  // AttributionCookieChecker:
  void IsDebugCookieSet(const url::Origin& origin, Callback callback) override {
    if (defer_callbacks_) {
      callbacks_.emplace_back(std::move(callback));
    } else {
      std::move(callback).Run(origins_with_debug_cookie_set_.contains(origin));
    }
  }

  base::flat_set<url::Origin> origins_with_debug_cookie_set_;

  bool defer_callbacks_ = false;
  base::circular_deque<Callback> callbacks_;
};

class MockAttributionOsLevelManager : public AttributionOsLevelManager {
 public:
  ~MockAttributionOsLevelManager() override = default;

  MOCK_METHOD(void,
              Register,
              (OsRegistration,
               const std::vector<bool>& is_debug_key_allowed,
               RegisterCallback callback),
              (override));

  MOCK_METHOD(void,
              ClearData,
              (base::Time delete_begin,
               base::Time delete_end,
               const std::set<url::Origin>& origins,
               const std::set<std::string>& domains,
               BrowsingDataFilterBuilder::Mode mode,
               bool delete_rate_limit_data,
               base::OnceClosure done),
              (override));
};

}  // namespace

class AttributionManagerImplTest : public testing::Test {
 public:
  AttributionManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        browser_context_(std::make_unique<TestBrowserContext>()),
        mock_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {
    // This UMA records a sample every 30s via a periodic task which
    // interacts poorly with TaskEnvironment::FastForward using day long
    // delays (we need to run the uma update every 30s for that
    // interval)
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{network::features::kGetCookiesStringUma,
                               kAttributionReportDeliveryThirdRetryAttempt});
  }

  void SetUp() override {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());

    content::SetNetworkConnectionTrackerForTesting(
        network::TestNetworkConnectionTracker::GetInstance());

    storage_task_runner_ =
        base::ThreadPool::CreateUpdateableSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
             base::ThreadPolicy::MUST_USE_FOREGROUND});

    CreateManager();
    CreateAggregationService();
  }

  void CreateManager() {
    CHECK(!attribution_manager_);

    auto storage_delegate = std::make_unique<ConfigurableStorageDelegate>();

    storage_delegate->set_report_delay(kFirstReportingWindow);
    storage_delegate->set_offline_report_delay_config(
        kDefaultOfflineReportDelay);

    ConfigureStorageDelegate(*storage_delegate);
    // From this point on, the delegate will only be accessed on storage's
    // sequence.
    storage_delegate->DetachFromSequence();

    auto cookie_checker = std::make_unique<MockCookieChecker>();
    cookie_checker_ = cookie_checker.get();

    auto report_sender = std::make_unique<MockReportSender>();
    report_sender_ = report_sender.get();

    auto os_level_manager = std::make_unique<MockAttributionOsLevelManager>();
    os_level_manager_ = os_level_manager.get();

    ON_CALL(*os_level_manager_, ClearData)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<6>());

    attribution_manager_ = AttributionManagerImpl::CreateForTesting(
        dir_.GetPath(), kMaxPendingEvents, mock_storage_policy_,
        std::move(storage_delegate), std::move(cookie_checker),
        std::move(report_sender), std::move(os_level_manager),
        static_cast<StoragePartitionImpl*>(
            browser_context_->GetDefaultStoragePartition()),
        storage_task_runner_);
  }

  void ShutdownManager() {
    cookie_checker_ = nullptr;
    report_sender_ = nullptr;
    os_level_manager_ = nullptr;
    attribution_manager_.reset();
  }

  void CreateAggregationService() {
    auto* partition = static_cast<StoragePartitionImpl*>(
        browser_context_->GetDefaultStoragePartition());
    auto aggregation_service = std::make_unique<MockAggregationService>();
    aggregation_service_ = aggregation_service.get();
    partition->OverrideAggregationServiceForTesting(
        std::move(aggregation_service));
  }

  void ShutdownAggregationService() {
    auto* partition = static_cast<StoragePartitionImpl*>(
        browser_context_->GetDefaultStoragePartition());
    aggregation_service_ = nullptr;
    partition->OverrideAggregationServiceForTesting(nullptr);
  }

  void RegisterAggregatableSourceAndMatchingTrigger(
      std::string_view origin_prefix) {
    const auto origin = *SuitableOrigin::Deserialize(
        base::StrCat({"https://", origin_prefix, ".example"}));

    attribution_manager_->HandleSource(TestAggregatableSourceProvider()
                                           .GetBuilder()
                                           .SetExpiry(kImpressionExpiry)
                                           .SetReportingOrigin(origin)
                                           .Build(),
                                       kFrameId);
    attribution_manager_->HandleTrigger(
        DefaultAggregatableTriggerBuilder().SetReportingOrigin(origin).Build(),
        kFrameId);
  }

  std::vector<StoredSource> StoredSources() {
    std::vector<StoredSource> result;
    base::RunLoop loop;
    attribution_manager_->GetActiveSourcesForWebUI(
        base::BindLambdaForTesting([&](std::vector<StoredSource> sources) {
          result = std::move(sources);
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  std::vector<AttributionReport> StoredReports() {
    std::vector<AttributionReport> result;
    base::RunLoop run_loop;
    attribution_manager_->GetPendingReportsForInternalUse(
        /*limit=*/-1,
        base::BindLambdaForTesting([&](std::vector<AttributionReport> reports) {
          result = std::move(reports);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void ForceGetReportsToSend() { attribution_manager_->GetReportsToSend(); }

  void SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
    // Ensure that the network connection observers have been notified before
    // this call returns.
    task_environment_.RunUntilIdle();
  }

  void ExpectOperationAllowed(
      MockAttributionReportingContentBrowserClient& browser_client,
      AttributionReportingOperation operation,
      ::testing::Matcher<const url::Origin*> source_origin,
      ::testing::Matcher<const url::Origin*> destination_origin,
      const url::Origin& reporting_origin,
      bool allowed) {
    EXPECT_CALL(
        browser_client,
        IsAttributionReportingOperationAllowed(
            /*browser_context=*/_, operation, /*rfh=*/_, source_origin,
            destination_origin, Pointee(reporting_origin), /*can_bypass=*/_))
        .WillOnce(Return(allowed));
  }

  void NotifyAttestationsLoaded() {
    attribution_manager_->OnAttestationsLoaded();
  }

 protected:
  // Override this in order to modify the delegate before it is passed
  // irretrievably to storage.
  virtual void ConfigureStorageDelegate(ConfigurableStorageDelegate&) const {}

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<AttributionManagerImpl> attribution_manager_;
  scoped_refptr<storage::MockSpecialStoragePolicy> mock_storage_policy_;
  raw_ptr<MockCookieChecker> cookie_checker_;
  raw_ptr<MockReportSender> report_sender_;
  raw_ptr<MockAttributionOsLevelManager> os_level_manager_;
  raw_ptr<MockAggregationService> aggregation_service_;
  scoped_refptr<base::UpdateableSequencedTaskRunner> storage_task_runner_;
};

TEST_F(AttributionManagerImplTest, ImpressionRegistered_ReturnedToWebUI) {
  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  EXPECT_THAT(StoredSources(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, ExpiredImpression_NotReturnedToWebUI) {
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  task_environment_.FastForwardBy(kImpressionExpiry);

  EXPECT_THAT(StoredSources(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, ImpressionConverted_ReportReturnedToWebUI) {
  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  EXPECT_THAT(StoredReports(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, ImpressionConverted_ReportSent) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _));
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  // Make sure the report is not sent earlier than its report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Microseconds(1));

  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Microseconds(1));
}

TEST_F(AttributionManagerImplTest,
       MultipleReportsWithSameReportTime_AllSentSimultaneously) {
  const GURL url_a(
      "https://a.example/.well-known/attribution-reporting/"
      "report-event-attribution");
  const GURL url_b(
      "https://b.example/.well-known/attribution-reporting/"
      "report-event-attribution");
  const GURL url_c(
      "https://c.example/.well-known/attribution-reporting/"
      "report-event-attribution");

  const auto origin_a = *SuitableOrigin::Create(url_a);
  const auto origin_b = *SuitableOrigin::Create(url_b);
  const auto origin_c = *SuitableOrigin::Create(url_c);

  std::vector<AttributionReport> sent_reports;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillRepeatedly([&](AttributionReport report, bool is_debug_report,
                            ReportSentCallback callback) {
          sent_reports.push_back(std::move(report));
        });
  }

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_a)
                                         .Build(),
                                     kFrameId);
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_a).Build(), kFrameId);

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_b)
                                         .Build(),
                                     kFrameId);
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_b).Build(), kFrameId);

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_c)
                                         .Build(),
                                     kFrameId);
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_c).Build(), kFrameId);

  EXPECT_THAT(StoredReports(), SizeIs(3));

  // Make sure the reports are not sent earlier than their report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Microseconds(1));

  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Microseconds(1));

  // The 3 reports can be sent in any order due to the `base::RandomShuffle()`
  // in `AttributionManagerImpl::OnGetReportsToSend()`.
  EXPECT_THAT(sent_reports,
              UnorderedElementsAre(ReportURLIs(url_a), ReportURLIs(url_b),
                                   ReportURLIs(url_c)));
}

TEST_F(AttributionManagerImplTest, SenderStillHandlingReport_NotSentAgain) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .Times(0);
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  task_environment_.FastForwardBy(kFirstReportingWindow);

  checkpoint.Call(1);

  // The sender hasn't invoked the callback, so the manager shouldn't try to
  // send the report again.
  ForceGetReportsToSend();
}

TEST_F(AttributionManagerImplTest,
       QueuedReportFailedWithShouldRetry_QueuedAgain) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  task_environment_.FastForwardBy(kFirstReportingWindow);

  checkpoint.Call(1);

  // First report delay.
  task_environment_.FastForwardBy(base::Minutes(5));

  checkpoint.Call(2);

  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_2G);

  // Second report delay.
  task_environment_.FastForwardBy(base::Minutes(15));

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversions.ReportSendOutcome3", 1, 1);

  histograms.ExpectTotalCount(
      "Conversions.TimeFromTriggerToReportSentSuccessfully", 0);

  static constexpr char kNetworkConnectionTypeOnFailureHistogram[] =
      "Conversions.EventLevelReport.NetworkConnectionTypeOnFailure";

  histograms.ExpectBucketCount(
      kNetworkConnectionTypeOnFailureHistogram,
      network::mojom::ConnectionType::CONNECTION_UNKNOWN, 2);
  histograms.ExpectBucketCount(kNetworkConnectionTypeOnFailureHistogram,
                               network::mojom::ConnectionType::CONNECTION_2G,
                               1);
}

TEST_F(AttributionManagerImplTest, RetryLogicOverridesGetReportTimer) {
  const GURL url_a(
      "https://a.example/.well-known/attribution-reporting/"
      "report-event-attribution");
  const GURL url_b(
      "https://b.example/.well-known/attribution-reporting/"
      "report-event-attribution");

  const auto origin_a = *SuitableOrigin::Create(url_a);
  const auto origin_b = *SuitableOrigin::Create(url_b);

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_,
                SendReport(ReportURLIs(url_a), /*is_debug_report=*/false, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_,
                SendReport(ReportURLIs(url_a), /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*report_sender_,
                SendReport(ReportURLIs(url_a), /*is_debug_report=*/false, _));
  }

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_a)
                                         .Build(),
                                     kFrameId);
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_a).Build(), kFrameId);

  task_environment_.FastForwardBy(base::Minutes(10));
  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetReportingOrigin(origin_b)
                                         .Build(),
                                     kFrameId);
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin_b).Build(), kFrameId);

  EXPECT_THAT(StoredReports(), SizeIs(2));

  checkpoint.Call(1);

  // Because this report will be retried at its original report time + 5
  // minutes, the get-reports timer, which was originally scheduled to run at
  // the second report's report time, should be overridden to run earlier.
  task_environment_.FastForwardBy(kFirstReportingWindow - base::Minutes(10));

  checkpoint.Call(2);

  task_environment_.FastForwardBy(base::Minutes(5));
}

TEST_F(AttributionManagerImplTest,
       QueuedReportFailedWithoutShouldRetry_NotQueuedAgain) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  EXPECT_THAT(StoredReports(), SizeIs(1));

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  // Ensure that observers are notified after the report is deleted.
  EXPECT_CALL(observer, OnSourcesChanged).Times(0);
  EXPECT_CALL(observer, OnReportsChanged);

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .WillOnce(InvokeReportSentCallback(SentResult::kFailure));
  task_environment_.FastForwardBy(kFirstReportingWindow);

  EXPECT_THAT(StoredReports(), IsEmpty());

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversions.ReportSendOutcome3", 1, 1);

  histograms.ExpectTotalCount(
      "Conversions.TimeFromTriggerToReportSentSuccessfully", 0);
}

TEST_F(AttributionManagerImplTest, QueuedReportAlwaysFails_StopsSending) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));
  }

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer,
              OnReportSent(_, /*is_debug_report=*/false,
                           Property(&SendResult::status,
                                    SendResult::Status::kTransientFailure)));

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Milliseconds(1));

  checkpoint.Call(1);

  // The report is sent at its expected report time.
  task_environment_.FastForwardBy(base::Milliseconds(1));

  checkpoint.Call(2);

  // The report is sent at the first retry time of +5 minutes.
  task_environment_.FastForwardBy(base::Minutes(5));

  checkpoint.Call(3);

  // The report is sent at the second retry time of +15 minutes.
  task_environment_.FastForwardBy(base::Minutes(15));

  // At this point, the report has reached the maximum number of attempts and it
  // should no longer be present in the DB.
  EXPECT_THAT(StoredReports(), IsEmpty());

  // kFailed = 1.
  histograms.ExpectUniqueSample("Conversions.ReportSendOutcome3", 1, 1);

  histograms.ExpectTotalCount(
      "Conversions.TimeFromTriggerToReportSentSuccessfully", 0);

  histograms.ExpectUniqueSample(
      "Conversions.EventLevelReport.ReportRetriesTillSuccessOrFailure", 3, 1);
}

TEST_F(AttributionManagerImplTest, ReportExpiredAtStartup_Sent) {
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  ShutdownManager();

  // Fast-forward past source expiry.
  task_environment_.FastForwardBy(kImpressionExpiry + base::Microseconds(1));

  // Simulate startup and ensure the report is sent.
  // Advance by the max offline report delay, per
  // `AttributionResolverDelegate::GetOfflineReportDelayConfig()`.
  CreateManager();

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _));
  task_environment_.FastForwardBy(kDefaultOfflineReportDelay.max);
}

TEST_F(AttributionManagerImplTest, ReportSent_Deleted) {
  base::HistogramTester histograms;
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .WillOnce(InvokeReportSentCallback(SentResult::kSent));

  task_environment_.FastForwardBy(kFirstReportingWindow);

  EXPECT_THAT(StoredReports(), IsEmpty());

  // kSent = 0.
  histograms.ExpectUniqueSample("Conversions.ReportSendOutcome3", 0, 1);
}

TEST_F(AttributionManagerImplTest, QueuedReportSent_ObserversNotified) {
  base::HistogramTester histograms;

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .WillOnce(InvokeReportSentCallback(SentResult::kSent))
      .WillOnce(InvokeReportSentCallback(SentResult::kFailure))
      .WillOnce(InvokeReportSentCallback(SentResult::kSent))
      .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer,
              OnReportSent(
                  EventLevelDataIs(Field(
                      &AttributionReport::EventLevelData::source_event_id, 1u)),
                  /*is_debug_report=*/false, _));
  EXPECT_CALL(observer,
              OnReportSent(
                  EventLevelDataIs(Field(
                      &AttributionReport::EventLevelData::source_event_id, 2u)),
                  /*is_debug_report=*/false, _));
  EXPECT_CALL(observer,
              OnReportSent(
                  EventLevelDataIs(Field(
                      &AttributionReport::EventLevelData::source_event_id, 3u)),
                  /*is_debug_report=*/false, _));

  attribution_manager_->HandleSource(
      SourceBuilder().SetSourceEventId(1).SetExpiry(kImpressionExpiry).Build(),
      kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  task_environment_.FastForwardBy(kFirstReportingWindow);

  // This one should be stored, as it won't be retried.
  attribution_manager_->HandleSource(
      SourceBuilder().SetSourceEventId(2).SetExpiry(kImpressionExpiry).Build(),
      kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  task_environment_.FastForwardBy(kFirstReportingWindow);

  attribution_manager_->HandleSource(
      SourceBuilder().SetSourceEventId(3).SetExpiry(kImpressionExpiry).Build(),
      kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  task_environment_.FastForwardBy(kFirstReportingWindow);

  // This one shouldn't be stored, as it will be retried.
  attribution_manager_->HandleSource(
      SourceBuilder().SetSourceEventId(4).SetExpiry(kImpressionExpiry).Build(),
      kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  task_environment_.FastForwardBy(kFirstReportingWindow);

  // kSent = 0, kFailed = 1.
  EXPECT_THAT(histograms.GetAllSamples("Conversions.ReportSendOutcome3"),
              base::BucketsAre(base::Bucket(0, 2), base::Bucket(1, 1)));
}

TEST_F(AttributionManagerImplTest, TriggerHandled_ObserversNotified) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(observer,
                OnTriggerHandled(
                    _, CreateReportEventLevelStatusIs(
                           AttributionTrigger::EventLevelResult::kSuccess)))
        .Times(3);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(
        observer,
        OnTriggerHandled(_, AllOf(ReplacedEventLevelReportIs(Pointee(
                                      EventLevelDataIs(TriggerPriorityIs(1)))),
                                  CreateReportEventLevelStatusIs(
                                      AttributionTrigger::EventLevelResult::
                                          kSuccessDroppedLowerPriority))));

    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(
        observer,
        OnTriggerHandled(
            _,
            AllOf(ReplacedEventLevelReportIs(IsNull()),
                  CreateReportEventLevelStatusIs(
                      AttributionTrigger::EventLevelResult::kPriorityTooLow))));

    EXPECT_CALL(checkpoint, Call(3));

    EXPECT_CALL(
        observer,
        OnTriggerHandled(_, AllOf(ReplacedEventLevelReportIs(Pointee(
                                      EventLevelDataIs(TriggerPriorityIs(2)))),
                                  CreateReportEventLevelStatusIs(
                                      AttributionTrigger::EventLevelResult::
                                          kSuccessDroppedLowerPriority))));
    EXPECT_CALL(
        observer,
        OnTriggerHandled(_, AllOf(ReplacedEventLevelReportIs(Pointee(
                                      EventLevelDataIs(TriggerPriorityIs(3)))),
                                  CreateReportEventLevelStatusIs(
                                      AttributionTrigger::EventLevelResult::
                                          kSuccessDroppedLowerPriority))));
  }

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .SetMaxEventLevelReports(3)
                                         .Build(),
                                     kFrameId);
  EXPECT_THAT(StoredSources(), SizeIs(1));

  // Store the maximum number of reports.
  for (int i = 1; i <= 3; i++) {
    attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(i).Build(),
                                        kFrameId);
    EXPECT_THAT(StoredReports(), SizeIs(i));
  }

  checkpoint.Call(1);

  {
    // This should replace the report with priority 1.
    attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(4).Build(),
                                        kFrameId);
    EXPECT_THAT(StoredReports(), SizeIs(3));
  }

  checkpoint.Call(2);

  {
    // This should be dropped, as it has a lower priority than all stored
    // reports.
    attribution_manager_->HandleTrigger(
        TriggerBuilder().SetPriority(-5).Build(), kFrameId);
    EXPECT_THAT(StoredReports(), SizeIs(3));
  }

  checkpoint.Call(3);

  {
    // These should replace the reports with priority 2 and 3.
    attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(5).Build(),
                                        kFrameId);
    attribution_manager_->HandleTrigger(TriggerBuilder().SetPriority(6).Build(),
                                        kFrameId);
    EXPECT_THAT(StoredReports(), SizeIs(3));
  }
}

// This functionality is tested more thoroughly in the AttributionResolverImpl
// unit tests. Here, just test to make sure the basic control flow is working.
TEST_F(AttributionManagerImplTest, ClearDataFromBrowserOnly) {
  for (bool match_url : {true, false}) {
    base::Time start = base::Time::Now();
    attribution_manager_->HandleSource(
        SourceBuilder(start).SetExpiry(kImpressionExpiry).Build(), kFrameId);
    attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

    base::RunLoop run_loop;
    attribution_manager_->ClearData(
        start, start + base::Minutes(1),
        base::BindLambdaForTesting(
            [match_url](const blink::StorageKey&) { return match_url; }),
        /*filter_builder=*/nullptr,
        /*delete_rate_limit_data=*/true, run_loop.QuitClosure());
    run_loop.Run();

    size_t expected_reports = match_url ? 0u : 1u;
    EXPECT_THAT(StoredReports(), SizeIs(expected_reports));
  }
}

TEST_F(AttributionManagerImplTest, ClearDataFromBrowserAndOs) {
  base::Time start = base::Time::Now();
  base::Time end = start + base::Minutes(1);
  auto mode = BrowsingDataFilterBuilder::Mode::kDelete;
  auto origin = url::Origin::Create(GURL("https://example.test"));
  std::string domain = "example.test";

  BrowsingDataFilterBuilderImpl filter_builder(mode);
  filter_builder.AddOrigin(origin);
  filter_builder.AddRegisterableDomain(domain);

  EXPECT_CALL(*os_level_manager_,
              ClearData(start, end, std::set<url::Origin>({origin}),
                        std::set<std::string>({domain}), mode,
                        /*delete_rate_limit_data=*/true, _));

  attribution_manager_->HandleSource(
      SourceBuilder(start).SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  base::RunLoop run_loop;
  attribution_manager_->ClearData(
      start, end,
      /*filter=*/base::NullCallback(), &filter_builder,
      /*delete_rate_limit_data=*/true, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_THAT(StoredReports(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, ClearAllDataFromBrowserAndOs) {
  base::Time start = base::Time::Now();
  base::Time end = start + base::Minutes(1);

  EXPECT_CALL(*os_level_manager_,
              ClearData(start, end, std::set<url::Origin>({}),
                        std::set<std::string>({}),
                        BrowsingDataFilterBuilder::Mode::kPreserve,
                        /*delete_rate_limit_data=*/false, _));

  attribution_manager_->HandleSource(
      SourceBuilder(start).SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  base::RunLoop run_loop;
  attribution_manager_->ClearData(start, end,
                                  /*filter=*/base::NullCallback(),
                                  /*filter_builder=*/nullptr,
                                  /*delete_rate_limit_data=*/false,
                                  run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_THAT(StoredReports(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, RemoveDataKeyFromBrowserAndOs) {
  const auto origin = *SuitableOrigin::Deserialize("https://example.test");

  attribution_manager_->HandleSource(
      SourceBuilder().SetReportingOrigin(origin).Build(), kFrameId);
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(origin).Build(), kFrameId);
  EXPECT_THAT(StoredReports(), SizeIs(1));

  AttributionManager::DataKey data_key(*origin);
  EXPECT_CALL(*os_level_manager_,
              ClearData(/*delete_begin=*/base::Time::Min(),
                        /*delete_end=*/base::Time::Max(),
                        /*origins=*/std::set<url::Origin>({*origin}),
                        /*domain*/ std::set<std::string>(),
                        /*mode=*/BrowsingDataFilterBuilder::Mode::kDelete,
                        /*delete_rate_limit_data=*/true, _));

  base::RunLoop run_loop;
  attribution_manager_->RemoveAttributionDataByDataKey(data_key,
                                                       run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_THAT(StoredReports(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, HandleOsRegistration) {
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state(
      AttributionOsLevelManager::ApiState::kEnabled);

  const GURL kRegistrationUrl1("https://r1.test/x");
  const GURL kRegistrationUrl2("https://r2.test/y");
  const GURL kRegistrationUrl3;  // opaque
  const GURL kRegistrationUrl4("https://r4.test/y");

  const auto kRegistrationOrigin1 = url::Origin::Create(kRegistrationUrl1);
  const auto kRegistrationOrigin2 = url::Origin::Create(kRegistrationUrl2);

  const auto kTopLevelOrigin1 = url::Origin::Create(GURL("https://o1.test"));
  const auto kTopLevelOrigin2 = url::Origin::Create(GURL("https://o2.test"));
  const auto kTopLevelOrigin3 = url::Origin::Create(GURL("https://o3.test"));
  const auto kTopLevelOrigin4 = url::Origin::Create(GURL("https://o4.test"));
  const auto kTopLevelOrigin5 = url::Origin::Create(GURL("https://o5.test"));

  cookie_checker_->AddOriginWithDebugCookieSet(
      url::Origin::Create(kRegistrationUrl1));

  const auto return_origin =
      [](const url::Origin& origin) -> ::testing::Matcher<const url::Origin*> {
    return Pointee(origin);
  };
  const auto return_nullptr =
      [](const url::Origin&) -> ::testing::Matcher<const url::Origin*> {
    return IsNull();
  };

  using GetMatcherFunc =
      base::FunctionRef<::testing::Matcher<const url::Origin*>(
          const url::Origin&)>;

  const struct {
    const char* name;
    std::optional<AttributionInputEvent> input_event;
    GetMatcherFunc source_origin;
    GetMatcherFunc destination_origin;
    AttributionReportingOperation register_op;
    AttributionReportingOperation transitional_debug_op;
    AttributionReportingOperation verbose_debug_op;
    const char* metric;
  } kTestCases[] = {
      {
          "source",
          AttributionInputEvent(),
          return_origin,
          return_nullptr,
          AttributionReportingOperation::kOsSource,
          AttributionReportingOperation::kOsSourceTransitionalDebugReporting,
          AttributionReportingOperation::kOsSourceVerboseDebugReport,
          "Conversions.OsRegistrationResult.Source",
      },
      {
          "trigger",
          std::nullopt,
          return_nullptr,
          return_origin,
          AttributionReportingOperation::kOsTrigger,
          AttributionReportingOperation::kOsTriggerTransitionalDebugReporting,
          AttributionReportingOperation::kOsTriggerVerboseDebugReport,
          "Conversions.OsRegistrationResult.Trigger",
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);

    MockAttributionReportingContentBrowserClient browser_client;

    base::HistogramTester histograms;

    {
      InSequence seq;

      const OsRegistration registration1(
          {OsRegistrationItem(kRegistrationUrl1, /*debug_reporting=*/false)},
          kTopLevelOrigin1, test_case.input_event,
          /*is_within_fenced_frame=*/false, kFrameId, kRegistrar);
      EXPECT_CALL(*os_level_manager_,
                  Register(registration1,
                           /*is_debug_key_allowed=*/ElementsAre(true), _))
          .WillOnce(base::test::RunOnceCallback<2>(registration1,
                                                   std::vector<bool>{true}));

      const OsRegistration registration2(
          {OsRegistrationItem(kRegistrationUrl2, /*debug_reporting=*/true)},
          kTopLevelOrigin2, test_case.input_event,
          /*is_within_fenced_frame=*/false, kFrameId, kRegistrar);
      EXPECT_CALL(*os_level_manager_,
                  Register(registration2,
                           /*is_debug_key_allowed=*/ElementsAre(false), _))
          .WillOnce(base::test::RunOnceCallback<2>(registration2,
                                                   std::vector<bool>{false}));

      // Dropped due to the URL being opaque.
      EXPECT_CALL(
          *os_level_manager_,
          Register(OsRegistration(
                       {OsRegistrationItem(kRegistrationUrl3,
                                           /*debug_reporting=*/false)},
                       kTopLevelOrigin3, test_case.input_event,
                       /*is_within_fenced_frame=*/false, kFrameId, kRegistrar),
                   _, _))
          .Times(0);

      // Drops the invalid item but process the two that are valid.
      const OsRegistration registration5(
          {OsRegistrationItem(kRegistrationUrl1, /*debug_reporting=*/false),
           OsRegistrationItem(kRegistrationUrl2, /*debug_reporting=*/false)},
          kTopLevelOrigin5, test_case.input_event,
          /*is_within_fenced_frame=*/false, kFrameId, kRegistrar);
      EXPECT_CALL(
          *os_level_manager_,
          Register(registration5,
                   /*is_debug_key_allowed=*/ElementsAre(true, false), _))
          .WillOnce(base::test::RunOnceCallback<2>(
              registration5, std::vector<bool>{true, true}));

      // Prohibited by policy below.
      EXPECT_CALL(
          *os_level_manager_,
          Register(OsRegistration(
                       {OsRegistrationItem(kRegistrationUrl4,
                                           /*debug_reporting=*/false)},
                       kTopLevelOrigin4, test_case.input_event,
                       /*is_within_fenced_frame=*/false, kFrameId, kRegistrar),
                   _, _))
          .Times(0);

      // Debug key prohibited by policy below.
      EXPECT_CALL(*os_level_manager_,
                  Register(registration1,
                           /*is_debug_key_allowed=*/ElementsAre(false), _))
          .WillOnce(base::test::RunOnceCallback<2>(registration1,
                                                   std::vector<bool>{true}));

      // Bypassing debug cookie.
      EXPECT_CALL(*os_level_manager_,
                  Register(registration2,
                           /*is_debug_key_allowed=*/ElementsAre(true), _))
          .WillOnce(base::test::RunOnceCallback<2>(registration2,
                                                   std::vector<bool>{false}));
    }

    attribution_manager_->HandleOsRegistration(OsRegistration(
        {OsRegistrationItem(kRegistrationUrl1, /*debug_reporting=*/false)},
        kTopLevelOrigin1, test_case.input_event,
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar));
    attribution_manager_->HandleOsRegistration(OsRegistration(
        {OsRegistrationItem(kRegistrationUrl2, /*debug_reporting=*/true)},
        kTopLevelOrigin2, test_case.input_event,
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar));
    attribution_manager_->HandleOsRegistration(OsRegistration(
        {OsRegistrationItem(kRegistrationUrl3, /*debug_reporting=*/false)},
        kTopLevelOrigin3, test_case.input_event,
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar));
    attribution_manager_->HandleOsRegistration(OsRegistration(
        {OsRegistrationItem(kRegistrationUrl3, /*debug_reporting=*/false),
         OsRegistrationItem(kRegistrationUrl1, /*debug_reporting=*/false),
         OsRegistrationItem(kRegistrationUrl2, /*debug_reporting=*/false)},
        kTopLevelOrigin5, test_case.input_event,
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar));

    ExpectOperationAllowed(
        browser_client, test_case.register_op,
        test_case.source_origin(kTopLevelOrigin4),
        test_case.destination_origin(kTopLevelOrigin4),
        /*reporting_origin=*/url::Origin::Create(kRegistrationUrl4),
        /*allowed=*/false);
    ExpectOperationAllowed(browser_client, test_case.register_op,
                           test_case.source_origin(kTopLevelOrigin1),
                           test_case.destination_origin(kTopLevelOrigin1),
                           /*reporting_origin=*/kRegistrationOrigin1,
                           /*allowed=*/true);
    ExpectOperationAllowed(browser_client, test_case.transitional_debug_op,
                           test_case.source_origin(kTopLevelOrigin1),
                           test_case.destination_origin(kTopLevelOrigin1),
                           /*reporting_origin=*/kRegistrationOrigin1,
                           /*allowed=*/false);
    ExpectOperationAllowed(browser_client, test_case.register_op,
                           test_case.source_origin(kTopLevelOrigin2),
                           test_case.destination_origin(kTopLevelOrigin2),
                           /*reporting_origin=*/kRegistrationOrigin2,
                           /*allowed=*/true);
    EXPECT_CALL(browser_client,
                IsAttributionReportingOperationAllowed(
                    _, test_case.transitional_debug_op, _,
                    test_case.source_origin(kTopLevelOrigin2),
                    test_case.destination_origin(kTopLevelOrigin2),
                    Pointee(kRegistrationOrigin2), _))
        .WillOnce([&](BrowserContext*, AttributionReportingOperation,
                      RenderFrameHost*, const url::Origin* source_origin,
                      const url::Origin* destination_origin,
                      const url::Origin* reporting_origin, bool* can_bypass) {
          *can_bypass = true;
          return false;
        });
    ExpectOperationAllowed(browser_client, test_case.verbose_debug_op,
                           test_case.source_origin(kTopLevelOrigin2),
                           test_case.destination_origin(kTopLevelOrigin2),
                           /*reporting_origin=*/kRegistrationOrigin2,
                           /*allowed=*/true);

    ScopedContentBrowserClientSetting setting(&browser_client);

    attribution_manager_->HandleOsRegistration(OsRegistration(
        {OsRegistrationItem(kRegistrationUrl4, /*debug_reporting=*/false)},
        kTopLevelOrigin4, test_case.input_event,
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar));
    attribution_manager_->HandleOsRegistration(OsRegistration(
        {OsRegistrationItem(kRegistrationUrl1, /*debug_reporting=*/false)},
        kTopLevelOrigin1, test_case.input_event,
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar));
    attribution_manager_->HandleOsRegistration(OsRegistration(
        {OsRegistrationItem(kRegistrationUrl2, /*debug_reporting=*/true)},
        kTopLevelOrigin2, test_case.input_event,
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar));

    EXPECT_THAT(
        histograms.GetAllSamples(test_case.metric),
        ElementsAre(
            base::Bucket(OsRegistrationResult::kPassedToOs, 4),
            base::Bucket(OsRegistrationResult::kInvalidRegistrationUrl, 2),
            base::Bucket(OsRegistrationResult::kProhibitedByBrowserPolicy, 1),
            base::Bucket(OsRegistrationResult::kRejectedByOs, 2)));

    ::testing::Mock::VerifyAndClear(os_level_manager_.get());
  }
}

TEST_F(AttributionManagerImplTest, ConversionsSentFromUI_ReportedImmediately) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kSent));
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  std::vector<AttributionReport> reports = StoredReports();
  ASSERT_THAT(reports, SizeIs(1));

  checkpoint.Call(1);

  int calls = 0;

  attribution_manager_->SendReportForWebUI(
      reports.front().id(), base::BindLambdaForTesting([&]() { ++calls; }));

  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(calls, 1);
}

TEST_F(AttributionManagerImplTest, ExpiredReportsAtStartup_Delayed) {
  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .Times(0);

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  ShutdownManager();

  // Fast forward past the expected report time of the first conversion.
  task_environment_.FastForwardBy(kFirstReportingWindow +
                                  base::Milliseconds(1));

  CreateManager();

  // Ensure that the expired report is delayed based on the time the browser
  // started and the min and max offline report delays, per
  // `AttributionResolverDelegate::GetOfflineReportDelayConfig()`.
  base::Time min_new_time = base::Time::Now();
  EXPECT_THAT(StoredReports(),
              ElementsAre(ReportTimeIs(
                  AllOf(Ge(min_new_time + kDefaultOfflineReportDelay.min),
                        Le(min_new_time + kDefaultOfflineReportDelay.max)))));
}

TEST_F(AttributionManagerImplTest,
       NonExpiredReportsQueuedAtStartup_NotDelayed) {
  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .Times(0);

  base::Time start_time = base::Time::Now();

  // Create a report that will be reported at t= 2 days.
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  ShutdownManager();

  // Fast forward just before the expected report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Milliseconds(1));

  CreateManager();

  // Ensure that this report does not receive additional delay.
  EXPECT_THAT(StoredReports(),
              ElementsAre(ReportTimeIs(start_time + kFirstReportingWindow)));
}

TEST_F(AttributionManagerImplTest, SessionOnlyOrigins_DataDeletedAtShutdown) {
  GURL session_only_origin("https://sessiononly.example");
  auto impression =
      SourceBuilder()
          .SetReportingOrigin(*SuitableOrigin::Create(session_only_origin))
          .Build();

  mock_storage_policy_->AddSessionOnly(session_only_origin);

  attribution_manager_->HandleSource(impression, kFrameId);
  attribution_manager_->HandleTrigger(
      TriggerBuilder()
          .SetReportingOrigin(impression.common_info().reporting_origin())
          .Build(),
      kFrameId);

  EXPECT_THAT(StoredSources(), SizeIs(1));
  EXPECT_THAT(StoredReports(), SizeIs(1));

  ShutdownManager();
  CreateManager();

  EXPECT_THAT(StoredSources(), IsEmpty());
  EXPECT_THAT(StoredReports(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, HandleTrigger_RecordsMetric) {
  base::HistogramTester histograms;
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  EXPECT_THAT(StoredReports(), IsEmpty());
  histograms.ExpectUniqueSample(
      "Conversions.CreateReportStatus9",
      AttributionTrigger::EventLevelResult::kNoMatchingImpressions, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.CreateReportStatus4",
      AttributionTrigger::AggregatableResult::kNotRegistered, 1);
}

TEST_F(AttributionManagerImplTest,
       HandleTrigger_RecordsAggregatableFilteringIdMetrics) {
  const struct {
    const char* name;
    AttributionTrigger trigger;
    bool expected_non_default_filtering_id;
    size_t expected_max_bytes_value;
  } kTestCases[] = {
      {
          "default filtering id and max bytes",
          DefaultTrigger(),
          /*expected_non_default_filtering_id=*/false,
          /*expected_max_bytes_value=*/1,
      },
      {
          "non-default filtering id and default max bytes",
          TriggerBuilder()
              .SetAggregatableValues({*AggregatableValues::Create(
                  /*values=*/{{"a", *AggregatableValuesValue::Create(1, 1)},
                              {"b", *AggregatableValuesValue::Create(2, 2)}},
                  attribution_reporting::FilterPair())})
              .Build(),
          /*expected_non_default_filtering_id=*/true,
          /*expected_max_bytes_value=*/1,
      },
      {
          "non-default filtering id and max bytes",
          TriggerBuilder()
              .SetSourceRegistrationTimeConfig(
                  attribution_reporting::mojom::SourceRegistrationTimeConfig::
                      kExclude)
              .SetAggregatableValues({*AggregatableValues::Create(
                  /*values=*/{{"a", *AggregatableValuesValue::Create(2, 2)}},
                  attribution_reporting::FilterPair())})
              .SetAggregatableFilteringIdMaxBytes(
                  *attribution_reporting::AggregatableFilteringIdsMaxBytes::
                      Create(2))
              .Build(),
          /*expected_non_default_filtering_id=*/true,
          /*expected_max_bytes_value=*/2,
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);

    base::HistogramTester histograms;
    attribution_manager_->HandleTrigger(test_case.trigger, kFrameId);

    histograms.ExpectUniqueSample(
        "Conversions.NonDefaultAggregatableFilteringId",
        /*sample=*/test_case.expected_non_default_filtering_id,
        /*expected_bucket_count=*/1);
    histograms.ExpectUniqueSample(
        "Conversions.AggregatableFilteringIdMaxBytesValue",
        /*sample=*/test_case.expected_max_bytes_value,
        /*expected_bucket_count=*/1);
  }
}

TEST_F(AttributionManagerImplTest, HandleSource_RecordsMetric) {
  base::HistogramTester histograms;
  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  task_environment_.RunUntilIdle();
  histograms.ExpectUniqueSample("Conversions.SourceStoredStatus8",
                                StorableSource::Result::kSuccess, 1);
}

TEST_F(AttributionManagerImplTest, OnReportSent_NotifiesObservers) {
  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  EXPECT_THAT(StoredReports(), SizeIs(1));

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  // Ensure that deleting a report notifies observers.
  EXPECT_CALL(observer, OnSourcesChanged).Times(0);
  EXPECT_CALL(observer, OnReportsChanged);

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .WillOnce(InvokeReportSentCallback(SentResult::kSent));
  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(StoredReports(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, HandleSource_NotifiesObservers) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged).Times(0);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged);

    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged).Times(0);
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  EXPECT_THAT(StoredSources(), SizeIs(1));
  checkpoint.Call(1);

  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  EXPECT_THAT(StoredReports(), SizeIs(1));
  checkpoint.Call(2);

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  EXPECT_THAT(StoredSources(), SizeIs(2));
}

TEST_F(AttributionManagerImplTest, HandleTrigger_NotifiesObservers) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged).Times(0);

    EXPECT_CALL(checkpoint, Call(1));

    // Each stored report should notify sources changed one time.
    for (size_t i = 1; i <= 3; i++) {
      EXPECT_CALL(observer, OnSourcesChanged);
      EXPECT_CALL(observer, OnReportsChanged);
    }

    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(observer, OnReportsChanged).Times(6);
    EXPECT_CALL(checkpoint, Call(3));

    EXPECT_CALL(observer, OnSourcesChanged);
    EXPECT_CALL(observer, OnReportsChanged);
  }

  attribution_manager_->HandleSource(TestAggregatableSourceProvider()
                                         .GetBuilder()
                                         .SetMaxEventLevelReports(3)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build(),
                                     kFrameId);
  EXPECT_THAT(StoredSources(), SizeIs(1));
  checkpoint.Call(1);

  // Store the maximum number of reports for the source.
  for (size_t i = 1; i <= 3; i++) {
    attribution_manager_->HandleTrigger(
        DefaultAggregatableTriggerBuilder().Build(), kFrameId);
    // i event-level reports and i aggregatable reports.
    EXPECT_THAT(StoredReports(), SizeIs(i * 2));
  }

  checkpoint.Call(2);

  // Simulate the reports being sent and removed from storage.
  EXPECT_CALL(*aggregation_service_, AssembleReport)
      .Times(3)
      .WillRepeatedly([](AggregatableReportRequest request,
                         AggregationService::AssemblyCallback callback) {
        std::move(callback).Run(std::move(request),
                                CreateExampleAggregatableReport(),
                                AggregationService::AssemblyStatus::kOk);
      });

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .Times(6)
      .WillRepeatedly(InvokeReportSentCallback(SentResult::kSent));

  task_environment_.FastForwardBy(kFirstReportingWindow);

  EXPECT_THAT(StoredReports(), IsEmpty());
  checkpoint.Call(3);

  // The next event-level report should cause the source to reach the
  // event-level attribution limit; the report itself shouldn't be stored as
  // we've already reached the maximum number of event-level reports per source.
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  EXPECT_THAT(StoredReports(), IsEmpty());
}

TEST_F(AttributionManagerImplTest, ClearData_NotifiesObservers) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer, OnSourcesChanged);
  EXPECT_CALL(observer, OnReportsChanged);

  base::RunLoop run_loop;
  attribution_manager_->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating([](const blink::StorageKey&) { return false; }),
      /*filter_builder=*/nullptr,
      /*delete_rate_limit_data=*/true, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(AttributionManagerImplTest,
       EmbedderDisallowsImpressions_SourceNotStored) {
  base::HistogramTester histograms;

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  const auto source = SourceBuilder().SetExpiry(kImpressionExpiry).Build();

  EXPECT_CALL(
      observer,
      OnSourceHandled(source, base::Time::Now(), testing::Eq(std::nullopt),
                      StorableSource::Result::kProhibitedByBrowserPolicy));

  const auto source_origin =
      url::Origin::Create(GURL("https://impression.test/"));
  const auto reporting_origin =
      url::Origin::Create(GURL("https://report.test/"));

  MockAttributionReportingContentBrowserClient browser_client;
  ExpectOperationAllowed(browser_client, AttributionReportingOperation::kSource,
                         Pointee(source_origin),
                         /*destination_origin=*/IsNull(), reporting_origin,
                         /*allowed=*/false);
  ScopedContentBrowserClientSetting setting(&browser_client);

  attribution_manager_->HandleSource(source, kFrameId);
  EXPECT_THAT(StoredSources(), IsEmpty());

  histograms.ExpectUniqueSample(
      "Conversions.SourceStoredStatus8",
      StorableSource::Result::kProhibitedByBrowserPolicy, 1);
}

TEST_F(AttributionManagerImplTest,
       EmbedderDisallowsConversions_ReportNotStored) {
  base::HistogramTester histograms;

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  const auto trigger = DefaultTrigger();

  EXPECT_CALL(
      observer,
      OnTriggerHandled(_, AllOf(Property(&CreateReportResult::trigger, trigger),
                                CreateReportEventLevelStatusIs(
                                    AttributionTrigger::EventLevelResult::
                                        kProhibitedByBrowserPolicy),
                                CreateReportAggregatableStatusIs(
                                    AttributionTrigger::AggregatableResult::
                                        kProhibitedByBrowserPolicy))));

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _,
          AnyOf(
              AttributionReportingOperation::kSource,
              AttributionReportingOperation::kSourceTransitionalDebugReporting),
          _, _, _, _, _))
      .WillRepeatedly(Return(true));

  const auto destination_origin =
      url::Origin::Create(GURL("https://sub.conversion.test/"));
  ExpectOperationAllowed(
      browser_client, AttributionReportingOperation::kTrigger,
      /*source_origin=*/IsNull(), Pointee(destination_origin),
      /*reporting_origin=*/url::Origin::Create(GURL("https://report.test/")),
      /*allowed=*/false);
  ScopedContentBrowserClientSetting setting(&browser_client);

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  EXPECT_THAT(StoredSources(), SizeIs(1));

  attribution_manager_->HandleTrigger(trigger, kFrameId);
  EXPECT_THAT(StoredReports(), IsEmpty());

  histograms.ExpectUniqueSample(
      "Conversions.CreateReportStatus9",
      AttributionTrigger::EventLevelResult::kProhibitedByBrowserPolicy, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.CreateReportStatus4",
      AttributionTrigger::AggregatableResult::kProhibitedByBrowserPolicy, 1);
}

TEST_F(AttributionManagerImplTest, EmbedderDisallowsReporting_ReportNotSent) {
  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _,
          AnyOf(
              AttributionReportingOperation::kSource,
              AttributionReportingOperation::kTrigger,
              AttributionReportingOperation::kSourceTransitionalDebugReporting),
          _, _, _, _, _))
      .WillRepeatedly(Return(true));
  const auto source_origin =
      url::Origin::Create(GURL("https://impression.test/"));
  const auto destination_origin =
      url::Origin::Create(GURL("https://sub.conversion.test/"));
  ExpectOperationAllowed(
      browser_client, AttributionReportingOperation::kReport,
      Pointee(source_origin), Pointee(destination_origin),
      /*reporting_origin=*/url::Origin::Create(GURL("https://report.test/")),
      /*allowed=*/false);
  ScopedContentBrowserClientSetting setting(&browser_client);

  base::HistogramTester histograms;

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .Times(0);

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  EXPECT_THAT(StoredReports(), SizeIs(1));

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer, OnReportSent(_, /*is_debug_report=*/false,
                                     Property(&SendResult::status,
                                              SendResult::Status::kDropped)));

  task_environment_.FastForwardBy(kFirstReportingWindow);

  EXPECT_THAT(StoredReports(), IsEmpty());

  // kDropped = 2.
  histograms.ExpectBucketCount("Conversions.ReportSendOutcome3", 2, 1);
}

TEST_F(AttributionManagerImplTest,
       EmbedderDisallowsReporting_DebugReportNotSent) {
  const auto source_origin = *SuitableOrigin::Deserialize("https://i.test");
  const auto destination_origin =
      *SuitableOrigin::Deserialize("https://d.test");
  const auto reporting_origin = *SuitableOrigin::Deserialize("https://r.test");

  cookie_checker_->AddOriginWithDebugCookieSet(reporting_origin);

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _,
          AnyOf(
              AttributionReportingOperation::kSource,
              AttributionReportingOperation::kTrigger,
              AttributionReportingOperation::kSourceTransitionalDebugReporting,
              AttributionReportingOperation::
                  kTriggerTransitionalDebugReporting),
          _, _, _, _, _))
      .WillRepeatedly(Return(true));
  ExpectOperationAllowed(browser_client, AttributionReportingOperation::kReport,
                         Pointee(source_origin), Pointee(destination_origin),
                         *reporting_origin,
                         /*allowed=*/false);
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/true, _))
      .Times(0);

  attribution_manager_->HandleSource(
      SourceBuilder()
          .SetSourceOrigin(source_origin)
          .SetDestinationSites({net::SchemefulSite(destination_origin)})
          .SetReportingOrigin(reporting_origin)
          .SetDebugKey(123)
          .SetExpiry(kImpressionExpiry)
          .Build(),
      kFrameId);

  attribution_manager_->HandleTrigger(
      TriggerBuilder()
          .SetDestinationOrigin(destination_origin)
          .SetReportingOrigin(reporting_origin)
          .SetDebugKey(456)
          .Build(),
      kFrameId);

  EXPECT_THAT(StoredReports(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest, Offline_NoReportSent) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _));
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  EXPECT_THAT(StoredReports(), SizeIs(1));

  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_NONE);
  task_environment_.FastForwardBy(kFirstReportingWindow);

  checkpoint.Call(1);

  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
}

class AttributionManagerImplOnlineConnectionTypeTest
    : public AttributionManagerImplTest {
 protected:
  void ConfigureStorageDelegate(
      ConfigurableStorageDelegate& delegate) const override {
    delegate.set_offline_report_delay_config(
        AttributionResolverDelegate::OfflineReportDelayConfig{
            .min = base::Minutes(1),
            .max = base::Minutes(1),
        });
  }
};

TEST_F(AttributionManagerImplOnlineConnectionTypeTest,
       OnlineConnectionTypeChanges_ReportTimesNotAdjusted) {
  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);
  EXPECT_THAT(StoredReports(), SizeIs(1));

  // Deliberately avoid running tasks so that the connection change and time
  // advance can be "atomic", which is necessary because
  // `AttributionResolver::AdjustOfflineReportTimes()` only adjusts times for
  // reports that should have been sent before now. In other words, the call to
  // `AdjustOfflineReportTimes()` would have no effect if we used
  // `FastForwardBy()` here, and we wouldn't be able to detect it below.
  task_environment_.AdvanceClock(kFirstReportingWindow + base::Microseconds(1));

  // This will fail with 0 calls if the report time was adjusted to +1 minute.
  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _));

  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_4G);

  // Cause any scheduled tasks to run.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionManagerImplTest, TimeFromConversionToReportSendHistogram) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  ReportSentCallback report_sent_callback;
  std::optional<AttributionReport> sent_report;

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .WillOnce([&](AttributionReport report, bool is_debug_report,
                    ReportSentCallback callback) {
        report_sent_callback = std::move(callback);
        sent_report = std::move(report);
      });

  task_environment_.FastForwardBy(kFirstReportingWindow);

  histograms.ExpectUniqueSample("Conversions.TimeFromConversionToReportSend",
                                kFirstReportingWindow.InHours(), 1);

  task_environment_.FastForwardBy(base::Hours(1));

  ASSERT_TRUE(report_sent_callback);
  ASSERT_TRUE(sent_report);
  std::move(report_sent_callback)
      .Run(*std::move(sent_report), SendResult::Sent(SentResult::kSent,
                                                     /*status=*/0));

  histograms.ExpectUniqueSample(
      "Conversions.TimeFromTriggerToReportSentSuccessfully",
      kFirstReportingWindow.InHours() + 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.EventLevelReport.ReportRetriesTillSuccessOrFailure", 0, 1);
}

TEST_F(AttributionManagerImplTest, ReportRetriesTillSuccessHistogram) {
  base::HistogramTester histograms;

  ReportSentCallback report_sent_callback;
  std::optional<AttributionReport> sent_report;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce([&](AttributionReport report, bool is_debug_report,
                      ReportSentCallback callback) {
          report_sent_callback = std::move(callback);
          sent_report = std::move(report);
        });
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  task_environment_.FastForwardBy(kFirstReportingWindow);

  checkpoint.Call(1);

  // First report delay.
  task_environment_.FastForwardBy(base::Minutes(5));

  ASSERT_TRUE(report_sent_callback);
  ASSERT_TRUE(sent_report);
  std::move(report_sent_callback)
      .Run(*std::move(sent_report), SendResult::Sent(SentResult::kSent,
                                                     /*status=*/0));

  // kSuccess = 0.
  histograms.ExpectUniqueSample("Conversions.ReportSendOutcome3", 0, 1);

  histograms.ExpectUniqueSample(
      "Conversions.EventLevelReport.ReportRetriesTillSuccessOrFailure", 1, 1);
}

class AttributionManagerImplTestThirdRetryFeature
    : public AttributionManagerImplTest {
 public:
  AttributionManagerImplTestThirdRetryFeature() {
    // This UMA records a sample every 30s via a periodic task which
    // interacts poorly with TaskEnvironment::FastForward using day long
    // delays (we need to run the uma update every 30s for that
    // interval)
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kAttributionReportDeliveryThirdRetryAttempt},
        /*disabled_features=*/{network::features::kGetCookiesStringUma});
  }
};

TEST_F(AttributionManagerImplTestThirdRetryFeature,
       ReportRetryThirdRetryFeature) {
  base::HistogramTester histograms;

  bool was_report_sent = false;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));

    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));

    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce(InvokeReportSentCallback(SentResult::kTransientFailure));

    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .WillOnce([&](AttributionReport report, bool is_debug_report,
                      ReportSentCallback callback) {
          std::move(callback).Run(std::move(report),
                                  SendResult::Sent(SentResult::kSent,
                                                   /*status=*/0));
          was_report_sent = true;
        });
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  task_environment_.FastForwardBy(kFirstReportingWindow);

  checkpoint.Call(1);

  // First report delay.
  task_environment_.FastForwardBy(base::Minutes(5));

  checkpoint.Call(2);

  // Second report delay.
  task_environment_.FastForwardBy(base::Minutes(15));

  checkpoint.Call(3);

  // Third report delay.
  task_environment_.FastForwardBy(base::Days(1));

  ASSERT_TRUE(was_report_sent);

  // kSuccess = 0.
  histograms.ExpectUniqueSample("Conversions.ReportSendOutcome3", 0, 1);

  histograms.ExpectUniqueSample(
      "Conversions.EventLevelReport.ReportRetriesTillSuccessOrFailure_"
      "ThirdRetryEnabled",
      3, 1);
}

TEST_F(AttributionManagerImplTest, SendReport_RecordsExtraReportDelay2) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _));
  }

  attribution_manager_->HandleSource(TestAggregatableSourceProvider()
                                         .GetBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .Build(),
                                     kFrameId);
  attribution_manager_->HandleTrigger(
      DefaultAggregatableTriggerBuilder().Build(), kFrameId);

  // Prevent the report from being sent until after its original report time.
  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_NONE);
  task_environment_.FastForwardBy(kFirstReportingWindow + base::Days(3));

  checkpoint.Call(1);

  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);

  task_environment_.FastForwardBy(kDefaultOfflineReportDelay.max);

  histograms.ExpectUniqueTimeSample(
      "Conversions.ExtraReportDelay2",
      base::Days(3) + kDefaultOfflineReportDelay.min, 1);
  histograms.ExpectUniqueTimeSample(
      "Conversions.AggregatableReport.ExtraReportDelay",
      base::Days(3) + kDefaultOfflineReportDelay.min, 1);
  histograms.ExpectUniqueTimeSample(
      "Conversions.AggregatableReport.NoContextID.ExtraReportDelay",
      base::Days(3) + kDefaultOfflineReportDelay.min, 1);
}

TEST_F(AttributionManagerImplTest, SendReport_RecordsSchedulerReportDelay) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(TestAggregatableSourceProvider()
                                         .GetBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .Build(),
                                     kFrameId);
  attribution_manager_->HandleTrigger(
      DefaultAggregatableTriggerBuilder().Build(), kFrameId);

  EXPECT_THAT(StoredReports(), SizeIs(2));

  // Deliberately avoid running tasks so that the scheduler is delayed.
  task_environment_.AdvanceClock(kFirstReportingWindow + base::Seconds(1));

  // Cause any scheduled tasks to run.
  task_environment_.FastForwardBy(base::TimeDelta());

  histograms.ExpectUniqueTimeSample("Conversions.SchedulerReportDelay",
                                    base::Seconds(1), 1);
  histograms.ExpectUniqueTimeSample(
      "Conversions.AggregatableReport.SchedulerReportDelay", base::Seconds(1),
      1);
}

TEST_F(AttributionManagerImplTest, SendReportsFromWebUI_DoesNotRecordMetrics) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _));

  attribution_manager_->SendReportForWebUI(AttributionReport::Id(1),
                                           base::DoNothing());
  task_environment_.FastForwardBy(base::TimeDelta());

  histograms.ExpectTotalCount("Conversions.ExtraReportDelay2", 0);
  histograms.ExpectTotalCount("Conversions.TimeFromConversionToReportSend", 0);
}

class AttributionManagerImplFakeReportTest : public AttributionManagerImplTest {
 protected:
  void ConfigureStorageDelegate(
      ConfigurableStorageDelegate& delegate) const override {
    delegate.set_randomized_response(
        std::vector<attribution_reporting::FakeEventLevelReport>{
            {.trigger_data = 0, .window_index = 0},
        });
  }
};

// Regression test for https://crbug.com/1294519.
TEST_F(AttributionManagerImplFakeReportTest,
       FakeReport_UpdatesSendReportTimer) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _));
  }

  attribution_manager_->HandleSource(
      SourceBuilder().SetExpiry(kImpressionExpiry).Build(), kFrameId);

  checkpoint.Call(1);
  task_environment_.FastForwardBy(kImpressionExpiry);
}

// Regression test for https://crbug.com/1506245.
TEST_F(AttributionManagerImplFakeReportTest, FakeReport_NotifiesObservers) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(observer, OnReportsChanged).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(observer, OnReportsChanged);
  }

  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::TimeDelta());
}

class AttributionManagerImplNoFakeReportTest
    : public AttributionManagerImplTest {
 protected:
  void ConfigureStorageDelegate(
      ConfigurableStorageDelegate& delegate) const override {
    delegate.set_randomized_response({});
  }
};

TEST_F(AttributionManagerImplNoFakeReportTest, NotNotifyObservers) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer, OnReportsChanged).Times(0);

  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  task_environment_.FastForwardBy(base::TimeDelta());
}

// Test that multiple source and trigger registrations, with and without debug
// keys present, are handled in the order they are received by the manager.
TEST_F(AttributionManagerImplTest, RegistrationsHandledInOrder) {
  cookie_checker_->DeferCallbacks();

  const auto r1 = *SuitableOrigin::Deserialize("https://r1.test");
  const auto r2 = *SuitableOrigin::Deserialize("https://r2.test");

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetSourceEventId(1)
                                         .SetDebugKey(11)
                                         .SetReportingOrigin(r1)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build(),
                                     kFrameId);

  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetTriggerData(2).SetReportingOrigin(r1).Build(),
      kFrameId);

  attribution_manager_->HandleTrigger(TriggerBuilder()
                                          .SetTriggerData(3)
                                          .SetDebugKey(13)
                                          .SetReportingOrigin(r2)
                                          .Build(),
                                      kFrameId);

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetSourceEventId(4)
                                         .SetDebugKey(14)
                                         .SetReportingOrigin(r2)
                                         .SetExpiry(kImpressionExpiry)
                                         .Build(),
                                     kFrameId);

  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetTriggerData(5).SetReportingOrigin(r2).Build(),
      kFrameId);

  ASSERT_THAT(StoredSources(), IsEmpty());
  ASSERT_THAT(StoredReports(), IsEmpty());

  // This should cause the first 2 events to be processed.
  cookie_checker_->RunNextDeferredCallback(/*is_debug_cookie_set=*/false);
  ASSERT_THAT(StoredSources(), ElementsAre(SourceEventIdIs(1)));
  ASSERT_THAT(StoredReports(), ElementsAre(EventLevelDataIs(TriggerDataIs(2))));

  // This should cause the next event to be processed. There's no matching
  // source, so the trigger should be dropped.
  cookie_checker_->RunNextDeferredCallback(/*is_debug_cookie_set=*/false);
  ASSERT_THAT(StoredSources(), ElementsAre(SourceEventIdIs(1)));
  ASSERT_THAT(StoredReports(), ElementsAre(EventLevelDataIs(TriggerDataIs(2))));

  // This should cause the next 2 events to be processed.
  cookie_checker_->RunNextDeferredCallback(/*is_debug_cookie_set=*/false);
  ASSERT_THAT(StoredSources(),
              UnorderedElementsAre(SourceEventIdIs(1), SourceEventIdIs(4)));
  ASSERT_THAT(StoredReports(),
              UnorderedElementsAre(EventLevelDataIs(TriggerDataIs(2)),
                                   EventLevelDataIs(TriggerDataIs(5))));
}

namespace {

const struct {
  const char* name;
  std::optional<uint64_t> input_debug_key;
  const char* reporting_origin;
  std::optional<uint64_t> expected_debug_key;
  std::optional<uint64_t> expected_cleared_key;
  bool cookie_access_allowed;
  bool expected_debug_cookie_set;
  bool can_bypass = false;
} kDebugKeyTestCases[] = {
    {
        "no debug key, no cookie",
        std::nullopt,
        "https://r2.test",
        std::nullopt,
        std::nullopt,
        true,
        false,
    },
    {
        "has debug key, no cookie",
        123,
        "https://r2.test",
        std::nullopt,
        123,
        true,
        false,
    },
    {
        "no debug key, has cookie",
        std::nullopt,
        "https://r1.test",
        std::nullopt,
        std::nullopt,
        true,
        true,
    },
    {
        "has debug key, has cookie",
        123,
        "https://r1.test",
        123,
        std::nullopt,
        true,
        true,
    },
    {
        "has debug key, no cookie access",
        123,
        "https://r1.test",
        std::nullopt,
        123,
        false,
        false,
    },
    {
        "has debug key, no cookie access, can bypass",
        123,
        "https://r1.test",
        123,
        std::nullopt,
        false,
        true,
        true,
    },
};

}  // namespace

TEST_F(AttributionManagerImplTest, HandleSource_DebugKey) {
  cookie_checker_->AddOriginWithDebugCookieSet(
      url::Origin::Create(GURL("https://r1.test")));

  for (const auto& test_case : kDebugKeyTestCases) {
    SCOPED_TRACE(test_case.name);

    MockAttributionObserver observer;
    base::ScopedObservation<AttributionManager, AttributionObserver>
        observation(&observer);
    observation.Observe(attribution_manager_.get());

    const auto reporting_origin =
        *SuitableOrigin::Deserialize(test_case.reporting_origin);
    MockAttributionReportingContentBrowserClient browser_client;
    EXPECT_CALL(
        browser_client,
        IsAttributionReportingOperationAllowed(
            _, ContentBrowserClient::AttributionReportingOperation::kSource, _,
            _, IsNull(), Pointee(*reporting_origin), _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(browser_client,
                IsAttributionReportingOperationAllowed(
                    _,
                    ContentBrowserClient::AttributionReportingOperation::
                        kSourceTransitionalDebugReporting,
                    _, _, IsNull(), Pointee(*reporting_origin), _))
        .WillOnce(
            [&](BrowserContext* browser_context,
                ContentBrowserClient::AttributionReportingOperation operation,
                RenderFrameHost* rfh, const url::Origin* source_origin,
                const url::Origin* destination_origin,
                const url::Origin* reporting_origin, bool* can_bypass) {
              *can_bypass = test_case.can_bypass;
              return test_case.cookie_access_allowed;
            });
    ScopedContentBrowserClientSetting setting(&browser_client);

    EXPECT_CALL(observer, OnSourceHandled(_, base::Time::Now(),
                                          test_case.expected_cleared_key, _));
    attribution_manager_->HandleSource(
        SourceBuilder()
            .SetReportingOrigin(reporting_origin)
            .SetDebugKey(test_case.input_debug_key)
            .SetExpiry(kImpressionExpiry)
            .Build(),
        kFrameId);

    EXPECT_THAT(
        StoredSources(),
        ElementsAre(AllOf(
            SourceDebugKeyIs(test_case.expected_debug_key),
            SourceDebugCookieSetIs(test_case.expected_debug_cookie_set))));

    attribution_manager_->ClearData(base::Time::Min(), base::Time::Max(),
                                    /*filter=*/base::NullCallback(),
                                    /*filter_builder=*/nullptr,
                                    /*delete_rate_limit_data=*/true,
                                    base::DoNothing());
  }
}

TEST_F(AttributionManagerImplTest, HandleTrigger_DebugKey) {
  cookie_checker_->AddOriginWithDebugCookieSet(
      url::Origin::Create(GURL("https://r1.test")));

  for (const auto& test_case : kDebugKeyTestCases) {
    SCOPED_TRACE(test_case.name);

    MockAttributionObserver observer;
    base::ScopedObservation<AttributionManager, AttributionObserver>
        observation(&observer);
    observation.Observe(attribution_manager_.get());

    const auto reporting_origin =
        *SuitableOrigin::Deserialize(test_case.reporting_origin);

    MockAttributionReportingContentBrowserClient browser_client;
    EXPECT_CALL(
        browser_client,
        IsAttributionReportingOperationAllowed(
            _,
            AnyOf(ContentBrowserClient::AttributionReportingOperation::kSource,
                  ContentBrowserClient::AttributionReportingOperation::kTrigger,
                  ContentBrowserClient::AttributionReportingOperation::
                      kSourceTransitionalDebugReporting),
            _, _, _, Pointee(*reporting_origin), _))
        .WillRepeatedly(Return(true));
    if (test_case.input_debug_key) {
      EXPECT_CALL(browser_client,
                  IsAttributionReportingOperationAllowed(
                      _,
                      ContentBrowserClient::AttributionReportingOperation::
                          kTriggerTransitionalDebugReporting,
                      _, IsNull(), _, Pointee(*reporting_origin), _))
          .WillOnce(
              [&](BrowserContext* browser_context,
                  ContentBrowserClient::AttributionReportingOperation operation,
                  RenderFrameHost* rfh, const url::Origin* source_origin,
                  const url::Origin* destination_origin,
                  const url::Origin* reporting_origin, bool* can_bypass) {
                *can_bypass = test_case.can_bypass;
                return test_case.cookie_access_allowed;
              });
    }
    ScopedContentBrowserClientSetting setting(&browser_client);

    attribution_manager_->HandleSource(SourceBuilder()
                                           .SetReportingOrigin(reporting_origin)
                                           .SetExpiry(kImpressionExpiry)
                                           .Build(),
                                       kFrameId);

    EXPECT_THAT(StoredSources(), SizeIs(1));
    EXPECT_CALL(observer, OnTriggerHandled(test_case.expected_cleared_key, _));
    attribution_manager_->HandleTrigger(
        TriggerBuilder()
            .SetReportingOrigin(reporting_origin)
            .SetDebugKey(test_case.input_debug_key)
            .Build(),
        kFrameId);
    EXPECT_THAT(
        StoredReports(),
        ElementsAre(AllOf(ReportSourceDebugKeyIs(std::nullopt),
                          TriggerDebugKeyIs(test_case.expected_debug_key))));

    attribution_manager_->ClearData(base::Time::Min(), base::Time::Max(),
                                    /*filter=*/base::NullCallback(),
                                    /*filter_builder=*/nullptr,
                                    /*delete_rate_limit_data=*/true,
                                    base::DoNothing());
  }
}

TEST_F(AttributionManagerImplTest, DebugReport_SentImmediately) {
  const auto reporting_origin = *SuitableOrigin::Deserialize("https://r1.test");

  cookie_checker_->AddOriginWithDebugCookieSet(reporting_origin);

  const struct {
    const char* name;
    std::optional<uint64_t> source_debug_key;
    std::optional<uint64_t> trigger_debug_key;
    bool send_expected;
  } kTestCases[] = {
      {"neither", std::nullopt, std::nullopt, false},
      {"source", 1, std::nullopt, false},
      {"trigger", std::nullopt, 1, false},
      {"both", 1, 2, true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);

    MockAttributionObserver observer;
    base::ScopedObservation<AttributionManager, AttributionObserver>
        observation(&observer);
    observation.Observe(attribution_manager_.get());
    EXPECT_CALL(observer, OnReportSent(_, /*is_debug_report=*/true, _))
        .Times(test_case.send_expected * 2);

    attribution_manager_->HandleSource(
        TestAggregatableSourceProvider()
            .GetBuilder()
            .SetReportingOrigin(reporting_origin)
            .SetExpiry(kImpressionExpiry)
            .SetDebugKey(test_case.source_debug_key)
            .Build(),
        kFrameId);

    EXPECT_THAT(StoredSources(), SizeIs(1));

    if (test_case.send_expected) {
      EXPECT_CALL(*aggregation_service_, AssembleReport)
          .WillOnce([](AggregatableReportRequest request,
                       AggregationService::AssemblyCallback callback) {
            std::move(callback).Run(std::move(request),
                                    CreateExampleAggregatableReport(),
                                    AggregationService::AssemblyStatus::kOk);
          });
    } else {
      EXPECT_CALL(*aggregation_service_, AssembleReport).Times(0);
    }

    EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
        .Times(0);

    if (test_case.send_expected) {
      EXPECT_CALL(
          *report_sender_,
          SendReport(AllOf(ReportSourceDebugKeyIs(test_case.source_debug_key),
                           TriggerDebugKeyIs(test_case.trigger_debug_key)),
                     true, _))
          .Times(2)
          .WillRepeatedly(
              InvokeReportSentCallback(SentResult::kTransientFailure));
    } else {
      EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/true, _))
          .Times(0);
    }

    attribution_manager_->HandleTrigger(
        DefaultAggregatableTriggerBuilder()
            .SetReportingOrigin(reporting_origin)
            .SetDebugKey(test_case.trigger_debug_key)
            .Build(),
        kFrameId);
    // one event-level-report, one aggregatable report.
    EXPECT_THAT(StoredReports(), SizeIs(2));

    attribution_manager_->ClearData(base::Time::Min(), base::Time::Max(),
                                    /*filter=*/base::NullCallback(),
                                    /*filter_builder=*/nullptr,
                                    /*delete_rate_limit_data=*/true,
                                    base::DoNothing());

    ::testing::Mock::VerifyAndClear(&observer);
  }
}

TEST_F(AttributionManagerImplTest,
       HandleSource_NotifiesObservers_SourceHandled) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  const StorableSource source = SourceBuilder().Build();

  EXPECT_CALL(observer, OnSourceHandled(source, base::Time::Now(),
                                        testing::Eq(std::nullopt),
                                        StorableSource::Result::kSuccess));

  attribution_manager_->HandleSource(source, kFrameId);
  EXPECT_THAT(StoredSources(), SizeIs(1));
}

TEST_F(AttributionManagerImplTest,
       AggregateReportAssemblySucceeded_ReportSent) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      TestAggregatableSourceProvider().GetBuilder().Build(), kFrameId);
  attribution_manager_->HandleTrigger(
      DefaultAggregatableTriggerBuilder().Build(), kFrameId);

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(
      observer,
      OnReportSent(ReportTypeIs(AttributionReport::Type::kEventLevel),
                   /*is_debug_report=*/false,
                   Property(&SendResult::status, SendResult::Status::kSent)));

  EXPECT_CALL(
      observer,
      OnReportSent(
          ReportTypeIs(AttributionReport::Type::kAggregatableAttribution),
          /*is_debug_report=*/false,
          Property(&SendResult::status, SendResult::Status::kSent)));

  Checkpoint checkpoint;
  {
    InSequence seq;
    EXPECT_CALL(*aggregation_service_, AssembleReport).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*aggregation_service_, AssembleReport)
        .WillOnce([](AggregatableReportRequest request,
                     AggregationService::AssemblyCallback callback) {
          std::move(callback).Run(std::move(request),
                                  CreateExampleAggregatableReport(),
                                  AggregationService::AssemblyStatus::kOk);
        });
  }

  // Make sure the report is not sent earlier than its report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Microseconds(1));

  checkpoint.Call(1);

  std::vector<ReportSentCallback> report_sent_callbacks;
  std::vector<AttributionReport> sent_reports;

  // One event-level report, one aggregatable report.
  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .WillRepeatedly([&](AttributionReport report, bool is_debug_report,
                          ReportSentCallback callback) {
        report_sent_callbacks.push_back(std::move(callback));
        sent_reports.push_back(std::move(report));
      });
  task_environment_.FastForwardBy(base::Microseconds(1));

  ASSERT_THAT(report_sent_callbacks, SizeIs(2));
  ASSERT_THAT(sent_reports, SizeIs(2));

  task_environment_.FastForwardBy(base::Minutes(1));

  std::move(report_sent_callbacks[0])
      .Run(std::move(sent_reports[0]), SendResult::Sent(SentResult::kSent,
                                                        /*status=*/0));
  std::move(report_sent_callbacks[1])
      .Run(std::move(sent_reports[1]), SendResult::Sent(SentResult::kSent,
                                                        /*status=*/0));

  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.AssembleReportStatus",
      AssembleAggregatableReportStatus::kSuccess, 1);
  histograms.ExpectUniqueTimeSample(
      "Conversions.AggregatableReport.TimeFromTriggerToReportAssembly2",
      kFirstReportingWindow, 1);
  histograms.ExpectUniqueTimeSample(
      "Conversions.AggregatableReport.TimeFromTriggerToReportSentSuccessfully",
      kFirstReportingWindow + base::Minutes(1), 1);
  // kSent = 0.
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.ReportSendOutcome2", 0, 1);
}

TEST_F(AttributionManagerImplTest, OnReportSent_RecordReportDelay) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(TestAggregatableSourceProvider()
                                         .GetBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .Build(),
                                     kFrameId);
  attribution_manager_->HandleTrigger(
      DefaultAggregatableTriggerBuilder().Build(), kFrameId);

  Checkpoint checkpoint;
  {
    InSequence seq;
    EXPECT_CALL(*aggregation_service_, AssembleReport).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*aggregation_service_, AssembleReport)
        .WillOnce([](AggregatableReportRequest request,
                     AggregationService::AssemblyCallback callback) {
          std::move(callback).Run(std::move(request),
                                  CreateExampleAggregatableReport(),
                                  AggregationService::AssemblyStatus::kOk);
        });
  }

  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_NONE);

  task_environment_.FastForwardBy(kFirstReportingWindow + base::Days(3));

  checkpoint.Call(1);

  std::vector<ReportSentCallback> report_sent_callbacks;
  std::vector<AttributionReport> sent_reports;

  // One event-level report, one aggregatable report.
  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _))
      .WillRepeatedly([&](AttributionReport report, bool is_debug_report,
                          ReportSentCallback callback) {
        report_sent_callbacks.push_back(std::move(callback));
        sent_reports.push_back(std::move(report));
      });

  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  task_environment_.FastForwardBy(base::Minutes(1));

  ASSERT_THAT(report_sent_callbacks, SizeIs(2));
  ASSERT_THAT(sent_reports, SizeIs(2));

  std::move(report_sent_callbacks[0])
      .Run(std::move(sent_reports[0]), SendResult::Sent(SentResult::kSent,
                                                        /*status=*/0));
  std::move(report_sent_callbacks[1])
      .Run(std::move(sent_reports[1]), SendResult::Sent(SentResult::kSent,
                                                        /*status=*/0));

  histograms.ExpectUniqueTimeSample(
      "Conversions.ExtraReportDelayForSuccessfulSend",
      base::Days(3) + base::Minutes(1), 1);
  histograms.ExpectUniqueTimeSample(
      "Conversions.AggregatableReport.ExtraReportDelayForSuccessfulSend",
      base::Days(3) + base::Minutes(1), 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.ReportRetriesTillSuccessOrFailure", 0, 1);
}

TEST_F(AttributionManagerImplTest,
       AggregateReportAssemblyFailed_RetriedAndReportNotSent) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(
      TestAggregatableSourceProvider().GetBuilder().Build(), kFrameId);
  attribution_manager_->HandleTrigger(
      DefaultAggregatableTriggerBuilder().Build(
          /*generate_event_trigger_data=*/false),
      kFrameId);

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(
      observer,
      OnReportSent(
          ReportTypeIs(AttributionReport::Type::kAggregatableAttribution),
          /*is_debug_report=*/false,
          Property(&SendResult::status,
                   SendResult::Status::kTransientAssemblyFailure)));

  Checkpoint checkpoint;
  {
    InSequence seq;
    EXPECT_CALL(*aggregation_service_, AssembleReport).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*aggregation_service_, AssembleReport)
        .WillOnce([](AggregatableReportRequest request,
                     AggregationService::AssemblyCallback callback) {
          std::move(callback).Run(
              std::move(request), std::nullopt,
              AggregationService::AssemblyStatus::kAssemblyFailed);
        });
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*aggregation_service_, AssembleReport)
        .WillOnce([](AggregatableReportRequest request,
                     AggregationService::AssemblyCallback callback) {
          std::move(callback).Run(
              std::move(request), std::nullopt,
              AggregationService::AssemblyStatus::kAssemblyFailed);
        });
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*aggregation_service_, AssembleReport)
        .WillOnce([](AggregatableReportRequest request,
                     AggregationService::AssemblyCallback callback) {
          std::move(callback).Run(
              std::move(request), std::nullopt,
              AggregationService::AssemblyStatus::kAssemblyFailed);
        });
  }

  // Make sure the report is not sent earlier than its report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Microseconds(1));

  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Microseconds(1));

  checkpoint.Call(2);

  // First report delay.
  task_environment_.FastForwardBy(base::Minutes(5));

  checkpoint.Call(3);

  // Second report delay.
  task_environment_.FastForwardBy(base::Minutes(15));

  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.AssembleReportStatus",
      AssembleAggregatableReportStatus::kAssembleReportFailed, 3);
  histograms.ExpectUniqueTimeSample(
      "Conversions.AggregatableReport.TimeFromTriggerToReportAssembly2",
      kFirstReportingWindow, 3);
  histograms.ExpectTotalCount(
      "Conversions.AggregatableReport.TimeFromTriggerToReportSentSuccessfully",
      0);
  // kFailedToAssemble = 3.
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.ReportSendOutcome2", 3, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.ReportRetriesTillSuccessOrFailure", 3, 1);
}

TEST_F(AttributionManagerImplTest, AggregationServiceDisabled_ReportNotSent) {
  base::HistogramTester histograms;

  ShutdownAggregationService();

  attribution_manager_->HandleSource(
      TestAggregatableSourceProvider().GetBuilder().Build(), kFrameId);
  attribution_manager_->HandleTrigger(
      DefaultAggregatableTriggerBuilder().Build(), kFrameId);

  // Event-level report was sent.
  EXPECT_CALL(*report_sender_, SendReport(_, /*is_debug_report=*/false, _));

  task_environment_.FastForwardBy(kFirstReportingWindow);

  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.AssembleReportStatus",
      AssembleAggregatableReportStatus::kAggregationServiceUnavailable, 1);
  histograms.ExpectUniqueTimeSample(
      "Conversions.AggregatableReport.TimeFromTriggerToReportAssembly2",
      kFirstReportingWindow, 1);
  histograms.ExpectTotalCount(
      "Conversions.AggregatableReport.TimeFromTriggerToReportSentSuccessfully",
      0);
  // kFailedToAssemble = 3.
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.ReportSendOutcome2", 3, 1);
}

TEST_F(AttributionManagerImplTest, GetFailedReportDelay) {
  const struct {
    int failed_send_attempts;
    std::optional<base::TimeDelta> expected;
    bool third_retry_enabled = false;
  } kTestCases[] = {
      {1, base::Minutes(5)},    {2, base::Minutes(15)},  {3, std::nullopt},
      {3, base::Days(1), true}, {4, std::nullopt, true},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected,
              GetFailedReportDelay(test_case.failed_send_attempts,
                                   test_case.third_retry_enabled))
        << "failed_send_attempts=" << test_case.failed_send_attempts
        << ", third_retry_enabled=" << test_case.third_retry_enabled;
  }
}

TEST_F(AttributionManagerImplTest, TooManyEventsInQueue) {
  base::HistogramTester histograms;

  // Prevent sources from being removed from the queue.
  cookie_checker_->DeferCallbacks();

  for (size_t i = 0; i <= kMaxPendingEvents; i++) {
    attribution_manager_->HandleSource(
        SourceBuilder().SetDebugKey(i).SetExpiry(kImpressionExpiry).Build(),
        kFrameId);
  }

  histograms.ExpectBucketCount("Conversions.EnqueueEventAllowed", true,
                               kMaxPendingEvents);
  histograms.ExpectBucketCount("Conversions.EnqueueEventAllowed", false, 1);

  // Unblock the cookie checks. Only the first `kMaxPendingEvents` sources
  // should be stored.
  for (size_t i = 0; i <= kMaxPendingEvents; i++) {
    cookie_checker_->RunNextDeferredCallback(/*is_debug_cookie_set=*/true);
    task_environment_.RunUntilIdle();
  }

  std::vector<StoredSource> sources = StoredSources();
  ASSERT_THAT(sources, SizeIs(kMaxPendingEvents));
  for (size_t i = 0; i < kMaxPendingEvents; i++) {
    EXPECT_THAT(sources[i], SourceDebugKeyIs(i));
  }
}

TEST_F(AttributionManagerImplTest, TriggerVerboseDebugReport_ReportSent) {
  base::HistogramTester histograms;

  const auto reporting_origin = *SuitableOrigin::Deserialize("https://r1.test");
  cookie_checker_->AddOriginWithDebugCookieSet(reporting_origin);

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, _)).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, _));
  }

  // Failed without debug reporting.
  attribution_manager_->HandleTrigger(
      TriggerBuilder().SetReportingOrigin(reporting_origin).Build(), kFrameId);
  task_environment_.RunUntilIdle();

  // Trigger registered within a fenced frame failed with debug reporting, but
  // no debug report is sent.
  attribution_manager_->HandleTrigger(TriggerBuilder()
                                          .SetReportingOrigin(reporting_origin)
                                          .SetDebugReporting(true)
                                          .SetIsWithinFencedFrame(true)
                                          .Build(),
                                      kFrameId);
  task_environment_.RunUntilIdle();

  // Trigger registered outside a fenced frame tree failed with debug reporting
  // but no debug cookie is set, therefore no debug report is sent.
  attribution_manager_->HandleTrigger(
      TriggerBuilder()
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://r2.test"))
          .SetDebugReporting(true)
          .Build(),
      kFrameId);
  task_environment_.RunUntilIdle();

  checkpoint.Call(1);

  histograms.ExpectTotalCount(kSentVerboseDebugReportTypeMetric, 0);

  // Trigger registered outside a fenced frame tree failed with debug
  // reporting and debug cookie is set.
  attribution_manager_->HandleTrigger(TriggerBuilder()
                                          .SetReportingOrigin(reporting_origin)
                                          .SetDebugReporting(true)
                                          .Build(),
                                      kFrameId);
  task_environment_.RunUntilIdle();

  // kTriggerNoMatchingSource = 6
  histograms.ExpectUniqueSample(kSentVerboseDebugReportTypeMetric, 6, 1);
}

TEST_F(AttributionManagerImplTest,
       EmbedderDisallowsTriggerVerboseDebugReport_NoReportSent) {
  const auto reporting_origin = *SuitableOrigin::Deserialize("https://r1.test");
  cookie_checker_->AddOriginWithDebugCookieSet(reporting_origin);

  EXPECT_CALL(*report_sender_, SendReport(_, _)).Times(0);

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client, IsAttributionReportingOperationAllowed(
                                  _,
                                  AnyOf(AttributionReportingOperation::kTrigger,
                                        AttributionReportingOperation::
                                            kTriggerTransitionalDebugReporting),
                                  _, _, _, _, _))
      .WillRepeatedly(Return(true));
  const auto destination_origin =
      url::Origin::Create(GURL("https://sub.conversion.test/"));
  ExpectOperationAllowed(
      browser_client, AttributionReportingOperation::kTriggerVerboseDebugReport,
      /*source_origin=*/IsNull(), Pointee(destination_origin), reporting_origin,
      /*allowed=*/false);
  ScopedContentBrowserClientSetting setting(&browser_client);

  attribution_manager_->HandleTrigger(TriggerBuilder()
                                          .SetReportingOrigin(reporting_origin)
                                          .SetDebugReporting(true)
                                          .Build(),
                                      kFrameId);
  task_environment_.RunUntilIdle();
}

TEST_F(AttributionManagerImplTest, PendingReportsMetrics) {
  base::HistogramTester histograms;

  RegisterAggregatableSourceAndMatchingTrigger("a");
  task_environment_.FastForwardBy(base::Seconds(10));

  RegisterAggregatableSourceAndMatchingTrigger("b");
  task_environment_.FastForwardBy(base::Seconds(20));

  RegisterAggregatableSourceAndMatchingTrigger("c");
  task_environment_.FastForwardBy(base::Seconds(40));

  ShutdownManager();

  histograms.ExpectTotalCount(kPendingAndBrowserWentOfflineTimeSinceCreation,
                              3);
  EXPECT_EQ(
      histograms.GetTotalSum(kPendingAndBrowserWentOfflineTimeSinceCreation),
      base::Seconds(70 + 60 + 40).InMilliseconds());

  histograms.ExpectTotalCount(kPendingAndBrowserWentOfflineTimeUntilReportTime,
                              3);
  EXPECT_EQ(
      histograms.GetTotalSum(kPendingAndBrowserWentOfflineTimeUntilReportTime),
      ((kFirstReportingWindow - base::Seconds(70)) +
       (kFirstReportingWindow - base::Seconds(60)) +
       (kFirstReportingWindow - base::Seconds(40)))
          .InMilliseconds());
}

TEST_F(AttributionManagerImplTest,
       PendingReportsMetrics_WithoutPendingReports) {
  base::HistogramTester histograms;

  RegisterAggregatableSourceAndMatchingTrigger("a");
  task_environment_.FastForwardBy(base::Seconds(10));

  // Advancing time enough for reports to send
  task_environment_.FastForwardBy(kFirstReportingWindow);

  ShutdownManager();

  // Expect no histograms on shutdown as the reports have already been sent.
  histograms.ExpectTotalCount(kPendingAndBrowserWentOfflineTimeSinceCreation,
                              0);
  histograms.ExpectTotalCount(kPendingAndBrowserWentOfflineTimeUntilReportTime,
                              0);
}

TEST_F(AttributionManagerImplTest, PendingReportsMetrics_Offline) {
  base::HistogramTester histograms;

  RegisterAggregatableSourceAndMatchingTrigger("a");
  task_environment_.FastForwardBy(base::Seconds(10));

  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_NONE);
  SetConnectionTypeAndWaitForObserversToBeNotified(
      network::mojom::ConnectionType::CONNECTION_WIFI);

  RegisterAggregatableSourceAndMatchingTrigger("b");
  task_environment_.FastForwardBy(base::Seconds(20));

  RegisterAggregatableSourceAndMatchingTrigger("c");
  task_environment_.FastForwardBy(base::Seconds(40));

  task_environment_.FastForwardBy(kFirstReportingWindow);

  ShutdownManager();

  // Expect only one histogram as there was only one pending report when it
  // first went offline.
  histograms.ExpectTotalCount(kPendingAndBrowserWentOfflineTimeSinceCreation,
                              1);
  histograms.ExpectTotalCount(kPendingAndBrowserWentOfflineTimeUntilReportTime,
                              1);
}

TEST_F(AttributionManagerImplTest, PendingReportsMetrics_OverLimits) {
  base::HistogramTester histograms;

  for (size_t i = 0; i < (kMaxPendingReportsTimings + 5); i++) {
    RegisterAggregatableSourceAndMatchingTrigger(base::NumberToString(i));
  }

  task_environment_.FastForwardBy(base::Seconds(10));
  RegisterAggregatableSourceAndMatchingTrigger("a");

  ShutdownManager();

  // Expect that events registered past the limit should be dropped.
  histograms.ExpectTotalCount(kPendingAndBrowserWentOfflineTimeSinceCreation,
                              kMaxPendingReportsTimings);
  histograms.ExpectTotalCount(kPendingAndBrowserWentOfflineTimeUntilReportTime,
                              kMaxPendingReportsTimings);

  EXPECT_EQ(
      histograms.GetTotalSum(kPendingAndBrowserWentOfflineTimeSinceCreation),
      (base::Seconds(10) * kMaxPendingReportsTimings).InMilliseconds());
}

TEST_F(AttributionManagerImplTest,
       RemoveAttributionDataByDataKey_NotifiesObservers) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer, OnSourcesChanged);
  EXPECT_CALL(observer, OnReportsChanged);

  base::RunLoop run_loop;
  attribution_manager_->RemoveAttributionDataByDataKey(
      AttributionDataModel::DataKey(
          url::Origin::Create(GURL("https://x.test"))),
      run_loop.QuitClosure());
  run_loop.Run();
}

class AttributionManagerImplCookieBasedDebugReportTest
    : public AttributionManagerImplTest {
 protected:
  void ConfigureStorageDelegate(
      ConfigurableStorageDelegate& delegate) const override {
    delegate.set_max_sources_per_origin(1);
  }
};

TEST_F(AttributionManagerImplCookieBasedDebugReportTest,
       VerboseDebugReport_ReportSent) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  const int kExpectedStatus = 200;
  EXPECT_CALL(observer, OnDebugReportSent(_, kExpectedStatus, _));

  const auto reporting_origin = *SuitableOrigin::Deserialize("https://r1.test");
  cookie_checker_->AddOriginWithDebugCookieSet(reporting_origin);

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(_, _)).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(_, _))
        .WillOnce([](AttributionDebugReport report,
                     DebugReportSentCallback callback) {
          std::move(callback).Run(std::move(report), kExpectedStatus);
        });
  }

  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);

  // Failed without debug reporting.
  attribution_manager_->HandleSource(
      SourceBuilder().SetReportingOrigin(reporting_origin).Build(), kFrameId);
  task_environment_.RunUntilIdle();

  // Source registered within a fenced frame failed with debug reporting, but
  // no debug report is sent.
  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetReportingOrigin(reporting_origin)
                                         .SetDebugReporting(true)
                                         .SetIsWithinFencedFrame(true)
                                         .Build(),
                                     kFrameId);
  task_environment_.RunUntilIdle();

  // Source registered outside a fenced frame failed with debug reporting but no
  // debug cookie is set, therefore no debug report is sent.
  attribution_manager_->HandleSource(
      SourceBuilder()
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://r2.test"))
          .SetDebugReporting(true)
          .Build(),
      kFrameId);
  task_environment_.RunUntilIdle();

  checkpoint.Call(1);

  // Source registered outside a fenced frame with debug reporting and debug
  // cookie is set.
  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetReportingOrigin(reporting_origin)
                                         .SetDebugReporting(true)
                                         .Build(),
                                     kFrameId);
  task_environment_.RunUntilIdle();
}

TEST_F(AttributionManagerImplCookieBasedDebugReportTest,
       EmbedderDisallowsVerboseDebugReport_NoReportSent) {
  const auto reporting_origin = *SuitableOrigin::Deserialize("https://r1.test");

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _,
          AnyOf(
              AttributionReportingOperation::kSource,
              AttributionReportingOperation::kSourceTransitionalDebugReporting),
          _, _, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _, AttributionReportingOperation::kSourceVerboseDebugReport, _,
          Pointee(url::Origin::Create(GURL("https://impression.test/"))),
          IsNull(), Pointee(reporting_origin), _))
      .WillRepeatedly(Return(false));
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_CALL(*report_sender_, SendReport(_, _)).Times(0);

  cookie_checker_->AddOriginWithDebugCookieSet(reporting_origin);

  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  EXPECT_THAT(StoredSources(), SizeIs(1));

  attribution_manager_->HandleSource(SourceBuilder()
                                         .SetReportingOrigin(reporting_origin)
                                         .SetDebugReporting(true)
                                         .Build(),
                                     kFrameId);
  EXPECT_THAT(StoredSources(), SizeIs(1));

  task_environment_.RunUntilIdle();
}

class AttributionManagerImplNullAggregatableReportTest
    : public AttributionManagerImplTest {
 protected:
  void ConfigureStorageDelegate(
      ConfigurableStorageDelegate& delegate) const override {
    delegate.set_null_aggregatable_reports_lookback_days({0});
  }
};

TEST_F(AttributionManagerImplNullAggregatableReportTest, ReportSent) {
  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  Checkpoint checkpoint;
  {
    InSequence seq;
    EXPECT_CALL(observer, OnReportsChanged);
    EXPECT_CALL(*aggregation_service_, AssembleReport).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*aggregation_service_, AssembleReport)
        .WillOnce([](AggregatableReportRequest request,
                     AggregationService::AssemblyCallback callback) {
          std::move(callback).Run(std::move(request),
                                  CreateExampleAggregatableReport(),
                                  AggregationService::AssemblyStatus::kOk);
        });
    EXPECT_CALL(
        *report_sender_,
        SendReport(ReportTypeIs(AttributionReport::Type::kNullAggregatable),
                   /*is_debug_report=*/false, _));
  }

  attribution_manager_->HandleTrigger(
      DefaultAggregatableTriggerBuilder().Build(), kFrameId);

  // Make sure the report is not sent earlier than its report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::Microseconds(1));

  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Microseconds(1));
}

TEST_F(AttributionManagerImplNullAggregatableReportTest,
       EmbedderDisallowsReporting_ReportNotSent) {
  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client,
              IsAttributionReportingOperationAllowed(
                  _, AttributionReportingOperation::kTrigger, _, _, _, _, _))
      .WillRepeatedly(Return(true));
  const auto destination_origin =
      url::Origin::Create(GURL("https://sub.conversion.test/"));
  ExpectOperationAllowed(
      browser_client, AttributionReportingOperation::kReport,
      /*source_origin=*/Pointee(destination_origin),
      Pointee(destination_origin),
      /*reporting_origin=*/url::Origin::Create(GURL("https://report.test/")),
      /*allowed=*/false);
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_CALL(*aggregation_service_, AssembleReport).Times(0);

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  EXPECT_CALL(observer,
              OnReportSent(
                  ReportTypeIs(AttributionReport::Type::kNullAggregatable),
                  /*is_debug_report=*/false,
                  Property(&SendResult::status, SendResult::Status::kDropped)));

  attribution_manager_->HandleTrigger(
      DefaultAggregatableTriggerBuilder().Build(), kFrameId);
  EXPECT_THAT(StoredReports(), SizeIs(1));

  task_environment_.FastForwardBy(kFirstReportingWindow);
  EXPECT_THAT(StoredReports(), IsEmpty());
}

TEST_F(AttributionManagerImplTest,
       OsRegistrationVerboseDebugReport_ReportSent) {
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state(
      AttributionOsLevelManager::ApiState::kEnabled);

  for (const bool is_os_source : {true, false}) {
    SCOPED_TRACE(is_os_source);

    base::HistogramTester histograms;

    const OsRegistration registration(
        {OsRegistrationItem(GURL("https://a.test/x"),
                            /*debug_reporting=*/true)},
        /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
        is_os_source
            ? std::make_optional<AttributionInputEvent>(AttributionInputEvent())
            : std::nullopt,
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar);

    EXPECT_CALL(*os_level_manager_, Register)
        .WillOnce(base::test::RunOnceCallback<2>(registration,
                                                 std::vector<bool>{true}));

    EXPECT_CALL(*report_sender_, SendReport(_, _));

    attribution_manager_->HandleOsRegistration(registration);

    histograms.ExpectUniqueSample(
        kSentVerboseDebugReportTypeMetric,
        is_os_source ? /*kOsSourceDelegated=*/24 : /*kOsTriggerDelegated=*/25,
        1);
  }
}

TEST_F(AttributionManagerImplTest,
       EmbedderDisallowsOsRegistrationVerboseDebugReport_NoReportSent) {
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state(
      AttributionOsLevelManager::ApiState::kEnabled);

  const GURL kRegistrationUrl("https://a.test/x");

  for (const bool is_os_source : {true, false}) {
    SCOPED_TRACE(is_os_source);

    base::HistogramTester histograms;

    const OsRegistration registration(
        {OsRegistrationItem(kRegistrationUrl, /*debug_reporting=*/true)},
        /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
        /*input_event=*/
        is_os_source
            ? std::make_optional<AttributionInputEvent>(AttributionInputEvent())
            : std::nullopt,
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar);

    EXPECT_CALL(*report_sender_, SendReport(_, _)).Times(0);

    MockAttributionReportingContentBrowserClient browser_client;
    EXPECT_CALL(browser_client,
                IsAttributionReportingOperationAllowed(
                    _,
                    is_os_source ? AttributionReportingOperation::kOsSource
                                 : AttributionReportingOperation::kOsTrigger,
                    _, _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(browser_client,
                IsAttributionReportingOperationAllowed(
                    _,
                    is_os_source ? AttributionReportingOperation::
                                       kOsSourceTransitionalDebugReporting
                                 : AttributionReportingOperation::
                                       kOsTriggerTransitionalDebugReporting,
                    _, _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(
        browser_client,
        IsAttributionReportingOperationAllowed(
            _,
            is_os_source
                ? AttributionReportingOperation::kOsSourceVerboseDebugReport
                : AttributionReportingOperation::kOsTriggerVerboseDebugReport,
            _, _, _, Pointee(url::Origin::Create(kRegistrationUrl)), _))
        .WillOnce(Return(false));
    ScopedContentBrowserClientSetting setting(&browser_client);

    EXPECT_CALL(*os_level_manager_, Register)
        .WillOnce(base::test::RunOnceCallback<2>(registration,
                                                 std::vector<bool>{false}));

    attribution_manager_->HandleOsRegistration(registration);

    histograms.ExpectTotalCount(kSentVerboseDebugReportTypeMetric, 0);
  }
}

TEST_F(AttributionManagerImplTest,
       SourceRegistrationDelayedByAttestationsLoading) {
  ShutdownManager();

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client, AddPrivacySandboxAttestationsObserver)
      .WillOnce(Return(false));
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _,
          AnyOf(
              AttributionReportingOperation::kSource,
              AttributionReportingOperation::kSourceTransitionalDebugReporting),
          _, _, _, _, _))
      .WillRepeatedly(Return(true));
  ScopedContentBrowserClientSetting setting(&browser_client);

  base::HistogramTester histograms;

  CreateManager();

  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  EXPECT_THAT(StoredSources(), IsEmpty());

  NotifyAttestationsLoaded();
  EXPECT_THAT(StoredSources(), SizeIs(1));

  histograms.ExpectTotalCount("Conversions.DelayOnAttestationsLoaded", 1);
  histograms.ExpectUniqueSample(
      "Conversions.NumEventsQueuedOnAttestationsLoaded", 1, 1);
  histograms.ExpectTotalCount(
      "Conversions.NumOsEventsQueuedOnAttestationsLoaded", 0);
}

TEST_F(AttributionManagerImplTest,
       SourceRegistrationDelayedByAttestationsLoadingAtTimeout) {
  ShutdownManager();

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client, AddPrivacySandboxAttestationsObserver)
      .WillOnce(Return(false));
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _,
          AnyOf(
              AttributionReportingOperation::kSource,
              AttributionReportingOperation::kSourceTransitionalDebugReporting),
          _, _, _, _, _))
      .WillRepeatedly(Return(true));
  ScopedContentBrowserClientSetting setting(&browser_client);

  base::HistogramTester histograms;

  CreateManager();

  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  EXPECT_THAT(StoredSources(), IsEmpty());

  task_environment_.FastForwardBy(kPrivacySandboxAttestationsTimeout);
  EXPECT_THAT(StoredSources(), SizeIs(1));

  histograms.ExpectTotalCount("Conversions.DelayOnAttestationsLoaded", 1);
}

TEST_F(AttributionManagerImplTest, OsRegistrationDelayedByAttestationsLoading) {
  ShutdownManager();

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client, AddPrivacySandboxAttestationsObserver)
      .WillOnce(Return(false));
  EXPECT_CALL(browser_client,
              IsAttributionReportingOperationAllowed(
                  _,
                  AnyOf(AttributionReportingOperation::kOsSource,
                        AttributionReportingOperation::
                            kOsSourceTransitionalDebugReporting),
                  _, _, _, _, _))
      .WillRepeatedly(Return(true));
  ScopedContentBrowserClientSetting setting(&browser_client);

  base::HistogramTester histograms;

  CreateManager();

  Checkpoint checkpoint;

  {
    InSequence seq;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*os_level_manager_, Register);
  }

  attribution_manager_->HandleOsRegistration(OsRegistration(
      {OsRegistrationItem(/*url=*/GURL("https://r.test"),
                          /*debug_reporting=*/false)},
      /*top_level_origin=*/url::Origin::Create(GURL("https://s.test")),
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kRegistrar));

  checkpoint.Call(1);

  NotifyAttestationsLoaded();

  histograms.ExpectTotalCount("Conversions.DelayOnAttestationsLoaded", 1);
  histograms.ExpectUniqueSample(
      "Conversions.NumOsEventsQueuedOnAttestationsLoaded", 1, 1);
  histograms.ExpectTotalCount("Conversions.NumEventsQueuedOnAttestationsLoaded",
                              0);
}

TEST_F(AttributionManagerImplTest,
       ReportAtStartupDelayedByAttestationsLoading) {
  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  ShutdownManager();

  // Fast-forward past the reporting window.
  task_environment_.FastForwardBy(kFirstReportingWindow);

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client, AddPrivacySandboxAttestationsObserver)
      .WillOnce(Return(false));
  EXPECT_CALL(browser_client,
              IsAttributionReportingOperationAllowed(
                  _, AttributionReportingOperation::kReport, _, _, _, _, _))
      .WillOnce(Return(true));
  ScopedContentBrowserClientSetting setting(&browser_client);

  base::HistogramTester histograms;

  CreateManager();

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(An<AttributionReport>(), _, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(An<AttributionReport>(), _, _));
  }

  task_environment_.FastForwardBy(kDefaultOfflineReportDelay.max);

  // The report is not sent until the attestations are loaded.
  NotifyAttestationsLoaded();

  checkpoint.Call(1);

  task_environment_.FastForwardBy(kDefaultOfflineReportDelay.max);

  histograms.ExpectTotalCount("Conversions.DelayOnAttestationsLoaded", 1);
}

TEST_F(AttributionManagerImplTest,
       ReportAtStartupDelayedByAttestationsLoadingAtTimeout) {
  attribution_manager_->HandleSource(SourceBuilder().Build(), kFrameId);
  attribution_manager_->HandleTrigger(DefaultTrigger(), kFrameId);

  ShutdownManager();

  // Fast-forward past the reporting window.
  task_environment_.FastForwardBy(kFirstReportingWindow);

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client, AddPrivacySandboxAttestationsObserver)
      .WillOnce(Return(false));
  EXPECT_CALL(browser_client,
              IsAttributionReportingOperationAllowed(
                  _, AttributionReportingOperation::kReport, _, _, _, _, _))
      .WillOnce(Return(true));
  ScopedContentBrowserClientSetting setting(&browser_client);

  base::HistogramTester histograms;

  CreateManager();

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*report_sender_, SendReport(An<AttributionReport>(), _, _))
        .Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*report_sender_, SendReport(An<AttributionReport>(), _, _));
  }

  task_environment_.FastForwardBy(kDefaultOfflineReportDelay.max);

  // The report is not sent until attestations timeout.
  task_environment_.FastForwardBy(kPrivacySandboxAttestationsTimeout -
                                  kDefaultOfflineReportDelay.max);

  checkpoint.Call(1);

  task_environment_.FastForwardBy(kDefaultOfflineReportDelay.max);

  histograms.ExpectTotalCount("Conversions.DelayOnAttestationsLoaded", 1);
}

TEST_F(AttributionManagerImplTest, RegistrationHeaderErrorDebugReport) {
  for (const bool allowed : {false, true}) {
    SCOPED_TRACE(allowed);

    base::HistogramTester histograms;

    MockAttributionReportingContentBrowserClient browser_client;
    EXPECT_CALL(browser_client, IsAttributionReportingAllowedForContext)
        .WillOnce(Return(allowed));
    ScopedContentBrowserClientSetting setting(&browser_client);

    EXPECT_CALL(*report_sender_, SendReport(_, _)).Times(allowed);

    attribution_manager_->ReportRegistrationHeaderError(
        *SuitableOrigin::Deserialize("https://r.test"),
        attribution_reporting::RegistrationHeaderError(
            /*header_value=*/"!!!", attribution_reporting::mojom::
                                        SourceRegistrationError::kInvalidJson),
        *SuitableOrigin::Deserialize("https://c.test"),
        /*is_within_fenced_frame=*/false, kFrameId);

    // kHeaderParsingError = 28
    histograms.ExpectUniqueSample(kSentVerboseDebugReportTypeMetric,
                                  /*sample=*/28, allowed);
  }
}

// Regression test for http://crbug.com/331915077. This test will fail flakily
// if the manager's queue processing uses reentrant calls.
TEST_F(AttributionManagerImplTest, OsQueueNotReentrant) {
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state(
      AttributionOsLevelManager::ApiState::kEnabled);

  const GURL kRegistrationUrl1("https://r1.test/x");

  const auto kTopLevelOrigin1 = url::Origin::Create(GURL("https://o1.test"));

  cookie_checker_->DeferCallbacks();

  for (int i = 0; i < 5; ++i) {
    attribution_manager_->HandleOsRegistration(OsRegistration(
        {OsRegistrationItem(kRegistrationUrl1, /*debug_reporting=*/false)},
        kTopLevelOrigin1, AttributionInputEvent(),
        /*is_within_fenced_frame=*/false, kFrameId, kRegistrar));
    cookie_checker_->DeferCallbacks(false);
  }

  cookie_checker_->RunNextDeferredCallback(true);
  task_environment_.RunUntilIdle();
}

TEST_F(AttributionManagerImplTest,
       AggregatableReportWithTriggerContextId_MetricsRecorded) {
  base::HistogramTester histograms;

  attribution_manager_->HandleSource(TestAggregatableSourceProvider()
                                         .GetBuilder()
                                         .SetExpiry(kImpressionExpiry)
                                         .Build(),
                                     kFrameId);
  attribution_manager_->HandleTrigger(
      DefaultAggregatableTriggerBuilder()
          .SetTriggerContextId("example")
          .SetSourceRegistrationTimeConfig(
              attribution_reporting::mojom::SourceRegistrationTimeConfig::
                  kExclude)
          .Build(/*generate_event_trigger_data=*/false),
      kFrameId);

  task_environment_.FastForwardBy(base::TimeDelta());

  histograms.ExpectTotalCount("Conversions.AggregatableReport.ExtraReportDelay",
                              1);
  histograms.ExpectTotalCount(
      "Conversions.AggregatableReport.ContextID.ExtraReportDelay", 1);
}

TEST_F(AttributionManagerImplTest, AggregatableDebugReport_ReportSent) {
  base::test::ScopedFeatureList scoped_feature_list(
      attribution_reporting::features::kAttributionAggregatableDebugReporting);

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  const AggregatableReport assembled_report =
      CreateExampleAggregatableDebugReport();

  const int kExpectedStatus = 200;
  EXPECT_CALL(
      observer,
      OnAggregatableDebugReportSent(
          _, _, _,
          Field(&SendAggregatableDebugReportResult::result,
                ::testing::VariantWith<SendAggregatableDebugReportResult::Sent>(
                    Field(&SendAggregatableDebugReportResult::Sent::status,
                          kExpectedStatus)))))
      .Times(3);

  EXPECT_CALL(*aggregation_service_, AssembleReport)
      .WillOnce([&](AggregatableReportRequest request,
                    AggregationService::AssemblyCallback callback) {
        EXPECT_THAT(request.payload_contents().contributions,
                    UnorderedElementsAre(
                        blink::mojom::AggregatableReportHistogramContribution(
                            /*bucket=*/3,
                            /*value=*/123, /*filtering_id=*/std::nullopt)));
        std::move(callback).Run(std::move(request), assembled_report,
                                AggregationService::AssemblyStatus::kOk);
      })
      .WillOnce([&](AggregatableReportRequest request,
                    AggregationService::AssemblyCallback callback) {
        EXPECT_THAT(request.payload_contents().contributions,
                    UnorderedElementsAre(
                        blink::mojom::AggregatableReportHistogramContribution(
                            /*bucket=*/7,
                            /*value=*/900, /*filtering_id=*/std::nullopt)));
        std::move(callback).Run(std::move(request), assembled_report,
                                AggregationService::AssemblyStatus::kOk);
      })
      .WillOnce([&](AggregatableReportRequest request,
                    AggregationService::AssemblyCallback callback) {
        EXPECT_THAT(request.payload_contents().contributions, IsEmpty());
        std::move(callback).Run(std::move(request), assembled_report,
                                AggregationService::AssemblyStatus::kOk);
      });

  EXPECT_CALL(*report_sender_, SendReport(An<AggregatableDebugReport>(), _, _))
      .Times(3)
      .WillRepeatedly([&](AggregatableDebugReport report,
                          base::Value::Dict report_body,
                          AggregatableDebugReportSentCallback callback) {
        EXPECT_THAT(report_body,
                    base::test::IsJson(assembled_report.GetAsJson()));
        std::move(callback).Run(std::move(report), std::move(report_body),
                                kExpectedStatus);
      });

  attribution_manager_->HandleSource(
      SourceBuilder()
          .SetFilterData(
              *attribution_reporting::FilterData::Create({{"x", {"a"}}}))
          .SetAggregatableDebugReportingConfig(
              *SourceAggregatableDebugReportingConfig::Create(
                  /*budget=*/1024,
                  AggregatableDebugReportingConfig(
                      /*key_piece=*/1,
                      /*debug_data=*/
                      {{DebugDataType::kSourceSuccess,
                        *attribution_reporting::
                            AggregatableDebugReportingContribution::Create(
                                /*key_piece=*/2, /*value=*/123)}},
                      /*aggregation_coordinator_origin=*/std::nullopt)))
          .Build(),
      kFrameId);

  const auto trigger =
      TriggerBuilder()
          .SetFilterPair(attribution_reporting::FilterPair(
              /*positive=*/{*attribution_reporting::FilterConfig::Create(
                  {{"x", {"b"}}})},
              /*negative=*/{}))
          .SetAggregatableDebugReportingConfig(AggregatableDebugReportingConfig(
              /*key_piece=*/3,
              /*debug_data=*/
              {{DebugDataType::kTriggerNoMatchingFilterData,
                *attribution_reporting::AggregatableDebugReportingContribution::
                    Create(
                        /*key_piece=*/4,
                        /*value=*/900)}},
              /*aggregation_coordinator_origin=*/std::nullopt))
          .Build();

  attribution_manager_->HandleTrigger(trigger, kFrameId);

  // Insufficient budget, null report.
  attribution_manager_->HandleTrigger(trigger, kFrameId);

  task_environment_.RunUntilIdle();
}

TEST_F(AttributionManagerImplTest,
       EmbedderDisallowsSourceAggregatableDebugReport_ReportNotSent) {
  base::test::ScopedFeatureList scoped_feature_list(
      attribution_reporting::features::kAttributionAggregatableDebugReporting);

  const auto source_origin = *SuitableOrigin::Deserialize("https://s.test");
  const auto reporting_origin = *SuitableOrigin::Deserialize("https://r.test");

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _,
          AnyOf(
              AttributionReportingOperation::kSource,
              AttributionReportingOperation::kSourceTransitionalDebugReporting),
          _, _, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _, AttributionReportingOperation::kSourceAggregatableDebugReport, _,
          Pointee(source_origin), IsNull(), Pointee(reporting_origin), _))
      .WillOnce(Return(false));

  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_CALL(*aggregation_service_, AssembleReport).Times(0);

  attribution_manager_->HandleSource(
      SourceBuilder()
          .SetSourceOrigin(source_origin)
          .SetReportingOrigin(reporting_origin)
          .SetDebugReporting(false)
          .SetAggregatableDebugReportingConfig(
              *SourceAggregatableDebugReportingConfig::Create(
                  /*budget=*/1024,
                  AggregatableDebugReportingConfig(
                      /*key_piece=*/1,
                      /*debug_data=*/
                      {{DebugDataType::kSourceSuccess,
                        *attribution_reporting::
                            AggregatableDebugReportingContribution::Create(
                                /*key_piece=*/2, /*value=*/123)}},
                      /*aggregation_coordinator_origin=*/std::nullopt)))
          .Build(),
      kFrameId);

  task_environment_.RunUntilIdle();
}

TEST_F(AttributionManagerImplTest,
       EmbedderDisallowsTriggerAggregatableDebugReport_ReportNotSent) {
  base::test::ScopedFeatureList scoped_feature_list(
      attribution_reporting::features::kAttributionAggregatableDebugReporting);

  const auto destination_origin =
      *SuitableOrigin::Deserialize("https://s.test");
  const auto reporting_origin = *SuitableOrigin::Deserialize("https://r.test");

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client, IsAttributionReportingOperationAllowed(
                                  _,
                                  AnyOf(AttributionReportingOperation::kTrigger,
                                        AttributionReportingOperation::
                                            kTriggerTransitionalDebugReporting),
                                  _, _, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(
      browser_client,
      IsAttributionReportingOperationAllowed(
          _, AttributionReportingOperation::kTriggerAggregatableDebugReport, _,
          IsNull(), Pointee(destination_origin), Pointee(reporting_origin), _))
      .WillOnce(Return(false));

  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_CALL(*aggregation_service_, AssembleReport).Times(0);

  attribution_manager_->HandleTrigger(
      TriggerBuilder()
          .SetDestinationOrigin(destination_origin)
          .SetReportingOrigin(reporting_origin)
          .SetAggregatableDebugReportingConfig(AggregatableDebugReportingConfig(
              /*key_piece=*/1,
              /*debug_data=*/
              {{DebugDataType::kTriggerNoMatchingSource,
                *attribution_reporting::AggregatableDebugReportingContribution::
                    Create(
                        /*key_piece=*/2,
                        /*value=*/123)}},
              /*aggregation_coordinator_origin=*/std::nullopt))
          .Build(),
      kFrameId);

  task_environment_.RunUntilIdle();
}

TEST_F(AttributionManagerImplTest, SetDebugMode_NotifiesObservers) {
  MockAttributionObserver observer;

  Checkpoint checkpoint;
  {
    InSequence seq;

    // Called with the initial value upon observation.
    EXPECT_CALL(observer, OnDebugModeChanged(false));
    EXPECT_CALL(checkpoint, Call(1));
    // Called with the new value via `SetDebugMode()`.
    EXPECT_CALL(observer, OnDebugModeChanged(true));
    EXPECT_CALL(checkpoint, Call(2));
    // Called with the new value upon observation.
    EXPECT_CALL(observer, OnDebugModeChanged(true));
  }

  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager_.get());

  checkpoint.Call(1);

  base::RunLoop run_loop;
  attribution_manager_->SetDebugMode(true, run_loop.QuitClosure());
  run_loop.Run();

  checkpoint.Call(2);

  observation.Reset();
  observation.Observe(attribution_manager_.get());
}

}  // namespace content
