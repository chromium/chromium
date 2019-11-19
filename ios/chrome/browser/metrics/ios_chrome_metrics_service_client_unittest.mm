// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"

#include "base/metrics/persistent_histogram_allocator.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_service.h"
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
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())),
        browser_state_(TestChromeBrowserState::Builder().Build()),
        enabled_state_provider_(/*consent=*/false, /*enabled=*/false) {}

  void SetUp() override {
    PlatformTest::SetUp();
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, &enabled_state_provider_, base::string16(),
        base::BindRepeating(
            &IOSChromeMetricsServiceClientTest::FakeStoreClientInfoBackup,
            base::Unretained(this)),
        base::BindRepeating(
            &IOSChromeMetricsServiceClientTest::LoadFakeClientInfoBackup,
            base::Unretained(this)));
  }

 protected:
  void FakeStoreClientInfoBackup(const metrics::ClientInfo& client_info) {}

  std::unique_ptr<metrics::ClientInfo> LoadFakeClientInfoBackup() {
    return std::make_unique<metrics::ClientInfo>();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  metrics::TestEnabledStateProvider enabled_state_provider_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSChromeMetricsServiceClientTest);
};

namespace {

TEST_F(IOSChromeMetricsServiceClientTest, FilterFiles) {
  base::ProcessId my_pid = base::GetCurrentProcId();
  base::FilePath active_dir(FILE_PATH_LITERAL("foo"));
  base::FilePath upload_dir(FILE_PATH_LITERAL("bar"));
  base::FilePath upload_path;
  base::GlobalHistogramAllocator::ConstructFilePathsForUploadDir(
      active_dir, upload_dir, "TestMetrics", &upload_path, nullptr, nullptr);
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
  // This is the metrics provider added in MetricsService constructor.
  // StabilityMetricsProvider, FieldTrialsProvider and
  // MetricsStateMetricsProvider.
  size_t expected_providers = 3;

  // This is the number of metrics providers that are registered inside
  // IOSChromeMetricsServiceClient::Initialize().
  expected_providers += 12;

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
  // Verify that the UKM service is instantiated when enabled.
  EXPECT_TRUE(chrome_metrics_service_client->GetUkmService());
}

TEST_F(IOSChromeMetricsServiceClientTest,
       TestRegisterUkmProvidersWhenForceMetricsReporting) {
  // Disable the feature of reporting UKM metrics.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(ukm::kUkmFeature);

  // Force metrics reporting using the commandline switch.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

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
