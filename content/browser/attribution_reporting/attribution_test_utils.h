// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_

#include <stdint.h>

#include <iosfwd>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_provider.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/attribution_reporting.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/origin.h"

namespace mojo {

template <typename Interface>
class PendingReceiver;

}  // namespace mojo

namespace content {

class AttributionManagerImpl;
class AttributionObserver;
class AttributionTrigger;

struct AttributionAggregatableKey;

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
              IsConversionMeasurementOperationAllowed,
              (content::BrowserContext * browser_context,
               ConversionMeasurementOperation operation,
               const url::Origin* impression_origin,
               const url::Origin* conversion_origin,
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
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host),
      (override));

  MOCK_METHOD(
      void,
      RegisterNavigationDataHost,
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
       const blink::AttributionSrcToken& attribution_src_token),
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

  const std::vector<blink::mojom::AttributionSourceDataPtr>& source_data()
      const {
    return source_data_;
  }

  const std::vector<blink::mojom::AttributionTriggerDataPtr>& trigger_data()
      const {
    return trigger_data_;
  }

 private:
  // blink::mojom::AttributionDataHost:
  void SourceDataAvailable(
      blink::mojom::AttributionSourceDataPtr data) override;
  void TriggerDataAvailable(
      blink::mojom::AttributionTriggerDataPtr data) override;

  size_t min_source_data_count_ = 0;
  std::vector<blink::mojom::AttributionSourceDataPtr> source_data_;

  size_t min_trigger_data_count_ = 0;
  std::vector<blink::mojom::AttributionTriggerDataPtr> trigger_data_;

  base::RunLoop wait_loop_;
  mojo::Receiver<blink::mojom::AttributionDataHost> receiver_{this};
};

class MockDataHostManager : public AttributionDataHostManager {
 public:
  MockDataHostManager();
  ~MockDataHostManager() override;

  MOCK_METHOD(
      void,
      RegisterDataHost,
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
       url::Origin context_origin),
      (override));

  MOCK_METHOD(
      void,
      RegisterNavigationDataHost,
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
       const blink::AttributionSrcToken& attribution_src_token),
      (override));

  MOCK_METHOD(void,
              NotifyNavigationForDataHost,
              (const blink::AttributionSrcToken& attribution_src_token,
               const url::Origin& source_origin,
               const url::Origin& destination_origin),
              (override));

  MOCK_METHOD(void,
              NotifyNavigationFailure,
              (const blink::AttributionSrcToken& attribution_src_token),
              (override));
};

base::GUID DefaultExternalReportID();

std::vector<base::GUID> DefaultExternalReportIDs(size_t size);

class ConfigurableStorageDelegate : public AttributionStorageDelegate {
 public:
  ConfigurableStorageDelegate();
  ~ConfigurableStorageDelegate() override;

  // AttributionStorageDelegate
  base::Time GetEventLevelReportTime(const CommonSourceInfo& source,
                                     base::Time trigger_time) const override;
  base::Time GetAggregatableReportTime(base::Time trigger_time) const override;
  int GetMaxAttributionsPerSource(
      AttributionSourceType source_type) const override;
  int GetMaxSourcesPerOrigin() const override;
  int GetMaxAttributionsPerOrigin() const override;
  RateLimitConfig GetRateLimits() const override;
  int GetMaxDestinationsPerSourceSiteReportingOrigin() const override;
  base::TimeDelta GetDeleteExpiredSourcesFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;
  base::GUID NewReportID() const override;
  absl::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const override;
  void ShuffleReports(std::vector<AttributionReport>& reports) override;
  double GetRandomizedResponseRate(AttributionSourceType) const override;
  RandomizedResponse GetRandomizedResponse(
      const CommonSourceInfo& source) override;
  int64_t GetAggregatableBudgetPerSource() const override;
  uint64_t SanitizeTriggerData(
      uint64_t trigger_data,
      AttributionSourceType source_type) const override;

  void set_max_attributions_per_source(int max);

