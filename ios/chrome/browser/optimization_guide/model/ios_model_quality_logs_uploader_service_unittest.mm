// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/ios_model_quality_logs_uploader_service.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/metrics_services_manager/metrics_services_manager.h"
#import "components/metrics_services_manager/metrics_services_manager_client.h"
#import "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class TestMetricsServicesManagerClient
    : public metrics_services_manager::MetricsServicesManagerClient {
 public:
  TestMetricsServicesManagerClient(
      metrics::TestEnabledStateProvider* enabled_state_provider,
      metrics::MetricsStateManager* metrics_state_manager_ptr,
      scoped_refptr<network::SharedURLLoaderFactory>
          test_shared_url_loader_factory)
      : enabled_state_provider_(enabled_state_provider),
        metrics_state_manager_(metrics_state_manager_ptr),
        test_shared_url_loader_factory_(test_shared_url_loader_factory) {}

  std::unique_ptr<variations::VariationsService> CreateVariationsService()
      override {
    return nullptr;
  }
  std::unique_ptr<metrics::MetricsServiceClient> CreateMetricsServiceClient(
      variations::SyntheticTrialRegistry* synthetic_trial_registry) override {
    return nullptr;
  }
  metrics::MetricsStateManager* GetMetricsStateManager() override {
    return metrics_state_manager_;
  }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return test_shared_url_loader_factory_;
  }
  const metrics::EnabledStateProvider& GetEnabledStateProvider() override {
    return *enabled_state_provider_;
  }
  bool IsOffTheRecordSessionActive() override { return false; }

 private:
  raw_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  raw_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

// Creates a MetricsServicesManager and sets it as the
// TestingApplicationContext's MetricsServicesManager for the life of the
// instance.
class ScopedMetricsServiceManager {
 public:
  explicit ScopedMetricsServiceManager(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
      : shared_url_loader_factory_(shared_url_loader_factory) {
    EXPECT_EQ(nullptr, GetApplicationContext()->GetMetricsServicesManager());
    enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(false, false);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        GetApplicationContext()->GetLocalState(), enabled_state_provider_.get(),
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);

    auto client = std::make_unique<TestMetricsServicesManagerClient>(
        enabled_state_provider_.get(), metrics_state_manager_.get(),
        shared_url_loader_factory_);

    metrics_services_manager_ =
        std::make_unique<metrics_services_manager::MetricsServicesManager>(
            std::move(client));

    TestingApplicationContext::GetGlobal()->SetMetricsServicesManager(
        metrics_services_manager_.get());
  }

  ~ScopedMetricsServiceManager() {
    EXPECT_EQ(metrics_services_manager_.get(),
              GetApplicationContext()->GetMetricsServicesManager());
    TestingApplicationContext::GetGlobal()->SetMetricsServicesManager(nullptr);
  }

  void SetMetricsConsent(bool consent) {
    enabled_state_provider_->set_consent(consent);
  }

 private:
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

}  // namespace

class IOSModelQualityLogsUploaderServiceTest : public PlatformTest {
 public:
  IOSModelQualityLogsUploaderServiceTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        scoped_metrics_services_manager_(test_shared_url_loader_factory_) {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kModelQualityLogging}, {});
  }

  std::unique_ptr<optimization_guide::MqlsFeatureMetadata>
  CreateMqlsFeatureMetadata(
      const base::Feature* field_trial_feature = nullptr) {
    optimization_guide::UserFeedbackCallback logging_callback =
        base::BindRepeating(
            [](optimization_guide::proto::LogAiDataRequest& request_proto) {
              return optimization_guide::proto::UserFeedback::
                  USER_FEEDBACK_UNSPECIFIED;
            });

    return std::make_unique<optimization_guide::MqlsFeatureMetadata>(
        "TestFeature",
        optimization_guide::proto::LogAiDataRequest::FEATURE_NOT_SET,
        optimization_guide::EnterprisePolicyPref("dummy_policy_pref"),
        field_trial_feature, logging_callback);
  }

  IOSModelQualityLogsUploaderService CreateUploaderService() {
    return IOSModelQualityLogsUploaderService(
        test_shared_url_loader_factory_,
        GetApplicationContext()->GetLocalState(),
        base::WeakPtr<optimization_guide::ModelExecutionFeaturesController>());
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  ScopedMetricsServiceManager scoped_metrics_services_manager_;
};

TEST_F(IOSModelQualityLogsUploaderServiceTest, CanUploadLogs_FeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      optimization_guide::features::kModelQualityLogging);

  IOSModelQualityLogsUploaderService service = CreateUploaderService();

  scoped_metrics_services_manager_.SetMetricsConsent(true);

  auto metadata = CreateMqlsFeatureMetadata(
      &optimization_guide::features::kModelQualityLogging);

  EXPECT_FALSE(service.CanUploadLogs(metadata.get()));
}

TEST_F(IOSModelQualityLogsUploaderServiceTest, CanUploadLogs_FeatureEnabled) {
  // Consent is given.
  scoped_metrics_services_manager_.SetMetricsConsent(true);

  IOSModelQualityLogsUploaderService service = CreateUploaderService();

  auto metadata = CreateMqlsFeatureMetadata(
      &optimization_guide::features::kModelQualityLogging);

  EXPECT_TRUE(service.CanUploadLogs(metadata.get()));
}

TEST_F(IOSModelQualityLogsUploaderServiceTest, CanUploadLogs_ConsentGiven) {
  IOSModelQualityLogsUploaderService service = CreateUploaderService();

  auto metadata = CreateMqlsFeatureMetadata(
      &optimization_guide::features::kModelQualityLogging);

  // Consent is not given by default.
  EXPECT_FALSE(service.CanUploadLogs(metadata.get()));

  // Consent is given.
  scoped_metrics_services_manager_.SetMetricsConsent(true);
  EXPECT_TRUE(service.CanUploadLogs(metadata.get()));
}

TEST_F(IOSModelQualityLogsUploaderServiceTest,
       SetSystemMetadata_ClientIdsAreStripped) {
  IOSModelQualityLogsUploaderService service = CreateUploaderService();

  optimization_guide::proto::LoggingMetadata metadata;
  metadata.mutable_system_profile()->set_client_uuid("123");
  metadata.mutable_system_profile()
      ->mutable_cloned_install_info()
      ->set_cloned_from_client_id(123);

  service.SetSystemMetadata(&metadata);
  EXPECT_FALSE(metadata.system_profile().has_client_uuid());
  EXPECT_FALSE(metadata.system_profile()
                   .cloned_install_info()
                   .has_cloned_from_client_id());
}
