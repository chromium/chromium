// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_

#include <stdint.h>

#include <iosfwd>
#include <vector>

#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/rate_limit_table.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

class HistogramContribution;
class AttributionTrigger;

struct AggregatableAttribution;

const CommonSourceInfo::SourceType kSourceTypes[] = {
    CommonSourceInfo::SourceType::kNavigation,
    CommonSourceInfo::SourceType::kEvent,
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
  explicit MockAttributionHost(WebContents* contents);

  ~MockAttributionHost() override;

  MOCK_METHOD(void,
              RegisterImpression,
              (const blink::Impression& impression),
              (override));

  MOCK_METHOD(void,
              RegisterConversion,
              (blink::mojom::ConversionPtr conversion),
              (override));
};

base::GUID DefaultExternalReportID();

class ConfigurableStorageDelegate : public AttributionStorage::Delegate {
 public:
  ConfigurableStorageDelegate();
  ~ConfigurableStorageDelegate() override;

  // AttributionStorage::Delegate
  base::Time GetReportTime(const CommonSourceInfo& source,
                           base::Time trigger_time) const override;
  int GetMaxAttributionsPerSource(
      CommonSourceInfo::SourceType source_type) const override;
  int GetMaxSourcesPerOrigin() const override;
  int GetMaxAttributionsPerOrigin() const override;
  RateLimitConfig GetRateLimits(
      AttributionStorage::AttributionType attribution_type) const override;
  int GetMaxDestinationsPerSourceSiteReportingOrigin() const override;
  base::TimeDelta GetDeleteExpiredSourcesFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;
  base::GUID NewReportID() const override;
  absl::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const override;
  void ShuffleReports(std::vector<AttributionReport>& reports) const override;
  RandomizedResponse GetRandomizedResponse(
      const CommonSourceInfo& source) const override;

  void set_max_attributions_per_source(int max) {
    max_attributions_per_source_ = max;
  }

  void set_max_sources_per_origin(int max) { max_sources_per_origin_ = max; }

  void set_max_attributions_per_origin(int max) {
    max_attributions_per_origin_ = max;
  }

  void set_max_destinations_per_source_site_reporting_origin(int max) {
    max_destinations_per_source_site_reporting_origin_ = max;
  }

  void set_rate_limits(RateLimitConfig c) { rate_limits_ = c; }

  void set_delete_expired_sources_frequency(base::TimeDelta frequency) {
    delete_expired_sources_frequency_ = frequency;
  }

  void set_delete_expired_rate_limits_frequency(base::TimeDelta frequency) {
    delete_expired_rate_limits_frequency_ = frequency;
  }

  void set_report_time_ms(int report_time_ms) {
    report_time_ms_ = report_time_ms;
  }

  void set_offline_report_delay_config(
      absl::optional<OfflineReportDelayConfig> config) {
    offline_report_delay_config_ = config;
  }

  void set_reverse_reports_on_shuffle(bool reverse) {
    reverse_reports_on_shuffle_ = reverse;
  }

  void set_randomized_response(RandomizedResponse randomized_response) {
    randomized_response_ = std::move(randomized_response);
  }

 private:
  int max_attributions_per_source_ = INT_MAX;
  int max_sources_per_origin_ = INT_MAX;
  int max_attributions_per_origin_ = INT_MAX;
  int max_destinations_per_source_site_reporting_origin_ = INT_MAX;

  RateLimitConfig rate_limits_ = {
      .time_window = base::TimeDelta::Max(),
      .max_contributions_per_window = INT_MAX,
  };

  base::TimeDelta delete_expired_sources_frequency_;
  base::TimeDelta delete_expired_rate_limits_frequency_;

  int report_time_ms_ = 0;

  absl::optional<OfflineReportDelayConfig> offline_report_delay_config_;

  // If true, `ShuffleReports()` reverses the reports to allow testing the
  // proper call from `AttributionStorage::GetAttributionsToReport()`.
  bool reverse_reports_on_shuffle_ = false;

  RandomizedResponse randomized_response_ = absl::nullopt;
};

