// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_

#include <list>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/browser/conversions/sent_report_info.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace content {

class ConversionDisallowingContentBrowserClient
    : public TestContentBrowserClient {
 public:
  ConversionDisallowingContentBrowserClient() = default;
  ~ConversionDisallowingContentBrowserClient() override = default;

  // ContentBrowserClient:
  bool IsConversionMeasurementAllowed(
      content::BrowserContext* browser_context) override;
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

class ConfigurableStorageDelegate : public ConversionStorage::Delegate {
 public:
  ConfigurableStorageDelegate();
  ~ConfigurableStorageDelegate() override;

  // ConversionStorage::Delegate
  const StorableImpression& GetImpressionToAttribute(
      const std::vector<StorableImpression>& impressions) override;
  void ProcessNewConversionReport(ConversionReport& report) override;
  int GetMaxConversionsPerImpression(
      StorableImpression::SourceType source_type) const override;
  int GetMaxImpressionsPerOrigin() const override;
  int GetMaxConversionsPerOrigin() const override;
  RateLimitConfig GetRateLimits() const override;

  void set_max_conversions_per_impression(int max) {
    max_conversions_per_impression_ = max;
  }

  void set_max_impressions_per_origin(int max) {
    max_impressions_per_origin_ = max;
  }

  void set_max_conversions_per_origin(int max) {
    max_conversions_per_origin_ = max;
  }

  void set_rate_limits(RateLimitConfig c) { rate_limits_ = c; }

  void set_report_time_ms(int report_time_ms) {
    report_time_ms_ = report_time_ms;
  }

 private:
  int max_conversions_per_impression_ = INT_MAX;
  int max_impressions_per_origin_ = INT_MAX;
  int max_conversions_per_origin_ = INT_MAX;

  RateLimitConfig rate_limits_ = {
      .time_window = base::TimeDelta::Max(),
      .max_attributions_per_window = INT_MAX,
  };

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
  void HandleImpression(const StorableImpression& impression) override;
  void HandleConversion(const StorableConversion& conversion) override;
  void GetActiveImpressionsForWebUI(
      base::OnceCallback<void(std::vector<StorableImpression>)> callback)
      override;
  void GetPendingReportsForWebUI(
      base::OnceCallback<void(std::vector<ConversionReport>)> callback,
      base::Time max_report_time) override;
  const base::circular_deque<SentReportInfo>& GetSentReportsForWebUI() override;
  void SendReportsForWebUI(base::OnceClosure done) override;
  const ConversionPolicy& GetConversionPolicy() const override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 base::RepeatingCallback<bool(const url::Origin&)> filter,
                 base::OnceClosure done) override;

  void SetActiveImpressionsForWebUI(
      std::vector<StorableImpression> impressions);
  void SetReportsForWebUI(std::vector<ConversionReport> reports);
  void SetSentReportsForWebUI(
      base::circular_deque<SentReportInfo> sent_reports);

  // Resets all counters on this.
  void Reset();

  size_t num_impressions() const { return num_impressions_; }
  size_t num_conversions() const { return num_conversions_; }

  const net::SchemefulSite& last_conversion_destination() {
    return last_conversion_destination_;
  }

  const absl::optional<StorableImpression::SourceType>&
  last_impression_source_type() {
    return last_impression_source_type_;
  }

  const absl::optional<url::Origin>& last_impression_origin() {
    return last_impression_origin_;
  }

  const absl::optional<int64_t> last_attribution_source_priority() {
    return last_attribution_source_priority_;
  }

 private:
  ConversionPolicy policy_;
  net::SchemefulSite last_conversion_destination_;
  absl::optional<StorableImpression::SourceType> last_impression_source_type_;
  absl::optional<url::Origin> last_impression_origin_;
  absl::optional<int64_t> last_attribution_source_priority_;
  size_t num_impressions_ = 0;
  size_t num_conversions_ = 0;

  std::vector<StorableImpression> impressions_;
  std::vector<ConversionReport> reports_;
  base::circular_deque<SentReportInfo> sent_reports_;
};

// Helper class to construct a StorableImpression for tests using default data.
// StorableImpression members are not mutable after construction requiring a
// builder pattern.
class ImpressionBuilder {
 public:
  explicit ImpressionBuilder(base::Time time);
  ~ImpressionBuilder();

  ImpressionBuilder& SetExpiry(base::TimeDelta delta);

  ImpressionBuilder& SetData(const std::string& data);

  ImpressionBuilder& SetImpressionOrigin(const url::Origin& origin);

  ImpressionBuilder& SetConversionOrigin(const url::Origin& domain);

  ImpressionBuilder& SetReportingOrigin(const url::Origin& origin);

  ImpressionBuilder& SetSourceType(StorableImpression::SourceType source_type);

  ImpressionBuilder& SetPriority(int64_t priority);

  ImpressionBuilder& SetImpressionId(absl::optional<int64_t> impression_id);

  StorableImpression Build() const;

 private:
  std::string impression_data_;
  base::Time impression_time_;
  base::TimeDelta expiry_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
  StorableImpression::SourceType source_type_;
  int64_t priority_;
  absl::optional<int64_t> impression_id_;
};

// Returns a StorableConversion with default data which matches the default
// impressions created by ImpressionBuilder.
StorableConversion DefaultConversion();

testing::AssertionResult ImpressionsEqual(const StorableImpression& expected,
                                          const StorableImpression& actual);

testing::AssertionResult ReportsEqual(
    const std::vector<ConversionReport>& expected,
    const std::vector<ConversionReport>& actual);

testing::AssertionResult SentReportInfosEqual(
    const base::circular_deque<SentReportInfo>& expected,
    const base::circular_deque<SentReportInfo>& actual);

std::vector<ConversionReport> GetConversionsToReportForTesting(
    ConversionManagerImpl* manager,
    base::Time max_report_time);

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_
