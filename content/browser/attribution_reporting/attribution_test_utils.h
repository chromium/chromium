// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_

#include <stdint.h>

#include <iosfwd>
#include <vector>

#include "base/compiler_specific.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_policy.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/rate_limit_table.h"
#include "content/browser/attribution_reporting/sent_report.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/storable_trigger.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

class StorableTrigger;

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
  base::Time GetReportTime(const StorableSource& source,
                           base::Time trigger_time) const override;
  int GetMaxAttributionsPerSource(
      StorableSource::SourceType source_type) const override;
  int GetMaxSourcesPerOrigin() const override;
  int GetMaxAttributionsPerOrigin() const override;
  RateLimitConfig GetRateLimits(
      AttributionStorage::AttributionType attribution_type) const override;
  int GetMaxAttributionDestinationsPerEventSource() const override;
  uint64_t GetFakeEventSourceTriggerData() const override;
  base::TimeDelta GetDeleteExpiredSourcesFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;
  base::GUID NewReportID() const override;

  void set_max_attributions_per_source(int max) {
    max_attributions_per_source_ = max;
  }

  void set_max_sources_per_origin(int max) { max_sources_per_origin_ = max; }

  void set_max_attributions_per_origin(int max) {
    max_attributions_per_origin_ = max;
  }

  void set_max_attribution_destinations_per_event_source(int max) {
    max_attribution_destinations_per_event_source_ = max;
  }

  void set_rate_limits(RateLimitConfig c) { rate_limits_ = c; }

  void set_fake_event_source_trigger_data(uint64_t data) {
    fake_event_source_trigger_data_ = data;
  }

  void set_delete_expired_sources_frequency(base::TimeDelta frequency) {
    delete_expired_sources_frequency_ = frequency;
  }

  void set_delete_expired_rate_limits_frequency(base::TimeDelta frequency) {
    delete_expired_rate_limits_frequency_ = frequency;
  }

  void set_report_time_ms(int report_time_ms) {
    report_time_ms_ = report_time_ms;
  }

 private:
  int max_attributions_per_source_ = INT_MAX;
  int max_sources_per_origin_ = INT_MAX;
  int max_attributions_per_origin_ = INT_MAX;
  int max_attribution_destinations_per_event_source_ = INT_MAX;

  RateLimitConfig rate_limits_ = {
      .time_window = base::TimeDelta::Max(),
      .max_contributions_per_window = INT_MAX,
  };

  uint64_t fake_event_source_trigger_data_ = 0;

  base::TimeDelta delete_expired_sources_frequency_;
  base::TimeDelta delete_expired_rate_limits_frequency_;

  int report_time_ms_ = 0;
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

  MOCK_METHOD(void, HandleTrigger, (StorableTrigger trigger), (override));

  MOCK_METHOD(void,
              GetActiveSourcesForWebUI,
              (base::OnceCallback<void(std::vector<StorableSource>)> callback),
              (override));

  MOCK_METHOD(
      void,
      GetPendingReportsForWebUI,
      (base::OnceCallback<void(std::vector<AttributionReport>)> callback),
      (override));

  MOCK_METHOD(void, SendReportsForWebUI, (base::OnceClosure done), (override));

  MOCK_METHOD(void,
              ClearData,
              (base::Time delete_begin,
               base::Time delete_end,
               base::RepeatingCallback<bool(const url::Origin&)> filter,
               base::OnceClosure done),
              (override));

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  const AttributionPolicy& GetAttributionPolicy() const override;

  void NotifySourcesChanged();
  void NotifyReportsChanged();
  void NotifySourceDeactivated(
      const AttributionStorage::DeactivatedSource& source);
  void NotifyReportSent(const SentReport& info);
  void NotifyReportDropped(
      const AttributionStorage::CreateReportResult& result);

 private:
  AttributionPolicy policy_;
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

// Helper class to construct a StorableSource for tests using default data.
// StorableSource members are not mutable after construction requiring a
// builder pattern.
class SourceBuilder {
 public:
  explicit SourceBuilder(base::Time time);
  ~SourceBuilder();

  SourceBuilder& SetExpiry(base::TimeDelta delta) WARN_UNUSED_RESULT;

  SourceBuilder& SetSourceEventId(uint64_t source_event_id) WARN_UNUSED_RESULT;

  SourceBuilder& SetImpressionOrigin(url::Origin origin) WARN_UNUSED_RESULT;

  SourceBuilder& SetConversionOrigin(url::Origin domain) WARN_UNUSED_RESULT;

  SourceBuilder& SetReportingOrigin(url::Origin origin) WARN_UNUSED_RESULT;

  SourceBuilder& SetSourceType(StorableSource::SourceType source_type)
      WARN_UNUSED_RESULT;

