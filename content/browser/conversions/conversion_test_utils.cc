// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_test_utils.h"

#include <limits.h>
#include <algorithm>

#include <tuple>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task_runner_util.h"
#include "base/test/bind.h"
#include "content/browser/conversions/storable_conversion.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kDefaultImpressionOrigin[] = "https://impression.test/";
const char kDefaultConversionOrigin[] = "https://sub.conversion.test/";
const char kDefaultConversionDestination[] = "https://conversion.test/";
const char kDefaultReportOrigin[] = "https://report.test/";

// Default expiry time for impressions for testing.
const int64_t kExpiryTime = 30;

}  // namespace

bool ConversionDisallowingContentBrowserClient::
    IsConversionMeasurementOperationAllowed(
        content::BrowserContext* browser_context,
        ConversionMeasurementOperation operation,
        const url::Origin* impression_origin,
        const url::Origin* conversion_origin,
        const url::Origin* reporting_origin) {
  return false;
}

ConfigurableConversionTestBrowserClient::
    ConfigurableConversionTestBrowserClient() = default;
ConfigurableConversionTestBrowserClient::
    ~ConfigurableConversionTestBrowserClient() = default;

bool ConfigurableConversionTestBrowserClient::
    IsConversionMeasurementOperationAllowed(
        content::BrowserContext* browser_context,
        ConversionMeasurementOperation operation,
        const url::Origin* impression_origin,
        const url::Origin* conversion_origin,
        const url::Origin* reporting_origin) {
  if (!!blocked_impression_origin_ != !!impression_origin ||
      !!blocked_conversion_origin_ != !!conversion_origin ||
      !!blocked_reporting_origin_ != !!reporting_origin) {
    return true;
  }

  // Allow the operation if any rule doesn't match.
  if ((impression_origin &&
       *blocked_impression_origin_ != *impression_origin) ||
      (conversion_origin &&
       *blocked_conversion_origin_ != *conversion_origin) ||
      (reporting_origin && *blocked_reporting_origin_ != *reporting_origin)) {
    return true;
  }

  return false;
}

void ConfigurableConversionTestBrowserClient::
    BlockConversionMeasurementInContext(
        absl::optional<url::Origin> impression_origin,
        absl::optional<url::Origin> conversion_origin,
        absl::optional<url::Origin> reporting_origin) {
  blocked_impression_origin_ = impression_origin;
  blocked_conversion_origin_ = conversion_origin;
  blocked_reporting_origin_ = reporting_origin;
}

ConfigurableStorageDelegate::ConfigurableStorageDelegate() = default;
ConfigurableStorageDelegate::~ConfigurableStorageDelegate() = default;

base::Time ConfigurableStorageDelegate::GetReportTime(
    const ConversionReport& report) const {
  return report.impression.impression_time() +
         base::TimeDelta::FromMilliseconds(report_time_ms_);
}

int ConfigurableStorageDelegate::GetMaxConversionsPerImpression(
    StorableImpression::SourceType source_type) const {
  return max_conversions_per_impression_;
}

int ConfigurableStorageDelegate::GetMaxImpressionsPerOrigin() const {
  return max_impressions_per_origin_;
}

int ConfigurableStorageDelegate::GetMaxConversionsPerOrigin() const {
  return max_conversions_per_origin_;
}

int ConfigurableStorageDelegate::GetMaxAttributionDestinationsPerEventSource()
    const {
  return max_attribution_destinations_per_event_source_;
}

ConversionStorage::Delegate::RateLimitConfig
ConfigurableStorageDelegate::GetRateLimits() const {
  return rate_limits_;
}

StorableImpression::AttributionLogic
ConfigurableStorageDelegate::SelectAttributionLogic(
    const StorableImpression& impression) const {
  return attribution_logic_;
}

uint64_t ConfigurableStorageDelegate::GetFakeEventSourceTriggerData() const {
  return fake_event_source_trigger_data_;
}

base::TimeDelta
ConfigurableStorageDelegate::GetDeleteExpiredImpressionsFrequency() const {
  return delete_expired_impressions_frequency_;
}

