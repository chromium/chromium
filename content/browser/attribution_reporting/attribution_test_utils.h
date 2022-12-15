// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_

#include <stdint.h>

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/guid.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/bounded_list.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/attribution_config.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace mojo {

template <typename Interface>
class PendingReceiver;

}  // namespace mojo

namespace url {
class Origin;
}  // namespace url

namespace content {

class AttributionDataHostManager;
class AttributionObserver;
class AttributionTrigger;

enum class RateLimitResult : int;

const AttributionSourceType kSourceTypes[] = {
    AttributionSourceType::kNavigation,
    AttributionSourceType::kEvent,
};

class MockAttributionReportingContentBrowserClient
    : public TestContentBrowserClient {
 public:
  MockAttributionReportingContentBrowserClient();
  ~MockAttributionReportingContentBrowserClient() override;

  // ContentBrowserClient:
  MOCK_METHOD(bool,
              IsAttributionReportingOperationAllowed,
              (content::BrowserContext * browser_context,
               AttributionReportingOperation operation,
               const url::Origin* source_origin,
               const url::Origin* destination_origin,
               const url::Origin* reporting_origin),
              (override));
};

class MockAttributionHost : public AttributionHost {
 public:
  static MockAttributionHost* Override(WebContents* web_contents);

  ~MockAttributionHost() override;

  MOCK_METHOD(
      void,
      RegisterDataHost,
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
       blink::mojom::AttributionRegistrationType),
      (override));

  MOCK_METHOD(
      void,
      RegisterNavigationDataHost,
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
       const blink::AttributionSrcToken& attribution_src_token,
       blink::mojom::AttributionNavigationType),
      (override));

 private:
  explicit MockAttributionHost(WebContents* web_contents);
};

class MockDataHost : public blink::mojom::AttributionDataHost {
 public:
  explicit MockDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host);
  ~MockDataHost() override;

  void WaitForSourceData(size_t num_source_data);
  void WaitForTriggerData(size_t num_trigger_data);

  const std::vector<attribution_reporting::SourceRegistration>& source_data()
      const {
    return source_data_;
  }

  const std::vector<attribution_reporting::TriggerRegistration>& trigger_data()
      const {
    return trigger_data_;
  }

  mojo::Receiver<blink::mojom::AttributionDataHost>& receiver() {
    return receiver_;
  }

 private:
  // blink::mojom::AttributionDataHost:
  void SourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::SourceRegistration) override;
  void TriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::TriggerRegistration) override;

  size_t min_source_data_count_ = 0;
  std::vector<attribution_reporting::SourceRegistration> source_data_;

  size_t min_trigger_data_count_ = 0;
  std::vector<attribution_reporting::TriggerRegistration> trigger_data_;

  base::RunLoop wait_loop_;
  mojo::Receiver<blink::mojom::AttributionDataHost> receiver_{this};
};

base::GUID DefaultExternalReportID();

class ConfigurableStorageDelegate : public AttributionStorageDelegate {
 public:
  ConfigurableStorageDelegate();
  ~ConfigurableStorageDelegate() override;

  // AttributionStorageDelegate
  base::Time GetEventLevelReportTime(const CommonSourceInfo& source,
                                     base::Time trigger_time) const override;
  base::Time GetAggregatableReportTime(base::Time trigger_time) const override;
  base::TimeDelta GetDeleteExpiredSourcesFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;
  base::GUID NewReportID() const override;
  absl::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const override;
  void ShuffleReports(std::vector<AttributionReport>& reports) override;
  RandomizedResponse GetRandomizedResponse(
      const CommonSourceInfo& source) override;

  void set_max_attributions_per_source(int max);

  void set_max_sources_per_origin(int max);

  void set_max_reports_per_destination(AttributionReport::Type report_type,
                                       int max);

  void set_max_destinations_per_source_site_reporting_origin(int max);

  void set_aggregatable_budget_per_source(int64_t max);

  void set_rate_limits(AttributionConfig::RateLimitConfig c);

