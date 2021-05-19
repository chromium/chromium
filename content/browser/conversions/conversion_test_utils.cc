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

bool ConversionDisallowingContentBrowserClient::IsConversionMeasurementAllowed(
    content::BrowserContext* browser_context) {
  return false;
}

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

const StorableImpression& ConfigurableStorageDelegate::GetImpressionToAttribute(
    const std::vector<StorableImpression>& impressions) {
  DCHECK(!impressions.empty());

  return *std::max_element(
      impressions.begin(), impressions.end(),
      [](const StorableImpression& a, const StorableImpression& b) {
        return a.impression_time() < b.impression_time();
      });
}

void ConfigurableStorageDelegate::ProcessNewConversionReport(
    ConversionReport& report) {
  report.report_time = report.impression.impression_time() +
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
ConversionStorage::Delegate::RateLimitConfig
ConfigurableStorageDelegate::GetRateLimits() const {
  return rate_limits_;
}

ConversionManager* TestManagerProvider::GetManager(
    WebContents* web_contents) const {
  return manager_;
}

TestConversionManager::TestConversionManager() = default;

TestConversionManager::~TestConversionManager() = default;

void TestConversionManager::HandleImpression(
    const StorableImpression& impression) {
  num_impressions_++;
  last_impression_source_type_ = impression.source_type();
  last_impression_origin_ = impression.impression_origin();
  last_attribution_source_priority_ = impression.priority();
}

void TestConversionManager::HandleConversion(
    const StorableConversion& conversion) {
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
TestConversionManager::GetSentReportsForWebUI() {
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
    : impression_data_("123"),
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

ImpressionBuilder& ImpressionBuilder::SetData(const std::string& data) {
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

StorableImpression ImpressionBuilder::Build() const {
  return StorableImpression(impression_data_, impression_origin_,
                            conversion_origin_, reporting_origin_,
                            impression_time_,
                            impression_time_ + expiry_ /* expiry_time */,
                            source_type_, priority_, impression_id_);
}

StorableConversion DefaultConversion() {
  StorableConversion conversion(
      "111" /* conversion_data */,
      net::SchemefulSite(
          GURL(kDefaultConversionDestination)) /* conversion_destination */,
      url::Origin::Create(GURL(kDefaultReportOrigin)) /* reporting_origin */);
  return conversion;
}

// Custom comparator for StorableImpressions that does not take impression id's
// into account.
testing::AssertionResult ImpressionsEqual(const StorableImpression& expected,
                                          const StorableImpression& actual) {
  const auto tie = [](const StorableImpression& impression) {
    return std::make_tuple(
        impression.impression_data(), impression.impression_origin(),
        impression.conversion_origin(), impression.reporting_origin(),
        impression.impression_time(), impression.expiry_time(),
        impression.priority());
  };

  if (tie(expected) != tie(actual)) {
    return testing::AssertionFailure();
  }
  return testing::AssertionSuccess();
}

// Custom comparator for comparing two vectors of conversion reports. Does not
// compare impression and conversion id's as they are set by the underlying
// sqlite db and should not be tested.
testing::AssertionResult ReportsEqual(
    const std::vector<ConversionReport>& expected,
    const std::vector<ConversionReport>& actual) {
  const auto tie = [](const ConversionReport& conversion) {
    return std::make_tuple(conversion.impression.impression_data(),
                           conversion.impression.impression_origin(),
                           conversion.impression.conversion_origin(),
                           conversion.impression.reporting_origin(),
                           conversion.impression.impression_time(),
                           conversion.impression.expiry_time(),
                           conversion.impression.priority(),
                           conversion.conversion_data, conversion.report_time);
  };

  if (expected.size() != actual.size())
    return testing::AssertionFailure() << "Expected length " << expected.size()
                                       << ", actual: " << actual.size();

  for (size_t i = 0; i < expected.size(); i++) {
    if (tie(expected[i]) != tie(actual[i])) {
      return testing::AssertionFailure()
             << "Expected " << expected[i] << " at index " << i
             << ", actual: " << actual[i];
    }
  }

  return testing::AssertionSuccess();
}

testing::AssertionResult SentReportInfosEqual(
    const base::circular_deque<SentReportInfo>& expected,
    const base::circular_deque<SentReportInfo>& actual) {
  const auto tie = [](const SentReportInfo& info) {
    return std::make_tuple(info.report_url, info.report_body,
                           info.http_response_code);
  };

  if (expected.size() != actual.size())
    return testing::AssertionFailure() << "Expected length " << expected.size()
                                       << ", actual: " << actual.size();

  for (size_t i = 0; i < expected.size(); i++) {
    if (tie(expected[i]) != tie(actual[i])) {
      return testing::AssertionFailure()
             << "Expected " << expected[i] << " at index " << i
             << ", actual: " << actual[i];
    }
  }

  return testing::AssertionSuccess();
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

}  // namespace content
