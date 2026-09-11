// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/connectors_internals/connectors_internals_page_handler.h"

#import <memory>
#import <set>
#import <string>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/enterprise/connectors/connectors_internals.mojom.h"
#import "components/enterprise/device_trust/core/attestation/attestation_service.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "components/enterprise/device_trust/core/device_trust_service.h"
#import "components/enterprise/device_trust/core/signals/signals_service.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/features.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_connector_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_ios.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class FakeAttestationService
    : public enterprise_connectors::AttestationService {
 public:
  FakeAttestationService() = default;
  ~FakeAttestationService() override = default;

  void BuildChallengeResponseForVAChallenge(
      const std::string& challenge,
      base::DictValue signals,
      const std::set<enterprise_connectors::DTCPolicyLevel>& levels,
      AttestationCallback callback) override {}
};

class FakeSignalsService : public enterprise_connectors::SignalsService {
 public:
  explicit FakeSignalsService(base::DictValue signals)
      : signals_(std::move(signals)) {}
  ~FakeSignalsService() override = default;

  void CollectSignals(CollectSignalsCallback callback) override {
    std::move(callback).Run(signals_.Clone());
  }

 private:
  base::DictValue signals_;
};

class FakeDeviceTrustService
    : public enterprise_connectors::DeviceTrustService {
 public:
  FakeDeviceTrustService(
      bool is_enabled,
      base::DictValue signals,
      enterprise_connectors::DeviceTrustConnectorService* connector_service)
      : enterprise_connectors::DeviceTrustService(
            std::make_unique<FakeAttestationService>(),
            std::make_unique<FakeSignalsService>(std::move(signals)),
            connector_service),
        is_enabled_(is_enabled) {}
  ~FakeDeviceTrustService() override = default;

  bool IsEnabled() const override { return is_enabled_; }

 private:
  bool is_enabled_;
};

class ConnectorsInternalsPageHandlerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    handler_ = std::make_unique<ConnectorsInternalsPageHandler>(
        page_handler_.BindNewPipeAndPassReceiver(), profile_.get());
  }

  void TearDown() override {
    handler_.reset();
    page_handler_.reset();
    profile_.reset();
    TestingApplicationContext::GetGlobal()
        ->GetBrowserPolicyConnector()
        ->Shutdown();
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  mojo::Remote<connectors_internals::mojom::PageHandler> page_handler_;
  std::unique_ptr<ConnectorsInternalsPageHandler> handler_;
};

// Tests that GetSignalsReportingState returns an error when the profile is
// null.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetSignalsReportingState_NullProfile) {
  mojo::Remote<connectors_internals::mojom::PageHandler>
      null_profile_page_handler;
  ConnectorsInternalsPageHandler null_profile_handler(
      null_profile_page_handler.BindNewPipeAndPassReceiver(), nullptr);

  base::test::TestFuture<connectors_internals::mojom::SignalsReportingStatePtr>
      future;
  null_profile_page_handler->GetSignalsReportingState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_EQ(state->error_info, "Profile unavailable");
  EXPECT_FALSE(state->status_report_enabled);
  EXPECT_FALSE(state->signals_report_enabled);
  EXPECT_FALSE(state->can_collect_all_fields);
}

// Tests that GetSignalsReportingState returns an error and disabled status
// reporting when the kIOSSignalSharingEnabled feature is disabled.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetSignalsReportingState_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      enterprise_reporting::kIOSSignalSharingEnabled);

  base::test::TestFuture<connectors_internals::mojom::SignalsReportingStatePtr>
      future;
  page_handler_->GetSignalsReportingState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_EQ(state->error_info,
            "User signals reporting is unsupported on the current platform");
  EXPECT_FALSE(state->status_report_enabled);
  EXPECT_FALSE(state->signals_report_enabled);
}