  void set_delete_expired_sources_frequency(base::TimeDelta frequency);

  void set_delete_expired_rate_limits_frequency(base::TimeDelta frequency);

  void set_report_delay(base::TimeDelta report_delay);

  void set_offline_report_delay_config(
      absl::optional<OfflineReportDelayConfig> config);

  void set_reverse_reports_on_shuffle(bool reverse);

  // Note that these rates are *not* used to produce a randomized response; that
  // is controlled deterministically by `set_randomized_response()`.
  void set_randomized_response_rates(double navigation, double event);

  void set_randomized_response(RandomizedResponse randomized_response);

  void set_trigger_data_cardinality(uint64_t navigation, uint64_t event);

  void set_source_event_id_cardinality(uint64_t cardinality);

  // Detaches the delegate from its current sequence in preparation for being
  // moved to storage, which runs on its own sequence.
  void DetachFromSequence();

 private:
  base::TimeDelta delete_expired_sources_frequency_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::TimeDelta delete_expired_rate_limits_frequency_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::TimeDelta report_delay_ GUARDED_BY_CONTEXT(sequence_checker_);

  absl::optional<OfflineReportDelayConfig> offline_report_delay_config_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // If true, `ShuffleReports()` reverses the reports to allow testing the
  // proper call from `AttributionStorage::GetAttributionReports()`.
  bool reverse_reports_on_shuffle_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  RandomizedResponse randomized_response_
      GUARDED_BY_CONTEXT(sequence_checker_) = absl::nullopt;
};

class MockAttributionManager : public AttributionManager {
 public:
  MockAttributionManager();
  ~MockAttributionManager() override;

  // AttributionManager:
  MOCK_METHOD(void, HandleSource, (StorableSource source), (override));

  MOCK_METHOD(void, HandleTrigger, (AttributionTrigger trigger), (override));

  MOCK_METHOD(void,
              GetActiveSourcesForWebUI,
              (base::OnceCallback<void(std::vector<StoredSource>)> callback),
              (override));

  MOCK_METHOD(
      void,
      GetPendingReportsForInternalUse,
      (AttributionReport::Types report_types,
       int limit,
       base::OnceCallback<void(std::vector<AttributionReport>)> callback),
      (override));

  MOCK_METHOD(void,
              SendReportsForWebUI,
              (const std::vector<AttributionReport::Id>& ids,
               base::OnceClosure done),
              (override));

  MOCK_METHOD(void,
              ClearData,
              (base::Time delete_begin,
               base::Time delete_end,
               StoragePartition::StorageKeyMatcherFunction filter,
               BrowsingDataFilterBuilder* filter_builder,
               bool delete_rate_limit_data,
               base::OnceClosure done),
              (override));

  MOCK_METHOD(void,
              NotifyFailedSourceRegistration,
              (const std::string& header_value,
               const attribution_reporting::SuitableOrigin& reporting_origin,
               attribution_reporting::mojom::SourceRegistrationError),
              (override));

  void AddObserver(AttributionObserver* observer) override;
  void RemoveObserver(AttributionObserver* observer) override;
  AttributionDataHostManager* GetDataHostManager() override;

  void NotifySourcesChanged();
  void NotifyReportsChanged(AttributionReport::Type report_type);
  void NotifySourceHandled(
      const StorableSource& source,
      StorableSource::Result result,
      absl::optional<uint64_t> cleared_debug_key = absl::nullopt);
  void NotifyReportSent(const AttributionReport& report,
                        bool is_debug_report,
                        const SendResult& info);
  void NotifyTriggerHandled(
      const AttributionTrigger& trigger,
      const CreateReportResult& result,
      absl::optional<uint64_t> cleared_debug_key = absl::nullopt);
  void NotifySourceRegistrationFailure(
      const std::string& header_value,
      const attribution_reporting::SuitableOrigin& reporting_origin,
      attribution_reporting::mojom::SourceRegistrationError);
  void NotifyDebugReportSent(const AttributionDebugReport&,
                             int status,
                             base::Time time);

