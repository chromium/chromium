// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_client.h"

#import <string>

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/persistent_histogram_allocator.h"
#import "base/test/scoped_feature_list.h"
#import "build/branding_buildflags.h"
#import "components/metrics/client_info.h"
#import "components/metrics/dwa/dwa_recorder.h"
#import "components/metrics/metrics_features.h"
#import "components/metrics/metrics_service.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/metrics_switches.h"
#import "components/metrics/private_metrics/private_metrics_features.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/metrics/unsent_log_store.h"
#import "components/prefs/testing_pref_service.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/sync/test/test_sync_service.h"
#import "components/ukm/ukm_service.h"
#import "components/ukm/ukm_test_helper.h"
#import "components/variations/synthetic_trial_registry.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace ukm {
class UkmService;
}

class IOSChromeMetricsServiceClientTest : public PlatformTest {
 public:
  IOSChromeMetricsServiceClientTest()
      : enabled_state_provider_(/*consent=*/false, /*enabled=*/false) {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    profile_manager_.AddProfileWithBuilder(std::move(builder));
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
    synthetic_trial_registry_ =
        std::make_unique<variations::SyntheticTrialRegistry>();
  }

  void TriggerOnAdvancedReportingEnabledForAllProfilesChanged(
      IOSChromeMetricsServiceClient* client,
      bool enabled,
      bool reset_client_state) {
    client->OnAdvancedReportingEnabledForAllProfilesChanged(enabled,
                                                            reset_client_state);
  }

  bool RegisterForProfileEvents(IOSChromeMetricsServiceClient* client,
                                ProfileIOS* profile) {
    return client->RegisterForProfileEvents(profile);
  }

  void OnProfileUnloaded(IOSChromeMetricsServiceClient* client,
                         ProfileIOS* profile) {
    client->OnProfileUnloaded(nullptr, profile);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  metrics::TestEnabledStateProvider enabled_state_provider_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
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
  expected_providers += 24;

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  EXPECT_EQ(expected_providers,
            chrome_metrics_service_client->GetMetricsService()
                ->delegating_provider_.GetProviders()
                .size());
}

TEST_F(IOSChromeMetricsServiceClientTest, TestDwaServiceNotInitialized) {
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(metrics::dwa::kDwaFeature);

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  EXPECT_EQ(chrome_metrics_service_client->GetDwaService(), nullptr);
}

TEST_F(IOSChromeMetricsServiceClientTest, TestDwaServiceInitialized) {
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(metrics::dwa::kDwaFeature);

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  EXPECT_NE(chrome_metrics_service_client->GetDwaService(), nullptr);
}

TEST_F(IOSChromeMetricsServiceClientTest, TestPumaServiceNotInitialized) {
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(
      metrics::private_metrics::kPrivateMetricsPuma);

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  EXPECT_EQ(chrome_metrics_service_client->GetPumaService(), nullptr);
}

TEST_F(IOSChromeMetricsServiceClientTest, TestPumaServiceInitialized) {
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      metrics::private_metrics::kPrivateMetricsPuma);

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  EXPECT_NE(chrome_metrics_service_client->GetPumaService(), nullptr);
}

TEST_F(IOSChromeMetricsServiceClientTest,
       TestRegisterUkmProvidersWhenUKMFeatureEnabled) {
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(ukm::kUkmFeature);

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());

  ukm::UkmService* ukmService = chrome_metrics_service_client->GetUkmService();
  // Verify that the UKM service is instantiated when enabled.
  EXPECT_TRUE(ukmService);

  // The number of providers registered in
  // IOSChromeMetricsServiceClient::RegisterMetricsServiceProviders().
  const size_t expected_providers = 6;

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
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  // Verify that the UKM service is instantiated when enabled.
  EXPECT_TRUE(chrome_metrics_service_client->GetUkmService());
}

TEST_F(IOSChromeMetricsServiceClientTest, TestUkmProvidersWhenDisabled) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(ukm::kUkmFeature);

  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  // Verify that the UKM service is not instantiated when disabled.
  EXPECT_FALSE(chrome_metrics_service_client->GetUkmService());
}

TEST_F(IOSChromeMetricsServiceClientTest, GetUploadSigningKey_NotEmpty) {
  std::unique_ptr<IOSChromeMetricsServiceClient> chrome_metrics_service_client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
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
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  const std::string signing_key =
      chrome_metrics_service_client->GetUploadSigningKey();

  std::string signature =
      metrics::UnsentLogStore::ComputeHMACForLog("Test Log Data", signing_key);
  // The signing operation itself never fails, even if there is no key
  // available: empty keys are padded out with 0 bytes.
  EXPECT_FALSE(signature.empty());
}

