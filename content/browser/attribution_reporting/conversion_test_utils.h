// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_TEST_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_TEST_UTILS_H_

#include <stdint.h>

#include <iosfwd>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_policy.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_session_storage.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/conversion_manager.h"
#include "content/browser/attribution_reporting/conversion_manager_impl.h"
#include "content/browser/attribution_reporting/rate_limit_table.h"
#include "content/browser/attribution_reporting/sent_report_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

class StorableTrigger;

class ConversionDisallowingContentBrowserClient
    : public TestContentBrowserClient {
 public:
  ConversionDisallowingContentBrowserClient() = default;
  ~ConversionDisallowingContentBrowserClient() override = default;

  // ContentBrowserClient:
  bool IsConversionMeasurementOperationAllowed(
      content::BrowserContext* browser_context,
      ConversionMeasurementOperation operation,
      const url::Origin* impression_origin,
      const url::Origin* conversion_origin,
      const url::Origin* reporting_origin) override;
};

// Configurable browser client capable of blocking conversion operations in a
// single embedded context.
class ConfigurableConversionTestBrowserClient
    : public TestContentBrowserClient {
 public:
  ConfigurableConversionTestBrowserClient();
  ~ConfigurableConversionTestBrowserClient() override;

  // ContentBrowserClient:
  bool IsConversionMeasurementOperationAllowed(
      content::BrowserContext* browser_context,
      ConversionMeasurementOperation operation,
      const url::Origin* impression_origin,
      const url::Origin* conversion_origin,
      const url::Origin* reporting_origin) override;

  // Sets the origins where conversion measurement is blocked. This only blocks
  // an operation if all origins match in
  // `AllowConversionMeasurementOperation()`.
  void BlockConversionMeasurementInContext(
      absl::optional<url::Origin> impression_origin,
      absl::optional<url::Origin> conversion_origin,
      absl::optional<url::Origin> reporting_origin);

 private:
  absl::optional<url::Origin> blocked_impression_origin_;
  absl::optional<url::Origin> blocked_conversion_origin_;
  absl::optional<url::Origin> blocked_reporting_origin_;
};

class ConfigurableStorageDelegate : public AttributionStorage::Delegate {
 public:
  ConfigurableStorageDelegate();
  ~ConfigurableStorageDelegate() override;

  // AttributionStorage::Delegate
  base::Time GetReportTime(const StorableSource& impression,
                           base::Time conversion_time) const override;
  int GetMaxConversionsPerImpression(
      StorableSource::SourceType source_type) const override;
  int GetMaxImpressionsPerOrigin() const override;
  int GetMaxConversionsPerOrigin() const override;
  RateLimitConfig GetRateLimits(
      AttributionStorage::AttributionType attribution_type) const override;
  int GetMaxAttributionDestinationsPerEventSource() const override;
  uint64_t GetFakeEventSourceTriggerData() const override;
  base::TimeDelta GetDeleteExpiredImpressionsFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;

  void set_max_conversions_per_impression(int max) {
    max_conversions_per_impression_ = max;
  }

  void set_max_impressions_per_origin(int max) {
    max_impressions_per_origin_ = max;
  }

  void set_max_conversions_per_origin(int max) {
    max_conversions_per_origin_ = max;
  }

  void set_max_attribution_destinations_per_event_source(int max) {
    max_attribution_destinations_per_event_source_ = max;
  }

  void set_rate_limits(RateLimitConfig c) { rate_limits_ = c; }

  void set_fake_event_source_trigger_data(uint64_t data) {
    fake_event_source_trigger_data_ = data;
  }

  void set_delete_expired_impressions_frequency(base::TimeDelta frequency) {
    delete_expired_impressions_frequency_ = frequency;
  }

  void set_delete_expired_rate_limits_frequency(base::TimeDelta frequency) {
    delete_expired_rate_limits_frequency_ = frequency;
  }

  void set_report_time_ms(int report_time_ms) {
    report_time_ms_ = report_time_ms;
  }

 private:
  int max_conversions_per_impression_ = INT_MAX;
  int max_impressions_per_origin_ = INT_MAX;
  int max_conversions_per_origin_ = INT_MAX;
  int max_attribution_destinations_per_event_source_ = INT_MAX;

  RateLimitConfig rate_limits_ = {
      .time_window = base::TimeDelta::Max(),
      .max_contributions_per_window = INT_MAX,
  };

  uint64_t fake_event_source_trigger_data_ = 0;

  base::TimeDelta delete_expired_impressions_frequency_;
  base::TimeDelta delete_expired_rate_limits_frequency_;

  int report_time_ms_ = 0;
};

// Test manager provider which can be used to inject a fake ConversionManager.
class TestManagerProvider : public ConversionManager::Provider {
 public:
  explicit TestManagerProvider(ConversionManager* manager)
      : manager_(manager) {}
  ~TestManagerProvider() override = default;

  ConversionManager* GetManager(WebContents* web_contents) const override;

 private:
  ConversionManager* manager_ = nullptr;
};

// Test ConversionManager which can be injected into tests to monitor calls to a
// ConversionManager instance.
class TestConversionManager : public ConversionManager {
 public:
  TestConversionManager();
  ~TestConversionManager() override;