  void SetDataHostManager(std::unique_ptr<AttributionDataHostManager> manager);

 private:
  std::unique_ptr<AttributionDataHostManager> data_host_manager_;
  base::ObserverList<AttributionObserver, /*check_empty=*/true> observers_;
};

class MockAttributionObserver : public AttributionObserver {
 public:
  MockAttributionObserver();
  ~MockAttributionObserver() override;

  MockAttributionObserver(const MockAttributionObserver&) = delete;
  MockAttributionObserver(MockAttributionObserver&&) = delete;

  MockAttributionObserver& operator=(const MockAttributionObserver&) = delete;
  MockAttributionObserver& operator=(MockAttributionObserver&&) = delete;

  MOCK_METHOD(void, OnSourcesChanged, (), (override));

  MOCK_METHOD(void, OnReportsChanged, (AttributionReport::Type), (override));

  MOCK_METHOD(void,
              OnSourceHandled,
              (const StorableSource& source,
               absl::optional<uint64_t> cleared_debug_key,
               StorableSource::Result result),
              (override));

  MOCK_METHOD(void,
              OnReportSent,
              (const AttributionReport& report,
               bool is_debug_report,
               const SendResult& info),
              (override));

  MOCK_METHOD(void,
              OnDebugReportSent,
              (const AttributionDebugReport& report,
               int status,
               base::Time time),
              (override));

  MOCK_METHOD(void,
              OnTriggerHandled,
              (const AttributionTrigger& trigger,
               absl::optional<uint64_t> cleared_debug_key,
               const CreateReportResult& result),
              (override));
};

// WebContentsObserver that waits until a source is available on a
// navigation handle for a finished navigation.
class SourceObserver : public TestNavigationObserver {
 public:
  explicit SourceObserver(WebContents* contents, size_t num_impressions = 1u);
  ~SourceObserver() override;

  // WebContentsObserver:
  void OnDidFinishNavigation(NavigationHandle* navigation_handle) override;

  const blink::Impression& last_impression() const { return *last_impression_; }

  // Waits for |expected_num_impressions_| navigations with impressions, and
  // returns the last impression.
  const blink::Impression& Wait();

  bool WaitForNavigationWithNoImpression();

 private:
  size_t num_impressions_ = 0u;
  const size_t expected_num_impressions_ = 0u;
  absl::optional<blink::Impression> last_impression_;
  bool waiting_for_null_impression_ = false;
  base::RunLoop impression_loop_;
};

// Helper class to construct a StorableSource for tests using default data.
// StorableSource members are not mutable after construction requiring a
// builder pattern.
class SourceBuilder {
 public:
  explicit SourceBuilder(base::Time time = base::Time::Now());
  ~SourceBuilder();

  SourceBuilder(const SourceBuilder&);
  SourceBuilder(SourceBuilder&&);

  SourceBuilder& operator=(const SourceBuilder&);
  SourceBuilder& operator=(SourceBuilder&&);

  SourceBuilder& SetExpiry(base::TimeDelta delta);

  SourceBuilder& SetEventReportWindow(base::TimeDelta delta);

  SourceBuilder& SetAggregatableReportWindow(base::TimeDelta delta);

  SourceBuilder& SetSourceEventId(uint64_t source_event_id);

  SourceBuilder& SetSourceOrigin(attribution_reporting::SuitableOrigin);

  SourceBuilder& SetDestinationOrigin(attribution_reporting::SuitableOrigin);

  SourceBuilder& SetDestinationOrigins(
      base::flat_set<attribution_reporting::SuitableOrigin>);

  SourceBuilder& SetReportingOrigin(attribution_reporting::SuitableOrigin);

  SourceBuilder& SetSourceType(AttributionSourceType source_type);

  SourceBuilder& SetPriority(int64_t priority);

  SourceBuilder& SetAttributionLogic(
      StoredSource::AttributionLogic attribution_logic);

  SourceBuilder& SetFilterData(attribution_reporting::FilterData filter_data);