base::TimeDelta
ConfigurableStorageDelegate::GetDeleteExpiredRateLimitsFrequency() const {
  return delete_expired_rate_limits_frequency_;
}

ConversionManager* TestManagerProvider::GetManager(
    WebContents* web_contents) const {
  return manager_;
}

TestConversionManager::TestConversionManager() = default;

TestConversionManager::~TestConversionManager() = default;

void TestConversionManager::HandleImpression(StorableImpression impression) {
  num_impressions_++;
  last_impression_source_type_ = impression.source_type();
  last_impression_origin_ = impression.impression_origin();
  last_attribution_source_priority_ = impression.priority();
}

void TestConversionManager::HandleConversion(StorableConversion conversion) {
  num_conversions_++;

  last_conversion_destination_ = conversion.conversion_destination();
}

void TestConversionManager::GetActiveImpressionsForWebUI(
    base::OnceCallback<void(std::vector<StorableImpression>)> callback) {
  std::move(callback).Run(impressions_);
}

void TestConversionManager::GetPendingReportsForWebUI(
    base::OnceCallback<void(std::vector<ConversionReport>)> callback,
    base::Time max_report_time) {
  std::move(callback).Run(reports_);
}

const base::circular_deque<SentReportInfo>&
TestConversionManager::GetSentReportsForWebUI() const {
  return sent_reports_;
}

void TestConversionManager::SendReportsForWebUI(base::OnceClosure done) {
  reports_.clear();
  std::move(done).Run();
}

const ConversionPolicy& TestConversionManager::GetConversionPolicy() const {
  return policy_;
}

void TestConversionManager::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter,
    base::OnceClosure done) {
  impressions_.clear();
  reports_.clear();
  std::move(done).Run();
}

void TestConversionManager::SetActiveImpressionsForWebUI(
    std::vector<StorableImpression> impressions) {
  impressions_ = std::move(impressions);
}

void TestConversionManager::SetReportsForWebUI(
    std::vector<ConversionReport> reports) {
  reports_ = std::move(reports);
}

void TestConversionManager::SetSentReportsForWebUI(
    base::circular_deque<SentReportInfo> reports) {
  sent_reports_ = std::move(reports);
}

void TestConversionManager::Reset() {
  num_impressions_ = 0u;
  num_conversions_ = 0u;
}

// Builds an impression with default values. This is done as a builder because
// all values needed to be provided at construction time.
ImpressionBuilder::ImpressionBuilder(base::Time time)
    : impression_data_(123),
      impression_time_(time),
      expiry_(base::TimeDelta::FromMilliseconds(kExpiryTime)),
      impression_origin_(url::Origin::Create(GURL(kDefaultImpressionOrigin))),
      conversion_origin_(url::Origin::Create(GURL(kDefaultConversionOrigin))),
      reporting_origin_(url::Origin::Create(GURL(kDefaultReportOrigin))),
      source_type_(StorableImpression::SourceType::kNavigation),
      priority_(0) {}

ImpressionBuilder::~ImpressionBuilder() = default;