  void set_max_sources_per_origin(int max);

  void set_max_attributions_per_origin(int max);

  void set_max_destinations_per_source_site_reporting_origin(int max);

  void set_aggregatable_budget_per_source(int64_t max);

  RateLimitConfig& rate_limits();

  void set_rate_limits(RateLimitConfig c);

  void set_delete_expired_sources_frequency(base::TimeDelta frequency);

  void set_delete_expired_rate_limits_frequency(base::TimeDelta frequency);

  void set_report_delay(base::TimeDelta report_delay);

  void set_offline_report_delay_config(
      absl::optional<OfflineReportDelayConfig> config);

  void set_reverse_reports_on_shuffle(bool reverse);

  // Note that these rates are *not* used to produce a randomized response; that
  // is controlled deterministically by `set_randomized_response()`.
  void set_randomized_response_rates(AttributionRandomizedResponseRates rates);

  void set_randomized_response(RandomizedResponse randomized_response);

  void set_trigger_data_cardinality(uint64_t navigation, uint64_t event);

  // Detaches the delegate from its current sequence in preparation for being
  // moved to storage, which runs on its own sequence.
  void DetachFromSequence();

 private:
  int max_attributions_per_source_ = INT_MAX;
  int max_sources_per_origin_ = INT_MAX;
  int max_attributions_per_origin_ = INT_MAX;
  int max_destinations_per_source_site_reporting_origin_ = INT_MAX;
  int64_t aggregatable_budget_per_source_ = std::numeric_limits<int64_t>::max();

  RateLimitConfig rate_limits_ = {
      .time_window = base::TimeDelta::Max(),
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = std::numeric_limits<int64_t>::max(),
  };

  base::TimeDelta delete_expired_sources_frequency_;
  base::TimeDelta delete_expired_rate_limits_frequency_;

  base::TimeDelta report_delay_;

  absl::optional<OfflineReportDelayConfig> offline_report_delay_config_;

  // If true, `ShuffleReports()` reverses the reports to allow testing the
  // proper call from `AttributionStorage::GetAttributionReports()`.
  bool reverse_reports_on_shuffle_ = false;

  AttributionRandomizedResponseRates randomized_response_rates_;
  RandomizedResponse randomized_response_ = absl::nullopt;

  absl::optional<uint64_t> navigation_trigger_data_cardinality_;
  absl::optional<uint64_t> event_trigger_data_cardinality_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Test manager provider which can be used to inject a fake
// `AttributionManager`.
class TestManagerProvider : public AttributionManagerProvider {
 public:
  explicit TestManagerProvider(AttributionManager* manager)
      : manager_(manager) {}
  ~TestManagerProvider() override = default;

  AttributionManager* GetManager(WebContents* web_contents) const override;

 private:
  raw_ptr<AttributionManager> manager_ = nullptr;
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
      (AttributionReport::ReportType report_type,
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
               base::RepeatingCallback<bool(const url::Origin&)> filter,
               base::OnceClosure done),
              (override));

  void AddObserver(AttributionObserver* observer) override;
  void RemoveObserver(AttributionObserver* observer) override;
  AttributionDataHostManager* GetDataHostManager() override;

  void NotifySourcesChanged();
  void NotifyReportsChanged(AttributionReport::ReportType report_type);
  void NotifySourceDeactivated(const DeactivatedSource& source);
  void NotifySourceHandled(const StorableSource& source,
                           StorableSource::Result result);
  void NotifyReportSent(const AttributionReport& report,
                        bool is_debug_report,
                        const SendResult& info);
  void NotifyTriggerHandled(const AttributionTrigger& trigger,
                            const CreateReportResult& result);

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

  MOCK_METHOD(void,
              OnReportsChanged,
              (AttributionReport::ReportType),
              (override));

  MOCK_METHOD(void,
              OnSourceHandled,
              (const StorableSource& source, StorableSource::Result result),
              (override));

  MOCK_METHOD(void,
              OnSourceDeactivated,
              (const DeactivatedSource& source),
              (override));