  SourceBuilder& SetActiveState(StoredSource::ActiveState active_state);

  SourceBuilder& SetDebugKey(absl::optional<uint64_t> debug_key);

  SourceBuilder& SetSourceId(StoredSource::Id source_id);

  SourceBuilder& SetDedupKeys(std::vector<uint64_t> dedup_keys);

  SourceBuilder& SetAggregationKeys(
      attribution_reporting::AggregationKeys aggregation_keys);

  SourceBuilder& SetAggregatableBudgetConsumed(
      int64_t aggregatable_budget_consumed);

  SourceBuilder& SetAggregatableDedupKeys(
      std::vector<uint64_t> aggregatable_dedup_keys);

  SourceBuilder& SetIsWithinFencedFrame(bool is_within_fenced_frame);

  SourceBuilder& SetDebugReporting(bool debug_reporting);

  StorableSource Build() const;

  StoredSource BuildStored() const;

  CommonSourceInfo BuildCommonInfo() const;

 private:
  uint64_t source_event_id_ = 123;
  base::Time source_time_;
  base::TimeDelta expiry_;
  absl::optional<base::TimeDelta> event_report_window_;
  absl::optional<base::TimeDelta> aggregatable_report_window_;
  attribution_reporting::SuitableOrigin source_origin_;
  base::flat_set<attribution_reporting::SuitableOrigin> destination_origins_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  AttributionSourceType source_type_ = AttributionSourceType::kNavigation;
  int64_t priority_ = 0;
  StoredSource::AttributionLogic attribution_logic_ =
      StoredSource::AttributionLogic::kTruthfully;
  attribution_reporting::FilterData filter_data_;
  StoredSource::ActiveState active_state_ = StoredSource::ActiveState::kActive;
  absl::optional<uint64_t> debug_key_;
  // `base::StrongAlias` does not automatically initialize the value here.
  // Ensure that we don't use uninitialized memory.
  StoredSource::Id source_id_{0};
  std::vector<uint64_t> dedup_keys_;
  attribution_reporting::AggregationKeys aggregation_keys_;
  int64_t aggregatable_budget_consumed_ = 0;
  std::vector<uint64_t> aggregatable_dedup_keys_;
  bool is_within_fenced_frame_ = false;
  bool debug_reporting_ = false;
};

// Returns a AttributionTrigger with default data which matches the default
// impressions created by SourceBuilder.
AttributionTrigger DefaultTrigger();

// Helper class to construct a AttributionTrigger for tests using default data.
// AttributionTrigger members are not mutable after construction requiring a
// builder pattern.
class TriggerBuilder {
 public:
  TriggerBuilder();
  ~TriggerBuilder();

  TriggerBuilder(const TriggerBuilder&);
  TriggerBuilder(TriggerBuilder&&);

  TriggerBuilder& operator=(const TriggerBuilder&);
  TriggerBuilder& operator=(TriggerBuilder&&);

  TriggerBuilder& SetTriggerData(uint64_t trigger_data);

  TriggerBuilder& SetEventSourceTriggerData(uint64_t event_source_trigger_data);

  TriggerBuilder& SetDestinationOrigin(attribution_reporting::SuitableOrigin);

  TriggerBuilder& SetReportingOrigin(attribution_reporting::SuitableOrigin);

  TriggerBuilder& SetPriority(int64_t priority);

  TriggerBuilder& SetDedupKey(absl::optional<uint64_t> dedup_key);

  TriggerBuilder& SetDebugKey(absl::optional<uint64_t> debug_key);

  TriggerBuilder& SetAggregatableTriggerData(
      std::vector<attribution_reporting::AggregatableTriggerData>);

  TriggerBuilder& SetAggregatableValues(
      attribution_reporting::AggregatableValues);

  TriggerBuilder& SetAggregatableDedupKey(
      absl::optional<uint64_t> aggregatable_dedup_key);

  TriggerBuilder& SetIsWithinFencedFrame(bool is_within_fenced_frame);

  TriggerBuilder& SetDebugReporting(bool debug_reporting);