TEST_F(IOSChromeMetricsServiceClientTest,
       OnAdvancedReportingEnabledForAllProfilesChanged) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {ukm::kUkmFeature, metrics::features::kRestructureMetricsConsentSettings},
      {});

  std::unique_ptr<IOSChromeMetricsServiceClient> client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  ukm::UkmService* ukm_service = client->GetUkmService();
  ASSERT_TRUE(ukm_service);

  uint64_t initial_client_id = ukm_service->client_id();

  // Trigger state change with reset_client_state = false.
  // UKM client ID should NOT change.
  TriggerOnAdvancedReportingEnabledForAllProfilesChanged(
      client.get(), /*enabled=*/false, /*reset_client_state=*/false);
  EXPECT_EQ(initial_client_id, ukm_service->client_id());

  // Trigger state change with reset_client_state = true.
  // UKM client ID SHOULD change.
  TriggerOnAdvancedReportingEnabledForAllProfilesChanged(
      client.get(), /*enabled=*/false, /*reset_client_state=*/true);
  EXPECT_NE(initial_client_id, ukm_service->client_id());
}

TEST_F(IOSChromeMetricsServiceClientTest,
       AdvancedReportingDataRetentionOnProfileUnload) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {ukm::kUkmFeature, metrics::features::kRestructureMetricsConsentSettings},
      {});

  std::unique_ptr<IOSChromeMetricsServiceClient> client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  ukm::UkmService* ukm_service = client->GetUkmService();
  ASSERT_TRUE(ukm_service);

  ukm::UkmTestHelper ukm_test_helper(ukm_service);

  // Get the testing profile created in the constructor.
  std::vector<ProfileIOS*> loaded_profiles =
      profile_manager_.GetLoadedProfiles();
  ASSERT_EQ(1U, loaded_profiles.size());
  ProfileIOS* profile = loaded_profiles[0];

  // Enable advanced reporting for this profile.
  // This will enable UKM recording and trigger initialization.
  metrics::MetricsReportingChoiceService::SetAdvancedReportingEnabled(
      profile->GetPrefs(), true);
  EXPECT_TRUE(client->IsUkmAllowedForAllProfiles());

  // Manually enable recording/reporting since MetricsServicesManager is not
  // running in this unit test.
  ukm_service->EnableRecording();
  ukm_service->EnableReporting();

  // Setup: build and store a dummy log to verify purging logic.
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_test_helper.RecordSourceForTesting(source_id);
  ukm_test_helper.BuildAndStoreLog();
  ASSERT_TRUE(ukm_test_helper.HasUnsentLogs());

  uint64_t initial_client_id = ukm_service->client_id();

  // Simulate profile unloading (like during browser shutdown).
  // This will trigger state change with reset_client_state = false.
  // UKM client ID should NOT change, and unsent logs should NOT be purged.
  OnProfileUnloaded(client.get(), profile);
  EXPECT_FALSE(client->IsUkmAllowedForAllProfiles());
  EXPECT_EQ(initial_client_id, ukm_service->client_id());
  EXPECT_TRUE(ukm_test_helper.HasUnsentLogs());
}

TEST_F(IOSChromeMetricsServiceClientTest,
       AdvancedReportingDataPurgeOnConsentRevocation) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatures(
      {ukm::kUkmFeature, metrics::features::kRestructureMetricsConsentSettings},
      {});

  std::unique_ptr<IOSChromeMetricsServiceClient> client =
      IOSChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                            synthetic_trial_registry_.get());
  ukm::UkmService* ukm_service = client->GetUkmService();
  ASSERT_TRUE(ukm_service);

  ukm::UkmTestHelper ukm_test_helper(ukm_service);

  // Get the testing profile created in the constructor.
  std::vector<ProfileIOS*> loaded_profiles =
      profile_manager_.GetLoadedProfiles();
  ASSERT_EQ(1U, loaded_profiles.size());
  ProfileIOS* profile = loaded_profiles[0];

  // Enable advanced reporting for this profile.
  // This will enable UKM recording and trigger initialization.
  metrics::MetricsReportingChoiceService::SetAdvancedReportingEnabled(
      profile->GetPrefs(), true);
  EXPECT_TRUE(client->IsUkmAllowedForAllProfiles());

  // Manually enable recording/reporting since MetricsServicesManager is not
  // running in this unit test.
  ukm_service->EnableRecording();
  ukm_service->EnableReporting();

  // Setup: build and store a dummy log to verify purging logic.
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_test_helper.RecordSourceForTesting(source_id);
  ukm_test_helper.BuildAndStoreLog();
  ASSERT_TRUE(ukm_test_helper.HasUnsentLogs());

  uint64_t initial_client_id = ukm_service->client_id();

  // Simulate user revoking consent (setting pref to false).
  // This will trigger state change with reset_client_state = true.
  // UKM client ID SHOULD change, and unsent logs SHOULD be purged.
  metrics::MetricsReportingChoiceService::SetAdvancedReportingEnabled(
      profile->GetPrefs(), false);
  EXPECT_FALSE(client->IsUkmAllowedForAllProfiles());
  EXPECT_NE(initial_client_id, ukm_service->client_id());
  EXPECT_FALSE(ukm_test_helper.HasUnsentLogs());
}