  SourceBuilder& SetPriority(int64_t priority) WARN_UNUSED_RESULT;

  SourceBuilder& SetAttributionLogic(
      StorableSource::AttributionLogic attribution_logic) WARN_UNUSED_RESULT;

  SourceBuilder& SetImpressionId(
      absl::optional<StorableSource::Id> impression_id) WARN_UNUSED_RESULT;

  SourceBuilder& SetDedupKeys(std::vector<int64_t> dedup_keys)
      WARN_UNUSED_RESULT;

  StorableSource Build() const WARN_UNUSED_RESULT;

 private:
  uint64_t source_event_id_ = 123;
  base::Time impression_time_;
  base::TimeDelta expiry_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
  StorableSource::SourceType source_type_ =
      StorableSource::SourceType::kNavigation;
  int64_t priority_ = 0;
  StorableSource::AttributionLogic attribution_logic_ =
      StorableSource::AttributionLogic::kTruthfully;
  absl::optional<StorableSource::Id> impression_id_;
  std::vector<int64_t> dedup_keys_;
};

// Returns a StorableTrigger with default data which matches the default
// impressions created by SourceBuilder.
StorableTrigger DefaultTrigger() WARN_UNUSED_RESULT;

// Helper class to construct a StorableTrigger for tests using default data.
// StorableTrigger members are not mutable after construction requiring a
// builder pattern.
class TriggerBuilder {
 public:
  TriggerBuilder();
  ~TriggerBuilder();

  TriggerBuilder& SetTriggerData(uint64_t trigger_data) WARN_UNUSED_RESULT;

  TriggerBuilder& SetEventSourceTriggerData(uint64_t event_source_trigger_data)
      WARN_UNUSED_RESULT;

  TriggerBuilder& SetConversionDestination(
      net::SchemefulSite conversion_destination) WARN_UNUSED_RESULT;

  TriggerBuilder& SetReportingOrigin(url::Origin reporting_origin)
      WARN_UNUSED_RESULT;

  TriggerBuilder& SetPriority(int64_t priority) WARN_UNUSED_RESULT;

  TriggerBuilder& SetDedupKey(absl::optional<int64_t> dedup_key)
      WARN_UNUSED_RESULT;

  StorableTrigger Build() const WARN_UNUSED_RESULT;

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
  explicit ReportBuilder(StorableSource source);
  ~ReportBuilder();

  ReportBuilder& SetTriggerData(uint64_t trigger_data) WARN_UNUSED_RESULT;

  ReportBuilder& SetConversionTime(base::Time time) WARN_UNUSED_RESULT;

  ReportBuilder& SetReportTime(base::Time time) WARN_UNUSED_RESULT;

  ReportBuilder& SetPriority(int64_t priority) WARN_UNUSED_RESULT;

  ReportBuilder& SetExternalReportId(base::GUID external_report_id)
      WARN_UNUSED_RESULT;

  ReportBuilder& SetReportId(absl::optional<AttributionReport::Id> id)
      WARN_UNUSED_RESULT;

  AttributionReport Build() const WARN_UNUSED_RESULT;

 private:
  StorableSource source_;
  uint64_t trigger_data_ = 0;
  base::Time conversion_time_;
  base::Time report_time_;
  int64_t priority_ = 0;
  base::GUID external_report_id_;
  absl::optional<AttributionReport::Id> report_id_;
};

bool operator==(const StorableSource& a, const StorableSource& b);

bool operator==(const AttributionReport& a, const AttributionReport& b);

bool operator==(const SentReport& a, const SentReport& b);

bool operator==(const AttributionStorage::DeactivatedSource& a,
                const AttributionStorage::DeactivatedSource& b);

std::ostream& operator<<(std::ostream& out,
                         AttributionStorage::CreateReportResult::Status status);

std::ostream& operator<<(std::ostream& out,
                         AttributionStorage::DeactivatedSource::Reason reason);

std::ostream& operator<<(std::ostream& out,
                         RateLimitTable::AttributionAllowedStatus status);

std::ostream& operator<<(std::ostream& out,
                         StorableSource::SourceType source_type);

std::ostream& operator<<(std::ostream& out, const StorableTrigger& conversion);

std::ostream& operator<<(std::ostream& out, const StorableSource& impression);

std::ostream& operator<<(std::ostream& out, const AttributionReport& report);

std::ostream& operator<<(std::ostream& out, SentReport::Status status);

std::ostream& operator<<(std::ostream& out, const SentReport& info);

std::ostream& operator<<(std::ostream& out,
                         StorableSource::AttributionLogic attribution_logic);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionStorage::DeactivatedSource& deactivated_source);

std::vector<AttributionReport> GetAttributionsToReportForTesting(
    AttributionManagerImpl* manager,
    base::Time max_report_time) WARN_UNUSED_RESULT;

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