  TriggerBuilder& SetAggregationCoordinator(
      ::aggregation_service::mojom::AggregationCoordinator);

  AttributionTrigger Build(bool generate_event_trigger_data = true) const;

 private:
  uint64_t trigger_data_ = 111;
  uint64_t event_source_trigger_data_ = 0;
  attribution_reporting::SuitableOrigin destination_origin_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  int64_t priority_ = 0;
  absl::optional<uint64_t> dedup_key_;
  absl::optional<uint64_t> debug_key_;
  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data_;
  attribution_reporting::AggregatableValues aggregatable_values_;
  absl::optional<uint64_t> aggregatable_dedup_key_;
  bool is_within_fenced_frame_ = false;
  bool debug_reporting_ = false;
  ::aggregation_service::mojom::AggregationCoordinator
      aggregation_coordinator_ =
          ::aggregation_service::mojom::AggregationCoordinator::kDefault;
};

// Helper class to construct an `AttributionInfo` for tests using default data.
class AttributionInfoBuilder {
 public:
  explicit AttributionInfoBuilder(StoredSource source);
  ~AttributionInfoBuilder();

  AttributionInfoBuilder& SetTime(base::Time time);

  AttributionInfoBuilder& SetDebugKey(absl::optional<uint64_t> debug_key);

  AttributionInfo Build() const;

 private:
  StoredSource source_;
  base::Time time_;
  absl::optional<uint64_t> debug_key_;
};

// Helper class to construct an `AttributionReport` for tests using default
// data.
class ReportBuilder {
 public:
  explicit ReportBuilder(AttributionInfo attribution_info);
  ~ReportBuilder();

  ReportBuilder& SetTriggerData(uint64_t trigger_data);

  ReportBuilder& SetReportTime(base::Time time);

  ReportBuilder& SetPriority(int64_t priority);

  ReportBuilder& SetExternalReportId(base::GUID external_report_id);

  ReportBuilder& SetRandomizedTriggerRate(double rate);

  ReportBuilder& SetReportId(AttributionReport::EventLevelData::Id id);

  ReportBuilder& SetReportId(
      AttributionReport::AggregatableAttributionData::Id id);

  ReportBuilder& SetAggregatableHistogramContributions(
      std::vector<AggregatableHistogramContribution> contributions);

  ReportBuilder& SetAggregationCoordinator(
      ::aggregation_service::mojom::AggregationCoordinator);

  AttributionReport Build() const;

  AttributionReport BuildAggregatableAttribution() const;

 private:
  AttributionInfo attribution_info_;
  uint64_t trigger_data_ = 0;
  base::Time report_time_;
  int64_t priority_ = 0;
  base::GUID external_report_id_;
  double randomized_trigger_rate_ = 0;
  AttributionReport::EventLevelData::Id report_id_{0};
  AttributionReport::AggregatableAttributionData::Id
      aggregatable_attribution_report_id_{0};
  std::vector<AggregatableHistogramContribution> contributions_;
  ::aggregation_service::mojom::AggregationCoordinator
      aggregation_coordinator_ =
          ::aggregation_service::mojom::AggregationCoordinator::kDefault;
};

bool operator==(const AttributionTrigger& a, const AttributionTrigger& b);

bool operator==(const CommonSourceInfo& a, const CommonSourceInfo& b);

bool operator==(const AttributionInfo& a, const AttributionInfo& b);

bool operator==(const AttributionStorageDelegate::FakeReport& a,
                const AttributionStorageDelegate::FakeReport& b);

bool operator<(const AttributionStorageDelegate::FakeReport& a,
               const AttributionStorageDelegate::FakeReport& b);

bool operator==(const StorableSource& a, const StorableSource& b);

bool operator==(const StoredSource& a, const StoredSource& b);

bool operator==(const AggregatableHistogramContribution& a,
                const AggregatableHistogramContribution& b);

bool operator==(const AttributionReport::EventLevelData& a,
                const AttributionReport::EventLevelData& b);

