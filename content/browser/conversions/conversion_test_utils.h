// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_

#include <list>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace content {

class ConversionDisallowingContentBrowserClient
    : public TestContentBrowserClient {
 public:
  ConversionDisallowingContentBrowserClient() = default;
  ~ConversionDisallowingContentBrowserClient() override = default;

  // ContentBrowserClient:
  bool AllowConversionMeasurement(BrowserContext* context) override;
};

class ConfigurableStorageDelegate : public ConversionStorage::Delegate {
 public:
  using AttributionCredits = std::list<int>;
  ConfigurableStorageDelegate();
  ~ConfigurableStorageDelegate() override;

  // ConversionStorage::Delegate
  void ProcessNewConversionReports(
      std::vector<ConversionReport>* reports) override;
  int GetMaxConversionsPerImpression() const override;
  int GetMaxImpressionsPerOrigin() const override;
  int GetMaxConversionsPerOrigin() const override;

  void set_max_conversions_per_impression(int max) {
    max_conversions_per_impression_ = max;
  }

  void set_max_impressions_per_origin(int max) {
    max_impressions_per_origin_ = max;
  }

  void set_max_conversions_per_origin(int max) {
    max_conversions_per_origin_ = max;
  }

  void set_report_time_ms(int report_time_ms) {
    report_time_ms_ = report_time_ms;
  }

  void AddCredits(AttributionCredits credits) {
    // Add all credits to our list in order.
    attribution_credits_.splice(attribution_credits_.end(), credits);
  }

 private:
  int max_conversions_per_impression_ = INT_MAX;
  int max_impressions_per_origin_ = INT_MAX;
  int max_conversions_per_origin_ = INT_MAX;

  int report_time_ms_ = 0;

  // List of attribution credits the test delegate should associate with
  // reports.
  AttributionCredits attribution_credits_;
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
  void GetReportsForWebUI(
      base::OnceCallback<void(std::vector<ConversionReport>)> callback,
      base::Time max_report_time) override;
  void SendReportsForWebUI(base::OnceClosure done) override;
  const ConversionPolicy& GetConversionPolicy() const override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 base::RepeatingCallback<bool(const url::Origin&)> filter,
                 base::OnceClosure done) override;

  void SetActiveImpressionsForWebUI(
      std::vector<StorableImpression> impressions);
  void SetReportsForWebUI(std::vector<ConversionReport> reports);

  // Resets all counters on this.
  void Reset();

  size_t num_impressions() const { return num_impressions_; }
  size_t num_conversions() const { return num_conversions_; }

 private:
  ConversionPolicy policy_;
  size_t num_impressions_ = 0;
  size_t num_conversions_ = 0;

  std::vector<StorableImpression> impressions_;
  std::vector<ConversionReport> reports_;
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

  ImpressionBuilder& SetConversionOrigin(const url::Origin& origin);

  ImpressionBuilder& SetReportingOrigin(const url::Origin& origin);

  StorableImpression Build() const;

 private:
  std::string impression_data_;
  base::Time impression_time_;
  base::TimeDelta expiry_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
};

// Returns a StorableConversion with default data which matches the default
// impressions created by ImpressionBuilder.
StorableConversion DefaultConversion();

testing::AssertionResult ImpressionsEqual(const StorableImpression& expected,
                                          const StorableImpression& actual);

testing::AssertionResult ReportsEqual(
    const std::vector<ConversionReport>& expected,
    const std::vector<ConversionReport>& actual);

std::vector<ConversionReport> GetConversionsToReportForTesting(
    ConversionManagerImpl* manager,
    base::Time max_report_time);

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_