ImpressionBuilder& ImpressionBuilder::SetExpiry(base::TimeDelta delta) {
  expiry_ = delta;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetData(uint64_t data) {
  impression_data_ = data;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetImpressionOrigin(
    const url::Origin& origin) {
  impression_origin_ = origin;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetConversionOrigin(
    const url::Origin& origin) {
  conversion_origin_ = origin;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetReportingOrigin(
    const url::Origin& origin) {
  reporting_origin_ = origin;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetSourceType(
    StorableImpression::SourceType source_type) {
  source_type_ = source_type;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetImpressionId(
    absl::optional<int64_t> impression_id) {
  impression_id_ = impression_id;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetDedupKeys(
    std::vector<int64_t> dedup_keys) {
  dedup_keys_ = std::move(dedup_keys);
  return *this;
}

StorableImpression ImpressionBuilder::Build() const {
  StorableImpression impression(impression_data_, impression_origin_,
                                conversion_origin_, reporting_origin_,
                                impression_time_,
                                /*expiry_time=*/impression_time_ + expiry_,
                                source_type_, priority_, impression_id_);
  impression.SetDedupKeys(dedup_keys_);
  return impression;
}

StorableConversion DefaultConversion() {
  return ConversionBuilder().Build();
}

ConversionBuilder::ConversionBuilder()
    : conversion_destination_(
          net::SchemefulSite(GURL(kDefaultConversionDestination))),
      reporting_origin_(url::Origin::Create(GURL(kDefaultReportOrigin))) {}

ConversionBuilder::~ConversionBuilder() = default;

ConversionBuilder& ConversionBuilder::SetConversionData(
    uint64_t conversion_data) {
  conversion_data_ = conversion_data;
  return *this;
}

ConversionBuilder& ConversionBuilder::SetEventSourceTriggerData(
    uint64_t event_source_trigger_data) {
  event_source_trigger_data_ = event_source_trigger_data;
  return *this;
}

ConversionBuilder& ConversionBuilder::SetConversionDestination(
    const net::SchemefulSite& conversion_destination) {
  conversion_destination_ = conversion_destination;
  return *this;
}

ConversionBuilder& ConversionBuilder::SetReportingOrigin(
    const url::Origin& reporting_origin) {
  reporting_origin_ = reporting_origin;
  return *this;
}

ConversionBuilder& ConversionBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
  return *this;
}

ConversionBuilder& ConversionBuilder::SetDedupKey(
    absl::optional<int64_t> dedup_key) {
  dedup_key_ = dedup_key;
  return *this;
}

StorableConversion ConversionBuilder::Build() const {
  return StorableConversion(conversion_data_, conversion_destination_,
                            reporting_origin_, event_source_trigger_data_,
                            priority_, dedup_key_);
}

// Custom comparator for StorableImpressions that does not take impression IDs
// or dedup keys into account.
bool operator==(const StorableImpression& a, const StorableImpression& b) {
  const auto tie = [](const StorableImpression& impression) {
    return std::make_tuple(
        impression.impression_data(), impression.impression_origin(),
        impression.conversion_origin(), impression.reporting_origin(),
        impression.impression_time(), impression.expiry_time(),
        impression.source_type(), impression.priority());
  };
  return tie(a) == tie(b);
}

// Custom comparator for comparing two vectors of conversion reports. Does not
// compare impression and conversion IDs as they are set by the underlying
// sqlite db and should not be tested.
bool operator==(const ConversionReport& a, const ConversionReport& b) {
  const auto tie = [](const ConversionReport& conversion) {
    return std::make_tuple(conversion.impression, conversion.conversion_data,
                           conversion.conversion_time, conversion.report_time,
                           conversion.original_report_time);
  };
  return tie(a) == tie(b);
}

bool operator==(const SentReportInfo& a, const SentReportInfo& b) {
  const auto tie = [](const SentReportInfo& info) {
    return std::make_tuple(info.report_url, info.report_body,
                           info.http_response_code, info.original_report_time,
                           info.conversion_id);
  };
  return tie(a) == tie(b);
}

std::vector<ConversionReport> GetConversionsToReportForTesting(
    ConversionManagerImpl* manager,
    base::Time max_report_time) {
  base::RunLoop run_loop;
  std::vector<ConversionReport> conversion_reports;
  manager->conversion_storage_
      .AsyncCall(&ConversionStorage::GetConversionsToReport)
      .WithArgs(max_report_time, /*limit=*/-1)
      .Then(base::BindOnce(base::BindLambdaForTesting(
          [&](std::vector<ConversionReport> reports) {
            conversion_reports = std::move(reports);
            run_loop.Quit();
          })));
  run_loop.Run();
  return conversion_reports;
}

SentReportInfo GetBlankSentReportInfo() {
  return SentReportInfo(/*conversion_id=*/0,
                        /*original_report_time=*/base::Time(),
                        /*report_url=*/GURL(),
                        /*report_body=*/"",
                        /*http_response_code=*/0,
                        /*should_retry*/ false);
}

}  // namespace content