bool operator==(const AttributionReport::AggregatableAttributionData& a,
                const AttributionReport::AggregatableAttributionData& b);

bool operator==(const AttributionReport& a, const AttributionReport& b);

bool operator==(const SendResult& a, const SendResult& b);

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::EventLevelResult status);

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::AggregatableResult status);

std::ostream& operator<<(std::ostream& out, RateLimitResult result);

std::ostream& operator<<(std::ostream& out,
                         const AttributionTrigger& conversion);

std::ostream& operator<<(std::ostream& out, const CommonSourceInfo& source);

std::ostream& operator<<(std::ostream& out,
                         const AttributionInfo& attribution_info);

std::ostream& operator<<(std::ostream& out,
                         const AttributionStorageDelegate::FakeReport&);

std::ostream& operator<<(std::ostream& out, const StorableSource& source);

std::ostream& operator<<(std::ostream& out, const StoredSource& source);

std::ostream& operator<<(std::ostream& out,
                         const AggregatableHistogramContribution& contribution);

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::EventLevelData& data);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionReport::AggregatableAttributionData& data);

std::ostream& operator<<(std::ostream& out, const AttributionReport& report);

std::ostream& operator<<(std::ostream& out,
                         AttributionReport::Type report_type);

std::ostream& operator<<(std::ostream& out, SendResult::Status status);

std::ostream& operator<<(std::ostream& out, const SendResult& info);

std::ostream& operator<<(std::ostream& out,
                         StoredSource::AttributionLogic attribution_logic);

std::ostream& operator<<(std::ostream& out,
                         StoredSource::ActiveState active_state);

std::ostream& operator<<(std::ostream& out, StorableSource::Result status);

std::vector<AttributionReport> GetAttributionReportsForTesting(
    AttributionManager* manager);

std::unique_ptr<MockDataHost> GetRegisteredDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host);

// Source matchers

MATCHER_P(CommonSourceInfoIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info(), result_listener);
}

MATCHER_P(SourceEventIdIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().source_event_id(),
                            result_listener);
}

MATCHER_P(ImpressionOriginIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().source_origin(),
                            result_listener);
}

MATCHER_P(DestinationOriginIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().destination_origin(),
                            result_listener);
}

MATCHER_P(ReportingOriginIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().reporting_origin(),
                            result_listener);
}

MATCHER_P(SourceTypeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().source_type(),
                            result_listener);
}

MATCHER_P(SourcePriorityIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().priority(),
                            result_listener);
}

MATCHER_P(SourceDebugKeyIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().debug_key(),
                            result_listener);
}

MATCHER_P(SourceFilterDataIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().filter_data(),
                            result_listener);
}

MATCHER_P(DedupKeysAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.dedup_keys(), result_listener);
}

MATCHER_P(AggregatableDedupKeysAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.aggregatable_dedup_keys(),
                            result_listener);
}

MATCHER_P(AggregationKeysAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().aggregation_keys(),
                            result_listener);
}

MATCHER_P(AggregatableBudgetConsumedIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.aggregatable_budget_consumed(),
                            result_listener);
}

MATCHER_P(SourceActiveStateIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.active_state(), result_listener);
}

// Trigger matchers.

MATCHER_P(TriggerDestinationOriginIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.destination_origin(), result_listener);
}

// Report matchers

MATCHER_P(ReportSourceIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.attribution_info().source,
                            result_listener);
}

MATCHER_P(ReportTimeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.report_time(), result_listener);
}

MATCHER_P(FailedSendAttemptsIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.failed_send_attempts(),
                            result_listener);
}

MATCHER_P(TriggerDebugKeyIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.attribution_info().debug_key,
                            result_listener);
}

MATCHER_P(EventLevelDataIs, matcher, "") {
  return ExplainMatchResult(
      ::testing::VariantWith<AttributionReport::EventLevelData>(matcher),
      arg.data(), result_listener);
}

MATCHER_P(TriggerDataIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.trigger_data, result_listener);
}

MATCHER_P(TriggerPriorityIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.priority, result_listener);
}