  // ConversionManager:
  void HandleImpression(StorableSource impression) override;
  void HandleConversion(StorableTrigger conversion) override;
  void GetActiveImpressionsForWebUI(
      base::OnceCallback<void(std::vector<StorableSource>)> callback) override;
  void GetPendingReportsForWebUI(
      base::OnceCallback<void(std::vector<AttributionReport>)> callback,
      base::Time max_report_time) override;
  const AttributionSessionStorage& GetSessionStorage() const override;
  void SendReportsForWebUI(base::OnceClosure done) override;
  const AttributionPolicy& GetAttributionPolicy() const override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 base::RepeatingCallback<bool(const url::Origin&)> filter,
                 base::OnceClosure done) override;

  void SetActiveImpressionsForWebUI(std::vector<StorableSource> impressions);
  void SetReportsForWebUI(std::vector<AttributionReport> reports);
  AttributionSessionStorage& GetSessionStorage();

  // Resets all counters on this.
  void Reset();

  size_t num_impressions() const { return num_impressions_; }
  size_t num_conversions() const { return num_conversions_; }

  const net::SchemefulSite& last_conversion_destination() {
    return last_conversion_destination_;
  }

  const absl::optional<StorableSource::SourceType>&
  last_impression_source_type() {
    return last_impression_source_type_;
  }

  const absl::optional<url::Origin>& last_impression_origin() {
    return last_impression_origin_;
  }

  const absl::optional<int64_t>& last_attribution_source_priority() {
    return last_attribution_source_priority_;
  }

 private:
  AttributionPolicy policy_;
  AttributionSessionStorage session_storage_{INT_MAX};
  net::SchemefulSite last_conversion_destination_;
  absl::optional<StorableSource::SourceType> last_impression_source_type_;
  absl::optional<url::Origin> last_impression_origin_;
  absl::optional<int64_t> last_attribution_source_priority_;
  size_t num_impressions_ = 0;
  size_t num_conversions_ = 0;

  std::vector<StorableSource> impressions_;
  std::vector<AttributionReport> reports_;
};

// Helper class to construct a StorableSource for tests using default data.
// StorableSource members are not mutable after construction requiring a
// builder pattern.
class ImpressionBuilder {
 public:
  explicit ImpressionBuilder(base::Time time);
  ~ImpressionBuilder();

  ImpressionBuilder& SetExpiry(base::TimeDelta delta) WARN_UNUSED_RESULT;

  ImpressionBuilder& SetData(uint64_t data) WARN_UNUSED_RESULT;

  ImpressionBuilder& SetImpressionOrigin(url::Origin origin) WARN_UNUSED_RESULT;

  ImpressionBuilder& SetConversionOrigin(url::Origin domain) WARN_UNUSED_RESULT;

  ImpressionBuilder& SetReportingOrigin(url::Origin origin) WARN_UNUSED_RESULT;

  ImpressionBuilder& SetSourceType(StorableSource::SourceType source_type)
      WARN_UNUSED_RESULT;

  ImpressionBuilder& SetPriority(int64_t priority) WARN_UNUSED_RESULT;

  ImpressionBuilder& SetAttributionLogic(
      StorableSource::AttributionLogic attribution_logic) WARN_UNUSED_RESULT;

  ImpressionBuilder& SetImpressionId(
      absl::optional<StorableSource::Id> impression_id) WARN_UNUSED_RESULT;

  ImpressionBuilder& SetDedupKeys(std::vector<int64_t> dedup_keys)
      WARN_UNUSED_RESULT;

  StorableSource Build() const WARN_UNUSED_RESULT;

 private:
  uint64_t impression_data_;
  base::Time impression_time_;
  base::TimeDelta expiry_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
  StorableSource::SourceType source_type_;
  int64_t priority_;
  StorableSource::AttributionLogic attribution_logic_;
  absl::optional<StorableSource::Id> impression_id_;
  std::vector<int64_t> dedup_keys_;
};

// Returns a StorableTrigger with default data which matches the default
// impressions created by ImpressionBuilder.
StorableTrigger DefaultConversion() WARN_UNUSED_RESULT;

// Helper class to construct a StorableTrigger for tests using default data.
// StorableTrigger members are not mutable after construction requiring a
// builder pattern.
class TriggerBuilder {
 public:
  TriggerBuilder();
  ~TriggerBuilder();

  TriggerBuilder& SetConversionData(uint64_t conversion_data)
      WARN_UNUSED_RESULT;

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
  uint64_t conversion_data_ = 111;
  uint64_t event_source_trigger_data_ = 0;
  net::SchemefulSite conversion_destination_;
  url::Origin reporting_origin_;
  int64_t priority_ = 0;
  absl::optional<int64_t> dedup_key_ = absl::nullopt;
};

bool operator==(const StorableSource& a, const StorableSource& b);

bool operator==(const AttributionReport& a, const AttributionReport& b);

bool operator==(const SentReportInfo& a, const SentReportInfo& b);

std::ostream& operator<<(std::ostream& out,
                         AttributionStorage::CreateReportResult::Status status);

std::ostream& operator<<(std::ostream& out,
                         RateLimitTable::AttributionAllowedStatus status);

std::ostream& operator<<(std::ostream& out,
                         StorableSource::SourceType source_type);

std::ostream& operator<<(std::ostream& out, const StorableTrigger& conversion);

std::ostream& operator<<(std::ostream& out, const StorableSource& impression);

std::ostream& operator<<(std::ostream& out, const AttributionReport& report);

std::ostream& operator<<(std::ostream& out, SentReportInfo::Status status);

std::ostream& operator<<(std::ostream& out, const SentReportInfo& info);

std::ostream& operator<<(std::ostream& out,
                         StorableSource::AttributionLogic attribution_logic);

std::vector<AttributionReport> GetConversionsToReportForTesting(
    ConversionManagerImpl* manager,
    base::Time max_report_time) WARN_UNUSED_RESULT;

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_TEST_UTILS_H_