  MOCK_METHOD(void,
              OnReportSent,
              (const AttributionReport& report,
               bool is_debug_report,
               const SendResult& info),
              (override));

  MOCK_METHOD(void,
              OnTriggerHandled,
              (const AttributionTrigger& trigger,
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

  SourceBuilder& SetSourceEventId(uint64_t source_event_id);

  SourceBuilder& SetImpressionOrigin(url::Origin origin);

  SourceBuilder& SetConversionOrigin(url::Origin domain);

  SourceBuilder& SetReportingOrigin(url::Origin origin);

  SourceBuilder& SetSourceType(AttributionSourceType source_type);

  SourceBuilder& SetPriority(int64_t priority);

  SourceBuilder& SetAttributionLogic(
      StoredSource::AttributionLogic attribution_logic);

  SourceBuilder& SetFilterData(AttributionFilterData filter_data);

  // Sets the filter data to the autogenerated "source_type" filter.
  SourceBuilder& SetDefaultFilterData();

  SourceBuilder& SetActiveState(StoredSource::ActiveState active_state);

  SourceBuilder& SetDebugKey(absl::optional<uint64_t> debug_key);

  SourceBuilder& SetSourceId(StoredSource::Id source_id);

  SourceBuilder& SetDedupKeys(std::vector<uint64_t> dedup_keys);

  SourceBuilder& SetAggregatableSource(
      AttributionAggregatableSource aggregatable_source);

  StorableSource Build() const;

  StoredSource BuildStored() const;

  CommonSourceInfo BuildCommonInfo() const;

 private:
  uint64_t source_event_id_ = 123;
  base::Time impression_time_;
  base::TimeDelta expiry_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
  AttributionSourceType source_type_ = AttributionSourceType::kNavigation;
  int64_t priority_ = 0;
  StoredSource::AttributionLogic attribution_logic_ =
      StoredSource::AttributionLogic::kTruthfully;
  AttributionFilterData filter_data_;
  StoredSource::ActiveState active_state_ = StoredSource::ActiveState::kActive;
  absl::optional<uint64_t> debug_key_;
  // `base::StrongAlias` does not automatically initialize the value here.
  // Ensure that we don't use uninitialized memory.
  StoredSource::Id source_id_{0};
  std::vector<uint64_t> dedup_keys_;
  AttributionAggregatableSource aggregatable_source_;
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

  TriggerBuilder& SetDestinationOrigin(url::Origin destination_origin);

  TriggerBuilder& SetReportingOrigin(url::Origin reporting_origin);

  TriggerBuilder& SetPriority(int64_t priority);

  TriggerBuilder& SetDedupKey(absl::optional<uint64_t> dedup_key);

  TriggerBuilder& SetDebugKey(absl::optional<uint64_t> debug_key);

  TriggerBuilder& SetAggregatableTrigger(
      AttributionAggregatableTrigger aggregatable_trigger);

  AttributionTrigger Build() const;

 private:
  uint64_t trigger_data_ = 111;
  uint64_t event_source_trigger_data_ = 0;
  url::Origin destination_origin_;
  url::Origin reporting_origin_;
  int64_t priority_ = 0;
  absl::optional<uint64_t> dedup_key_;
  absl::optional<uint64_t> debug_key_;
  AttributionAggregatableTrigger aggregatable_trigger_;
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

  ReportBuilder& SetReportId(
      absl::optional<AttributionReport::EventLevelData::Id> id);

  ReportBuilder& SetReportId(
      absl::optional<AttributionReport::AggregatableAttributionData::Id> id);

  ReportBuilder& SetAggregatableHistogramContributions(
      std::vector<AggregatableHistogramContribution> contributions);

  AttributionReport Build() const;

  AttributionReport BuildAggregatableAttribution() const;

 private:
  AttributionInfo attribution_info_;
  uint64_t trigger_data_ = 0;
  base::Time report_time_;
  int64_t priority_ = 0;
  base::GUID external_report_id_;
  double randomized_trigger_rate_ = 0;
  absl::optional<AttributionReport::EventLevelData::Id> report_id_;
  absl::optional<AttributionReport::AggregatableAttributionData::Id>
      aggregatable_attribution_report_id_;
  std::vector<AggregatableHistogramContribution> contributions_;
};

// Helper class to construct a `proto::AttributionAggregatableKey` for testing.
class AggregatableKeyProtoBuilder {
 public:
  AggregatableKeyProtoBuilder();
  ~AggregatableKeyProtoBuilder();

  AggregatableKeyProtoBuilder& SetHighBits(uint64_t high_bits);

  AggregatableKeyProtoBuilder& SetLowBits(uint64_t low_bits);

  proto::AttributionAggregatableKey Build() const;

 private:
  proto::AttributionAggregatableKey key_;
};

// Helper class to construct a `proto::AttributionAggregatableSource` for
// testing.
class AggregatableSourceProtoBuilder {
 public:
  AggregatableSourceProtoBuilder();
  ~AggregatableSourceProtoBuilder();

  AggregatableSourceProtoBuilder& AddKey(std::string key_id,
                                         proto::AttributionAggregatableKey key);

  proto::AttributionAggregatableSource Build() const;

 private:
  proto::AttributionAggregatableSource aggregatable_source_;
};

// Helper class to construct a `blink::mojom::AttributionAggregatableSource`
// for testing.
class AggregatableSourceMojoBuilder {
 public:
  AggregatableSourceMojoBuilder();
  ~AggregatableSourceMojoBuilder();

  AggregatableSourceMojoBuilder& AddKey(
      std::string key_id,
      blink::mojom::AttributionAggregatableKeyPtr key);

  blink::mojom::AttributionAggregatableSourcePtr Build() const;

 private:
  blink::mojom::AttributionAggregatableSource aggregatable_source_;
};

bool operator==(const AttributionTrigger::EventTriggerData& a,
                const AttributionTrigger::EventTriggerData& b);

bool operator==(const AttributionTrigger& a, const AttributionTrigger& b);

bool operator==(const AttributionFilterData& a, const AttributionFilterData& b);

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

bool operator==(const DeactivatedSource& a, const DeactivatedSource& b);

bool operator==(const AttributionAggregatableKey& a,
                const AttributionAggregatableKey& b);

bool operator==(const AttributionAggregatableTriggerData& a,
                const AttributionAggregatableTriggerData& b);

bool operator==(const AttributionAggregatableTrigger& a,
                const AttributionAggregatableTrigger& b);

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::EventLevelResult status);

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::AggregatableResult status);