MATCHER_P(RandomizedTriggerRateIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.randomized_trigger_rate,
                            result_listener);
}

MATCHER_P(ReportURLIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.ReportURL(), result_listener);
}

MATCHER_P(ReportTypeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.GetReportType(), result_listener);
}

MATCHER_P(AggregatableAttributionDataIs, matcher, "") {
  return ExplainMatchResult(
      ::testing::VariantWith<AttributionReport::AggregatableAttributionData>(
          matcher),
      arg.data(), result_listener);
}

MATCHER_P(AggregatableHistogramContributionsAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.contributions, result_listener);
}

MATCHER_P(InitialReportTimeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.initial_report_time, result_listener);
}

MATCHER_P(AggregationCoordinatorIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.aggregation_coordinator,
                            result_listener);
}

// `CreateReportResult` matchers

MATCHER_P(CreateReportEventLevelStatusIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.event_level_status(), result_listener);
}

MATCHER_P(CreateReportAggregatableStatusIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.aggregatable_status(),
                            result_listener);
}

MATCHER_P(ReplacedEventLevelReportIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.replaced_event_level_report(),
                            result_listener);
}

MATCHER_P(DeactivatedSourceIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.GetDeactivatedSource(),
                            result_listener);
}

MATCHER_P(NewEventLevelReportIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.new_event_level_report(),
                            result_listener);
}

MATCHER_P(NewAggregatableReportIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.new_aggregatable_report(),
                            result_listener);
}

MATCHER_P(DroppedEventLevelReportIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.dropped_event_level_report(),
                            result_listener);
}

struct EventTriggerDataMatcherConfig {
  ::testing::Matcher<uint64_t> data;
  ::testing::Matcher<int64_t> priority;
  ::testing::Matcher<absl::optional<uint64_t>> dedup_key;
  ::testing::Matcher<const attribution_reporting::Filters&> filters;
  ::testing::Matcher<const attribution_reporting::Filters&> not_filters;

  EventTriggerDataMatcherConfig() = delete;
  explicit EventTriggerDataMatcherConfig(
      ::testing::Matcher<uint64_t> data = ::testing::_,
      ::testing::Matcher<int64_t> priority = ::testing::_,
      ::testing::Matcher<absl::optional<uint64_t>> dedup_key = ::testing::_,
      ::testing::Matcher<const attribution_reporting::Filters&> filters =
          ::testing::_,
      ::testing::Matcher<const attribution_reporting::Filters&> not_filters =
          ::testing::_);
  ~EventTriggerDataMatcherConfig();
};

::testing::Matcher<const attribution_reporting::EventTriggerData&>
EventTriggerDataMatches(const EventTriggerDataMatcherConfig&);

template <typename T>
struct BoundedListMatcherConfig {
  ::testing::Matcher<const std::vector<T>&> vec = ::testing::_;

  BoundedListMatcherConfig() = delete;
  explicit BoundedListMatcherConfig(
      ::testing::Matcher<const std::vector<T>&> vec = ::testing::_)
      : vec(std::move(vec)) {}

  ~BoundedListMatcherConfig() = default;
};

template <typename T, size_t kMaxSize>
::testing::Matcher<const attribution_reporting::BoundedList<T, kMaxSize>&>
BoundedListMatches(const BoundedListMatcherConfig<T>& cfg) {
  return Property("vec", &attribution_reporting::BoundedList<T, kMaxSize>::vec,
                  cfg.vec);
}

using EventTriggerDataListMatcherConfig =
    BoundedListMatcherConfig<attribution_reporting::EventTriggerData>;

constexpr auto EventTriggerDataListMatches =
    BoundedListMatches<attribution_reporting::EventTriggerData,
                       attribution_reporting::kMaxEventTriggerData>;