// Test manager provider which can be used to inject a fake AttributionManager.
class TestManagerProvider : public AttributionManager::Provider {
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
      (base::OnceCallback<void(std::vector<AttributionReport>)> callback),
      (override));

  MOCK_METHOD(void,
              SendReportsForWebUI,
              (const std::vector<AttributionReport::EventLevelData::Id>& ids,
               base::OnceClosure done),
              (override));

  MOCK_METHOD(void,
              ClearData,
              (base::Time delete_begin,
               base::Time delete_end,
               base::RepeatingCallback<bool(const url::Origin&)> filter,
               base::OnceClosure done),
              (override));

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void NotifySourcesChanged();
  void NotifyReportsChanged();
  void NotifySourceDeactivated(
      const AttributionStorage::DeactivatedSource& source);
  void NotifyReportSent(const AttributionReport& report,
                        const SendResult& info);
  void NotifyReportDropped(
      const AttributionStorage::CreateReportResult& result);

 private:
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

// Helper class to construct a StorableSource for tests using default data.
// StorableSource members are not mutable after construction requiring a
// builder pattern.
class SourceBuilder {
 public:
  explicit SourceBuilder(base::Time time = base::Time::Now());
  ~SourceBuilder();

  SourceBuilder& SetExpiry(base::TimeDelta delta);

  SourceBuilder& SetSourceEventId(uint64_t source_event_id);

  SourceBuilder& SetImpressionOrigin(url::Origin origin);

  SourceBuilder& SetConversionOrigin(url::Origin domain);

  SourceBuilder& SetReportingOrigin(url::Origin origin);

  SourceBuilder& SetSourceType(CommonSourceInfo::SourceType source_type);

  SourceBuilder& SetPriority(int64_t priority);

  SourceBuilder& SetAttributionLogic(
      StoredSource::AttributionLogic attribution_logic);

  SourceBuilder& SetSourceId(StoredSource::Id source_id);

  SourceBuilder& SetDedupKeys(std::vector<int64_t> dedup_keys);

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
  CommonSourceInfo::SourceType source_type_ =
      CommonSourceInfo::SourceType::kNavigation;
  int64_t priority_ = 0;
  StoredSource::AttributionLogic attribution_logic_ =
      StoredSource::AttributionLogic::kTruthfully;
  // `base::StrongAlias` does not automatically initialize the value here.
  // Ensure that we don't use uninitialized memory.
  StoredSource::Id source_id_{0};
  std::vector<int64_t> dedup_keys_;
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

  TriggerBuilder& SetTriggerData(uint64_t trigger_data);

  TriggerBuilder& SetEventSourceTriggerData(uint64_t event_source_trigger_data);

  TriggerBuilder& SetConversionDestination(
      net::SchemefulSite conversion_destination);

  TriggerBuilder& SetReportingOrigin(url::Origin reporting_origin);

  TriggerBuilder& SetPriority(int64_t priority);

  TriggerBuilder& SetDedupKey(absl::optional<int64_t> dedup_key);

  AttributionTrigger Build() const;

 private:
  uint64_t trigger_data_ = 111;
  uint64_t event_source_trigger_data_ = 0;
  net::SchemefulSite conversion_destination_;
  url::Origin reporting_origin_;
  int64_t priority_ = 0;
  absl::optional<int64_t> dedup_key_ = absl::nullopt;
};

// Helper class to construct an `AttributionReport` for tests using default
// data.
class ReportBuilder {
 public:
  explicit ReportBuilder(StoredSource source);
  ~ReportBuilder();

  ReportBuilder& SetTriggerData(uint64_t trigger_data);

  ReportBuilder& SetTriggerTime(base::Time time);

  ReportBuilder& SetReportTime(base::Time time);

  ReportBuilder& SetPriority(int64_t priority);

  ReportBuilder& SetExternalReportId(base::GUID external_report_id);

  ReportBuilder& SetReportId(
      absl::optional<AttributionReport::EventLevelData::Id> id);

  AttributionReport Build() const;