std::ostream& operator<<(std::ostream& out, DeactivatedSource::Reason reason);

std::ostream& operator<<(std::ostream& out, RateLimitResult result);

std::ostream& operator<<(std::ostream& out, AttributionSourceType source_type);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionTrigger::EventTriggerData& event_trigger);

std::ostream& operator<<(std::ostream& out,
                         const AttributionTrigger& conversion);

std::ostream& operator<<(std::ostream& out,
                         const AttributionFilterData& filter_data);

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
                         AttributionReport::ReportType report_type);

std::ostream& operator<<(std::ostream& out, SendResult::Status status);

std::ostream& operator<<(std::ostream& out, const SendResult& info);

std::ostream& operator<<(std::ostream& out,
                         StoredSource::AttributionLogic attribution_logic);

std::ostream& operator<<(std::ostream& out,
                         StoredSource::ActiveState active_state);

std::ostream& operator<<(std::ostream& out,
                         const DeactivatedSource& deactivated_source);

std::ostream& operator<<(std::ostream& out, StorableSource::Result status);

std::ostream& operator<<(std::ostream& out,
                         const AttributionAggregatableKey& key);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionAggregatableTriggerData& trigger_data);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionAggregatableTrigger& aggregatable_trigger);

bool operator==(const AttributionAggregatableSource& a,
                const AttributionAggregatableSource& b);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionAggregatableSource& aggregatable_source);