// Tests that GetSignalsReportingState handles the default profile environment
// where reporting services are not initialized, returning an appropriate error.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetSignalsReportingState_FeatureEnabled_ReportingServiceUnavailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_reporting::kIOSSignalSharingEnabled);

  base::test::TestFuture<connectors_internals::mojom::SignalsReportingStatePtr>
      future;
  page_handler_->GetSignalsReportingState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_EQ(state->error_info, "Profile reporting service unavailable");
  EXPECT_FALSE(state->status_report_enabled);
  EXPECT_FALSE(state->signals_report_enabled);
  EXPECT_TRUE(state->can_collect_all_fields);
}

// Tests that GetSignalsReportingState returns an error when the reporting
// service is available but its report scheduler is unavailable.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetSignalsReportingState_FeatureEnabled_ReportSchedulerUnavailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_reporting::kIOSSignalSharingEnabled},
      /*disabled_features=*/{enterprise_reporting::kCloudProfileReporting});

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      enterprise_reporting::CloudProfileReportingServiceFactoryIOS::
          GetInstance(),
      base::BindOnce([](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            enterprise_reporting::CloudProfileReportingServiceIOS>(
            /*profile_id_service=*/nullptr,
            /*url_loader_factory=*/nullptr,
            /*profile_name=*/"",
            /*report_scheduler_delegate=*/nullptr,
            /*signals_aggregator=*/nullptr);
      }));
  std::unique_ptr<TestProfileIOS> test_profile = std::move(builder).Build();

  mojo::Remote<connectors_internals::mojom::PageHandler> test_page_handler;
  ConnectorsInternalsPageHandler test_handler(
      test_page_handler.BindNewPipeAndPassReceiver(), test_profile.get());

  base::test::TestFuture<connectors_internals::mojom::SignalsReportingStatePtr>
      future;
  test_page_handler->GetSignalsReportingState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_EQ(state->error_info, "Profile report scheduler unavailable");
  EXPECT_FALSE(state->status_report_enabled);
  EXPECT_FALSE(state->signals_report_enabled);
  EXPECT_TRUE(state->can_collect_all_fields);
}

// Tests that GetSignalsReportingState correctly retrieves and formats upload
// timestamps and configuration from preferences.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetSignalsReportingState_FeatureEnabled_WithTimestamps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_reporting::kIOSSignalSharingEnabled);

  base::Time attempt_time = base::Time::Now() - base::Hours(2);
  base::Time success_time = base::Time::Now() - base::Hours(1);
  profile_->GetPrefs()->SetTime(
      enterprise_reporting::kLastSignalsUploadAttemptTimestamp, attempt_time);
  profile_->GetPrefs()->SetTime(
      enterprise_reporting::kLastSignalsUploadSucceededTimestamp, success_time);
  profile_->GetPrefs()->SetString(
      enterprise_reporting::kLastSignalsUploadSucceededConfig, "test_config");

  base::test::TestFuture<connectors_internals::mojom::SignalsReportingStatePtr>
      future;
  page_handler_->GetSignalsReportingState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_FALSE(state->last_upload_attempt_timestamp.empty());
  EXPECT_FALSE(state->last_upload_success_timestamp.empty());
  EXPECT_EQ(state->last_signals_upload_config, "test_config");
  EXPECT_TRUE(state->can_collect_all_fields);
}

// Tests that GetDeviceTrustState returns unsupported state when the
// kEnableIOSDeviceTrustConnector feature is disabled.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetDeviceTrustState_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      enterprise_connectors::features::kEnableIOSDeviceTrustConnector);

  base::test::TestFuture<connectors_internals::mojom::DeviceTrustStatePtr>
      future;
  page_handler_->GetDeviceTrustState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_FALSE(state->is_enabled);
  ASSERT_TRUE(state->key_info);
  EXPECT_EQ(
      state->key_info->is_key_manager_initialized,
      connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED);
  EXPECT_TRUE(state->signals_json.empty());
  EXPECT_TRUE(state->policy_enabled_levels.empty());
}