 private:
  StoredSource source_;
  uint64_t trigger_data_ = 0;
  base::Time trigger_time_;
  base::Time report_time_;
  int64_t priority_ = 0;
  base::GUID external_report_id_;
  absl::optional<AttributionReport::EventLevelData::Id> report_id_;
};

bool operator==(const AttributionTrigger& a, const AttributionTrigger& b);

bool operator==(const CommonSourceInfo& a, const CommonSourceInfo& b);

bool operator==(const AttributionStorage::Delegate::FakeReport& a,
                const AttributionStorage::Delegate::FakeReport& b);

bool operator<(const AttributionStorage::Delegate::FakeReport& a,
               const AttributionStorage::Delegate::FakeReport& b);

bool operator==(const StorableSource& a, const StorableSource& b);

bool operator==(const StoredSource& a, const StoredSource& b);

bool operator==(const HistogramContribution& a, const HistogramContribution& b);

bool operator==(const AggregatableAttribution& a, AggregatableAttribution& b);

bool operator==(const AttributionReport::EventLevelData& a,
                const AttributionReport::EventLevelData& b);

bool operator==(const AttributionReport::AggregatableContributionData& a,
                const AttributionReport::AggregatableContributionData& b);

bool operator==(const AttributionReport& a, const AttributionReport& b);

bool operator==(const SendResult& a, const SendResult& b);

bool operator==(const AttributionStorage::DeactivatedSource& a,
                const AttributionStorage::DeactivatedSource& b);

std::ostream& operator<<(std::ostream& out,
                         AttributionStorage::CreateReportResult::Status status);

std::ostream& operator<<(std::ostream& out,
                         AttributionStorage::DeactivatedSource::Reason reason);

std::ostream& operator<<(std::ostream& out,
                         RateLimitTable::AttributionAllowedStatus status);

std::ostream& operator<<(std::ostream& out,
                         CommonSourceInfo::SourceType source_type);

std::ostream& operator<<(std::ostream& out,
                         const AttributionTrigger& conversion);

std::ostream& operator<<(std::ostream& out, const CommonSourceInfo& source);

std::ostream& operator<<(std::ostream& out,
                         const AttributionStorage::Delegate::FakeReport&);

std::ostream& operator<<(std::ostream& out, const StorableSource& source);

std::ostream& operator<<(std::ostream& out, const StoredSource& source);

std::ostream& operator<<(std::ostream& out,
                         const HistogramContribution& contribution);

std::ostream& operator<<(
    std::ostream& out,
    const AggregatableAttribution& aggregatable_attribution);

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::EventLevelData& data);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionReport::AggregatableContributionData& data);

std::ostream& operator<<(std::ostream& out, const AttributionReport& report);

std::ostream& operator<<(std::ostream& out, SendResult::Status status);

std::ostream& operator<<(std::ostream& out, const SendResult& info);

std::ostream& operator<<(std::ostream& out,
                         StoredSource::AttributionLogic attribution_logic);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionStorage::DeactivatedSource& deactivated_source);

std::ostream& operator<<(std::ostream& out,
                         AttributionStorage::StoreSourceResult::Status status);

std::vector<AttributionReport> GetAttributionsToReportForTesting(
    AttributionManagerImpl* manager,
    base::Time max_report_time);

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

MATCHER_P(DedupKeysAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.dedup_keys(), result_listener);
}

// Trigger matchers.

MATCHER_P(TriggerConversionDestinationIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.conversion_destination(),
                            result_listener);
}

// Report matchers

MATCHER_P(ReportSourceIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.source(), result_listener);
}

MATCHER_P(ReportTimeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.report_time(), result_listener);
}

MATCHER_P(FailedSendAttemptsIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.failed_send_attempts(),
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

// `CreateReportResult` matchers

MATCHER_P(CreateReportStatusIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.status(), result_listener);
}

MATCHER_P(DroppedReportIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.dropped_report(), result_listener);
}

MATCHER_P(DeactivatedSourceIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.GetDeactivatedSource(),
                            result_listener);
}

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