std::vector<AttributionReport> GetAttributionReportsForTesting(
    AttributionManagerImpl* manager,
    base::Time max_report_time);

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
  return ExplainMatchResult(matcher, arg.common_info().impression_origin(),
                            result_listener);
}

MATCHER_P(ConversionOriginIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().conversion_origin(),
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

MATCHER_P(ImpressionTimeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().impression_time(),
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

MATCHER_P(AggregatableSourceAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().aggregatable_source(),
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

MATCHER_P(NewReportsAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.new_reports(), result_listener);
}

struct EventTriggerDataMatcherConfig {
  ::testing::Matcher<uint64_t> data = ::testing::_;
  ::testing::Matcher<int64_t> priority = ::testing::_;
  ::testing::Matcher<absl::optional<uint64_t>> dedup_key = ::testing::_;
  ::testing::Matcher<const AttributionFilterData&> filters = ::testing::_;
  ::testing::Matcher<const AttributionFilterData&> not_filters = ::testing::_;

  EventTriggerDataMatcherConfig() = delete;
  ~EventTriggerDataMatcherConfig();
};

::testing::Matcher<const AttributionTrigger::EventTriggerData&>
EventTriggerDataMatches(const EventTriggerDataMatcherConfig&);

struct AttributionTriggerMatcherConfig {
  ::testing::Matcher<const url::Origin&> destination_origin = ::testing::_;
  ::testing::Matcher<const url::Origin&> reporting_origin = ::testing::_;
  ::testing::Matcher<const AttributionFilterData&> filters = ::testing::_;
  ::testing::Matcher<absl::optional<uint64_t>> debug_key = ::testing::_;
  ::testing::Matcher<const std::vector<AttributionTrigger::EventTriggerData>&>
      event_triggers = ::testing::_;

  AttributionTriggerMatcherConfig() = delete;
  ~AttributionTriggerMatcherConfig();
};

::testing::Matcher<AttributionTrigger> AttributionTriggerMatches(
    const AttributionTriggerMatcherConfig&);

struct AttributionFilterSizeTestCase {
  const char* description;
  bool valid;

  size_t filter_count;
  size_t filter_size;
  size_t value_count;
  size_t value_size;

  using Map = base::flat_map<std::string, std::vector<std::string>>;

  Map AsMap() const;
};

constexpr AttributionFilterSizeTestCase kAttributionFilterSizeTestCases[] = {
    {"empty", true, 0, 0, 0, 0},
    {"max_filters", true, blink::kMaxAttributionFiltersPerSource, 1, 0, 0},
    {"too_many_filters", false, blink::kMaxAttributionFiltersPerSource + 1, 1,
     0, 0},
    {"max_filter_size", true, 1, blink::kMaxBytesPerAttributionFilterString, 0,
     0},
    {"excessive_filter_size", false, 1,
     blink::kMaxBytesPerAttributionFilterString + 1, 0, 0},
    {"max_values", true, 1, 0, blink::kMaxValuesPerAttributionFilter, 0},
    {"too_many_values", false, 1, 0, blink::kMaxValuesPerAttributionFilter + 1,
     0},
    {"max_value_size", true, 1, 0, 1,
     blink::kMaxBytesPerAttributionFilterString},
    {"excessive_value_size", false, 1, 0, 1,
     blink::kMaxBytesPerAttributionFilterString + 1},
};

class TestAggregatableSourceProvider {
 public:
  explicit TestAggregatableSourceProvider(size_t size = 1);
  ~TestAggregatableSourceProvider();

  SourceBuilder GetBuilder(base::Time source_time = base::Time::Now()) const;

 private:
  AttributionAggregatableSource source_;
};

TriggerBuilder DefaultAggregatableTriggerBuilder(
    const std::vector<uint32_t>& histogram_values = {1});

std::vector<AggregatableHistogramContribution>
DefaultAggregatableHistogramContributions(
    const std::vector<uint32_t>& histogram_values = {1});

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
