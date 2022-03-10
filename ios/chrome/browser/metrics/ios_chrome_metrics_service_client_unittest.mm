// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"

#include <string>

#include "base/files/file_path.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/unsent_log_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ukm {
class UkmService;
}

class IOSChromeMetricsServiceClientTest : public PlatformTest {
 public:
  IOSChromeMetricsServiceClientTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(
                TestChromeBrowserState::Builder().Build())),
        enabled_state_provider_(/*consent=*/false, /*enabled=*/false) {
    browser_state_ = GetApplicationContext()
                         ->GetChromeBrowserStateManager()
                         ->GetLastUsedBrowserState();
  }

  IOSChromeMetricsServiceClientTest(const IOSChromeMetricsServiceClientTest&) =
      delete;
  IOSChromeMetricsServiceClientTest& operator=(
      const IOSChromeMetricsServiceClientTest&) = delete;

  void SetUp() override {
    PlatformTest::SetUp();
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, &enabled_state_provider_, std::wstring(), base::FilePath());
    metrics_state_manager_->InstantiateFieldTrialList();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  ChromeBrowserState* browser_state_;
  metrics::TestEnabledStateProvider enabled_state_provider_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
};

namespace {

TEST_F(IOSChromeMetricsServiceClientTest, FilterFiles) {
  base::ProcessId my_pid = base::GetCurrentProcId();
  base::FilePath active_dir(FILE_PATH_LITERAL("foo"));
  base::FilePath upload_dir(FILE_PATH_LITERAL("bar"));
  base::FilePath upload_path =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          upload_dir, "TestMetrics");
  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID,
      IOSChromeMetricsServiceClient::FilterBrowserMetricsFiles(upload_path));
  EXPECT_EQ(metrics::FileMetricsProvider::FILTER_PROCESS_FILE,
            IOSChromeMetricsServiceClient::FilterBrowserMetricsFiles(
                base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
                    upload_dir, "Test", base::Time::Now(), (my_pid + 10))));
}

}  // namespace

// This is not in anonymous namespace so this test can be a friend class of
// MetricsService for accessing protected ivars.
TEST_F(IOSChromeMetricsServiceClientTest, TestRegisterMetricsServiceProviders) {
  // This is for the two metrics providers added in the MetricsService
  // constructor: StabilityMetricsProvider and MetricsStateMetricsProvider.
  size_t expected_providers = 2;

  // This is the number of metrics providers that are registered inside
  // IOSChromeMetricsServiceClient::Initialize().
  expected_providers += 17;

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get());
  EXPECT_EQ(expected_providers,
            chrome_metrics_service_client->GetMetricsService()
                ->delegating_provider_.GetProviders()
                .size());
}

TEST_F(IOSChromeMetricsServiceClientTest,
       TestRegisterUkmProvidersWhenUKMFeatureEnabled) {
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(ukm::kUkmFeature);

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get());

  ukm::UkmService* ukmService =
      chrome_metrics_service_client->GetUkmService();
  // Verify that the UKM service is instantiated when enabled.
  EXPECT_TRUE(ukmService);

  // Number of providers registered by
  // IOSChromeMetricsServiceClient::RegisterMetricsServiceProviders(), namely
  // CPUMetricsProvider, ScreenInfoMetricsProvider, FormFactorMetricsProvider,
  // and FieldTrialsProvider.
  const size_t expected_providers = 4;

  EXPECT_EQ(expected_providers,
            ukmService->metrics_providers_.GetProviders().size());
}

TEST_F(IOSChromeMetricsServiceClientTest,
       TestRegisterUkmProvidersWhenForceMetricsReporting) {
  // Disable the feature of reporting UKM metrics.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(ukm::kUkmFeature);

  // Force metrics reporting using the commandline switch.
  metrics::ForceEnableMetricsReportingForTesting();

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get());
  // Verify that the UKM service is instantiated when enabled.
  EXPECT_TRUE(chrome_metrics_service_client->GetUkmService());
}

TEST_F(IOSChromeMetricsServiceClientTest, TestUkmProvidersWhenDisabled) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(ukm::kUkmFeature);

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get());
  // Verify that the UKM service is not instantiated when disabled.
  EXPECT_FALSE(chrome_metrics_service_client->GetUkmService());
}

TEST_F(IOSChromeMetricsServiceClientTest, GetUploadSigningKey_NotEmpty) {
  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get());
  [[maybe_unused]] const std::string signing_key =
      chrome_metrics_service_client->GetUploadSigningKey();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The signing key should never be an empty string for a Chrome-branded build.
  EXPECT_FALSE(signing_key.empty());
#else
  // In non-branded builds, we may still have a valid signing key if
  // USE_OFFICIAL_GOOGLE_API_KEYS is true. However, that macro is not available
  // in this file.
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

TEST_F(IOSChromeMetricsServiceClientTest, GetUploadSigningKey_CanSignLogs) {
  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get());
  const std::string signing_key =
      chrome_metrics_service_client->GetUploadSigningKey();

  std::string signature;
  bool sign_success = metrics::UnsentLogStore::ComputeHMACForLog(
      "Test Log Data", signing_key, &signature);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The signing key should be able to sign data for a Chrome-branded build.
  EXPECT_TRUE(sign_success);
  EXPECT_FALSE(signature.empty());
#else
  // In non-branded builds, we may still have a valid signing key if
  // USE_OFFICIAL_GOOGLE_API_KEYS is true. However, that macro is not available
  // in this file, so just check that success == a non-empty signature.
  EXPECT_EQ(sign_success, !signature.empty());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