// Tests that GetDeviceTrustState returns unsupported state when the
// DeviceTrustService is null for the profile.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetDeviceTrustState_FeatureEnabled_NoDeviceTrustService) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::features::kEnableIOSDeviceTrustConnector);

  base::test::TestFuture<connectors_internals::mojom::DeviceTrustStatePtr>
      future;
  page_handler_->GetDeviceTrustState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_FALSE(state->is_enabled);
  ASSERT_TRUE(state->key_info);
  EXPECT_EQ(
      state->key_info->is_key_manager_initialized,
      connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED);
}

// Tests that GetDeviceTrustState returns device trust details and formatted
// signals when DeviceTrustService is available and collects signals.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetDeviceTrustState_FeatureEnabled_WithDeviceTrustService) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::features::kEnableIOSDeviceTrustConnector);

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      DeviceTrustServiceFactoryIOS::GetInstance(),
      base::BindOnce([](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
        base::DictValue signals;
        signals.Set("test_signal_key", "test_signal_value");
        return std::make_unique<FakeDeviceTrustService>(
            /*is_enabled=*/true, std::move(signals),
            DeviceTrustConnectorServiceFactoryIOS::GetForProfile(profile));
      }));
  std::unique_ptr<TestProfileIOS> test_profile = std::move(builder).Build();

  mojo::Remote<connectors_internals::mojom::PageHandler> test_page_handler;
  ConnectorsInternalsPageHandler test_handler(
      test_page_handler.BindNewPipeAndPassReceiver(), test_profile.get());

  base::test::TestFuture<connectors_internals::mojom::DeviceTrustStatePtr>
      future;
  test_page_handler->GetDeviceTrustState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_TRUE(state->is_enabled);
  ASSERT_TRUE(state->key_info);
  EXPECT_EQ(
      state->key_info->is_key_manager_initialized,
      connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY);
  EXPECT_NE(
      state->signals_json.find("\"test_signal_key\": \"test_signal_value\""),
      std::string::npos);
}

// Tests that DeleteDeviceTrustKey runs its completion callback.
TEST_F(ConnectorsInternalsPageHandlerTest, DeleteDeviceTrustKey) {
  base::test::TestFuture<void> future;
  page_handler_->DeleteDeviceTrustKey(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

// Tests that GetClientCertificateState returns client certificate state for
// a profile.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetClientCertificateState_WithProfile) {
  base::test::TestFuture<
      connectors_internals::mojom::ClientCertificateStatePtr>
      future;
  page_handler_->GetClientCertificateState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_TRUE(state->policy_enabled_levels.empty());
  EXPECT_TRUE(state->managed_browser_identity.is_null());
  EXPECT_TRUE(state->managed_profile_identity.is_null());
}

// Tests that GetClientCertificateState handles a null profile gracefully.
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetClientCertificateState_NullProfile) {
  mojo::Remote<connectors_internals::mojom::PageHandler>
      null_profile_page_handler;
  ConnectorsInternalsPageHandler null_profile_handler(
      null_profile_page_handler.BindNewPipeAndPassReceiver(), nullptr);

  base::test::TestFuture<
      connectors_internals::mojom::ClientCertificateStatePtr>
      future;
  null_profile_page_handler->GetClientCertificateState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_TRUE(state->policy_enabled_levels.empty());
  EXPECT_TRUE(state->managed_browser_identity.is_null());
  EXPECT_TRUE(state->managed_profile_identity.is_null());
}

// Tests that GetProvisioningDomainState returns an empty list of configs
// since PvD configs are not supported on iOS.
TEST_F(ConnectorsInternalsPageHandlerTest, GetProvisioningDomainState) {
  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future;
  page_handler_->GetProvisioningDomainState(future.GetCallback());
  auto state = future.Take();

  ASSERT_TRUE(state);
  EXPECT_TRUE(state->pvd_configs.empty());
}

}  // namespace