struct TriggerRegistrationMatcherConfig {
  ::testing::Matcher<const attribution_reporting::Filters&> filters =
      ::testing::_;
  ::testing::Matcher<const attribution_reporting::Filters&> not_filters =
      ::testing::_;
  ::testing::Matcher<absl::optional<uint64_t>> debug_key = ::testing::_;
  ::testing::Matcher<const attribution_reporting::EventTriggerDataList&>
      event_triggers = ::testing::_;
  ::testing::Matcher<absl::optional<uint64_t>> aggregatable_dedup_key =
      ::testing::_;
  ::testing::Matcher<bool> debug_reporting = ::testing::_;
  ::testing::Matcher<const attribution_reporting::AggregatableTriggerDataList&>
      aggregatable_trigger_data = ::testing::_;
  ::testing::Matcher<const attribution_reporting::AggregatableValues&>
      aggregatable_values = ::testing::_;
  ::testing::Matcher<::aggregation_service::mojom::AggregationCoordinator>
      aggregation_coordinator = ::testing::_;

  TriggerRegistrationMatcherConfig() = delete;
  explicit TriggerRegistrationMatcherConfig(
      ::testing::Matcher<const attribution_reporting::Filters&> filters =
          ::testing::_,
      ::testing::Matcher<const attribution_reporting::Filters&> not_filters =
          ::testing::_,
      ::testing::Matcher<absl::optional<uint64_t>> debug_key = ::testing::_,
      ::testing::Matcher<const attribution_reporting::EventTriggerDataList&>
          event_triggers = ::testing::_,
      ::testing::Matcher<absl::optional<uint64_t>> aggregatable_dedup_key =
          ::testing::_,
      ::testing::Matcher<bool> debug_reporting = ::testing::_,
      ::testing::Matcher<
          const attribution_reporting::AggregatableTriggerDataList&>
          aggregatable_trigger_data = ::testing::_,
      ::testing::Matcher<const attribution_reporting::AggregatableValues&>
          aggregatable_values = ::testing::_,
      ::testing::Matcher<::aggregation_service::mojom::AggregationCoordinator>
          aggregation_coordinator = ::testing::_);
  ~TriggerRegistrationMatcherConfig();
};

::testing::Matcher<const attribution_reporting::TriggerRegistration&>
TriggerRegistrationMatches(const TriggerRegistrationMatcherConfig&);

struct AttributionTriggerMatcherConfig {
  ::testing::Matcher<const attribution_reporting::SuitableOrigin&>
      reporting_origin = ::testing::_;
  ::testing::Matcher<const attribution_reporting::TriggerRegistration&>
      registration = ::testing::_;
  ::testing::Matcher<const attribution_reporting::SuitableOrigin&>
      destination_origin = ::testing::_;

  ::testing::Matcher<bool> is_within_fenced_frame = ::testing::_;

  AttributionTriggerMatcherConfig() = delete;
  explicit AttributionTriggerMatcherConfig(
      ::testing::Matcher<const attribution_reporting::SuitableOrigin&>
          reporting_origin = ::testing::_,
      ::testing::Matcher<const attribution_reporting::TriggerRegistration&>
          registration = ::testing::_,
      ::testing::Matcher<const attribution_reporting::SuitableOrigin&>
          destination_origin = ::testing::_,
      ::testing::Matcher<bool> is_within_fenced_frame = ::testing::_);
  ~AttributionTriggerMatcherConfig();
};

::testing::Matcher<AttributionTrigger> AttributionTriggerMatches(
    const AttributionTriggerMatcherConfig&);

class TestAggregatableSourceProvider {
 public:
  explicit TestAggregatableSourceProvider(size_t size = 1);
  ~TestAggregatableSourceProvider();

  SourceBuilder GetBuilder(base::Time source_time = base::Time::Now()) const;

 private:
  attribution_reporting::AggregationKeys source_;
};

TriggerBuilder DefaultAggregatableTriggerBuilder(
    const std::vector<uint32_t>& histogram_values = {1});

std::vector<AggregatableHistogramContribution>
DefaultAggregatableHistogramContributions(
    const std::vector<uint32_t>& histogram_values = {1});

// Returns filters that match only the given source type.
attribution_reporting::Filters AttributionFiltersForSourceType(
    AttributionSourceType);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
