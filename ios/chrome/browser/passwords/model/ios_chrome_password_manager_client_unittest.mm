// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_manager_client.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/device_reauth/device_authenticator.h"
#import "components/device_reauth/mock_device_authenticator.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_form_manager.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/test/test_sync_service.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/passwords/model/features.h"
#import "ios/chrome/browser/passwords/model/password_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/metrics_utils.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

using ::password_manager::MockPasswordFormManagerForUI;
using ::password_manager::PasswordFormManager;
using ::password_manager::PasswordFormManagerForUI;
using ::password_manager::PasswordManagerClient;
using ::password_manager::prefs::kCredentialsEnableService;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class MockRouter : public enterprise_connectors::ReportingEventRouter {
 public:
  MockRouter(enterprise_connectors::IOSRealtimeReportingClient* client)
      : ReportingEventRouter(client) {}
  MOCK_METHOD(void,
              OnLoginEvent,
              (const GURL& url,
               bool is_federated,
               const url::SchemeHostPort& federated_origin,
               const std::u16string& username),
              (override));

  MOCK_METHOD(
      void,
      OnPasswordBreach,
      (const std::string& trigger,
       (const std::vector<std::pair<GURL, std::u16string>>& identities)),
      (override));
};

std::unique_ptr<KeyedService> MakeMockRouter(ProfileIOS* profile) {
  return std::make_unique<MockRouter>(
      enterprise_connectors::IOSRealtimeReportingClientFactory::GetForProfile(
          profile));
}

// TODO(crbug.com/41456340): this file is initiated because of needing test for
// ios policy. More unit test of the client should be added.
class IOSChromePasswordManagerClientTest : public PlatformTest {
 public:
  IOSChromePasswordManagerClientTest()
      : web_client_(std::make_unique<ChromeWebClient>()),
        store_(new testing::NiceMock<
               password_manager::MockPasswordStoreInterface>()) {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        enterprise_connectors::IOSReportingEventRouterFactory::GetInstance(),
        base::BindOnce(&MakeMockRouter));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();
    reporting_event_router_ = static_cast<MockRouter*>(
        enterprise_connectors::IOSReportingEventRouterFactory::GetForProfile(
            profile_.get()));
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  ~IOSChromePasswordManagerClientTest() override {
    store_->ShutdownOnUIThread();
  }

  void SetUp() override {
    PlatformTest::SetUp();
    ON_CALL(*store_, GetError)
        .WillByDefault(Return(password_manager::ActionableError::kNoError));

    // When waiting for predictions is on, it makes tests more complicated.
    // Disable waiting, since most tests have nothing to do with predictions.
    // All tests that test working with prediction should explicitly turn
    // predictions on.
    PasswordFormManager::set_wait_for_server_predictions_for_filling(false);

    passwordController_ =
        [[PasswordController alloc] initWithWebState:web_state()];
  }

  web::WebState* web_state() { return web_state_.get(); }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<web::WebState> web_state_;
  raw_ptr<MockRouter> reporting_event_router_;

  // PasswordController for testing.
  PasswordController* passwordController_;

  scoped_refptr<password_manager::MockPasswordStoreInterface> store_;
};

// Tests that saving password behaves properly with the
// kCredentialsEnableService pref.
TEST_F(IOSChromePasswordManagerClientTest, PasswordManagerEnabledPolicyTest) {
  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  GURL url = GURL("http://foo.example.com");

  // Password Manager is enabled by default. IsSavingAndFillingEnabled should be
  // true when PasswordManagerEnabled policy is not set.
  EXPECT_TRUE(client->IsSavingAndFillingEnabled(url::Origin::Create(url)));

  // The pref kCredentialsEnableService should be false when disable the policy.
  client->GetPrefs()->SetBoolean(kCredentialsEnableService, false);
  // IsSavingAndFillingEnabled should return false, which means the password
  // won't be saved anymore.
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(url::Origin::Create(url)));

  // The pref kCredentialsEnableService should be true when enable the policy.
  client->GetPrefs()->SetBoolean(kCredentialsEnableService, true);
  // IsSavingAndFillingEnabled should return true, which means the password
  // should be saved.
  EXPECT_TRUE(client->IsSavingAndFillingEnabled(url::Origin::Create(url)));
}

// Tests that `NotifySuccessfulLoginWithExistingPassword` dispatches
// `PromosManagerCommands`.
TEST_F(IOSChromePasswordManagerClientTest,
       NotifySuccessfulLoginWithExistingPasswordTest) {
  // Create a dispatcher for the client, register the command handler for
  // `PromosManagerCommands`
  id promos_manager_commands_handler_mock =
      OCMStrictProtocolMock(@protocol(PromosManagerCommands));

  id dispatcher = [[CommandDispatcher alloc] init];
  [dispatcher startDispatchingToTarget:promos_manager_commands_handler_mock
                           forProtocol:@protocol(PromosManagerCommands)];
  passwordController_.dispatcher = dispatcher;

  // Expect the call with correct trigger type.
  [[promos_manager_commands_handler_mock expect]
      showCredentialProviderPromoWithTrigger:
          CredentialProviderPromoTrigger::SuccessfulLoginUsingExistingPassword];

  // Set up the param for the `NotifySuccessfulLoginWithExistingPassword` call.
  password_manager::PasswordForm form;
  auto manager = std::make_unique<NiceMock<MockPasswordFormManagerForUI>>();
  ON_CALL(*manager, GetPendingCredentials)
      .WillByDefault(testing::ReturnRef(form));
  ON_CALL(*manager, IsMovableToAccountStore).WillByDefault(Return(true));

  // Call the tested function.
  (passwordController_.passwordManagerClient)
      ->NotifySuccessfulLoginWithExistingPassword(std::move(manager));

  // Verify.
  [promos_manager_commands_handler_mock verify];

  passwordController_.dispatcher = nil;
}

// Tests that the AutofillCrowdsourcingManager can be retrieved for PWM when the
// feature is enabled.
TEST_F(IOSChromePasswordManagerClientTest,
       GetAutofillCrowdsourcingManager_Enabled) {
  base::test::ScopedFeatureList scoped_feature_list{
      kPasswordManagerEnableCrowdsourcingUploads};

  InfoBarManagerImpl::CreateForWebState(web_state_.get());
  auto autofill_client = std::make_unique<
      autofill::WithFakedFromWebState<autofill::ChromeAutofillClientIOS>>(
      profile_.get(), web_state_.get(),
      InfoBarManagerImpl::FromWebState(web_state_.get()), nil);

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  ASSERT_TRUE(client->GetAutofillCrowdsourcingManager());

  // Destroy the webstate now so WebStateDestroyed() is called before destroying
  // the autofill client, so the expected teardown order is respected.
  web_state_.reset();
}

// Tests that the AutofillCrowdsourcingManager is not retrieved for PWM when the
// feature is disabled.
TEST_F(IOSChromePasswordManagerClientTest,
       GetAutofillCrowdsourcingManager_Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kPasswordManagerEnableCrowdsourcingUploads);

  InfoBarManagerImpl::CreateForWebState(web_state_.get());
  auto autofill_client = std::make_unique<
      autofill::WithFakedFromWebState<autofill::ChromeAutofillClientIOS>>(
      profile_.get(), web_state_.get(),
      InfoBarManagerImpl::FromWebState(web_state_.get()), nil);

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  ASSERT_FALSE(client->GetAutofillCrowdsourcingManager());

  // Destroy the webstate now so WebStateDestroyed() is called before destroying
  // the autofill client, so the expected teardown order is respected.
  web_state_.reset();
}

// Tests that MaybeReportEnterpriseLoginEvent invoked router->OnLoginEvent as
// expected.
TEST_F(IOSChromePasswordManagerClientTest, OnLogInInvoked) {

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  EXPECT_CALL(*reporting_event_router_, OnLoginEvent(_, _, _, _)).Times(1);
  client->MaybeReportEnterpriseLoginEvent(GURL("https://www.example.com/"),
                                          url::SchemeHostPort().IsValid(),
                                          url::SchemeHostPort(), u"Fakeuser");
}

// Tests that MaybeReportEnterprisePasswordBreachEvent invoked
// router->OnPasswordBreach as expected.
TEST_F(IOSChromePasswordManagerClientTest, OnPasswordBreachInvoked) {

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  std::vector<std::pair<GURL, std::u16string>> expected_data;
  expected_data.emplace_back(GURL("https://first.example.com"),
                             u"first_user_name");
  EXPECT_CALL(*reporting_event_router_,
              OnPasswordBreach(_, testing::Eq(expected_data)))
      .Times(1);
  client->MaybeReportEnterprisePasswordBreachEvent(expected_data);
}

// Tests that `IsReauthBeforeFillingRequired` returns true when the
// authenticator can authenticate with biometric or screen lock.
TEST_F(IOSChromePasswordManagerClientTest,
       IsReauthBeforeFillingRequired_ReauthRequired) {
  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_TRUE(client->IsReauthBeforeFillingRequired(authenticator.get()));
}

// Tests that `IsReauthBeforeFillingRequired` returns false when the
// authenticator cannot authenticate with biometric or screen lock.
TEST_F(IOSChromePasswordManagerClientTest,
       IsReauthBeforeFillingRequired_ReauthNotRequired) {
  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(false));
  EXPECT_FALSE(client->IsReauthBeforeFillingRequired(authenticator.get()));
}

// Tests that `GetDeviceAuthenticator` returns a valid device authenticator.
TEST_F(IOSChromePasswordManagerClientTest, GetDeviceAuthenticator) {
  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
      client->GetDeviceAuthenticator();
  EXPECT_TRUE(authenticator);
}

// Tests that `AutomaticPasswordSave` displays the password saved infobar.
TEST_F(IOSChromePasswordManagerClientTest, AutomaticPasswordSaveTest) {
  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile_.get()));
  CoreAccountInfo account_info;
  account_info.email = "user@example.com";
  sync_service->SetSignedIn(signin::ConsentLevel::kSignin, account_info);

  InfoBarManagerImpl::CreateForWebState(web_state());
  PasswordManagerClient* client = passwordController_.passwordManagerClient;

  password_manager::PasswordForm form;
  auto mock_form_manager =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();
  EXPECT_CALL(*mock_form_manager, GetPendingCredentials())
      .WillOnce(ReturnRef(form));

  client->AutomaticPasswordSave(std::move(mock_form_manager),
                                /*is_update_confirmation=*/false);

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  ASSERT_EQ(infobar_manager->infobars().size(), 1u);
  EXPECT_EQ(infobar_manager->infobars()[0]->delegate()->GetIdentifier(),
            infobars::InfoBarDelegate::PASSWORD_SAVED_INFOBAR_DELEGATE_IOS);
}

// Tests that `AutomaticPasswordSave` does not display the password saved
// infobar when `kPasswordSavedInfobar` is disabled.
TEST_F(IOSChromePasswordManagerClientTest, AutomaticPasswordSaveTest_Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kPasswordSavedInfobar);

  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile_.get()));
  CoreAccountInfo account_info;
  account_info.email = "user@example.com";
  sync_service->SetSignedIn(signin::ConsentLevel::kSignin, account_info);

  InfoBarManagerImpl::CreateForWebState(web_state());
  PasswordManagerClient* client = passwordController_.passwordManagerClient;

  client->AutomaticPasswordSave(nullptr, /*is_update_confirmation=*/false);

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);
}

// Tests that `AutomaticPasswordSave` does not display the password saved
// infobar when the user is signed out.
TEST_F(IOSChromePasswordManagerClientTest,
       AutomaticPasswordSaveTest_SignedOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPasswordSavedInfobar);

  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile_.get()));
  sync_service->SetSignedOut();

  InfoBarManagerImpl::CreateForWebState(web_state());
  PasswordManagerClient* client = passwordController_.passwordManagerClient;

  client->AutomaticPasswordSave(nullptr, /*is_update_confirmation=*/false);

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);
}

// Tests that `NotifyOnSuccessfulLogin` records Touch to Fill submission metrics
// when submission is successful within the time threshold.
TEST_F(IOSChromePasswordManagerClientTest,
       TestTouchToFillSuccessfulSubmissionWasObserved) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  const std::u16string username = u"test_user";

  client->StartSubmissionTrackingAfterTouchToFill(username);
  task_environment_.FastForwardBy(base::Seconds(5));
  client->NotifyOnSuccessfulLogin(username);

  histogram_tester.ExpectUniqueSample(
      kTouchToFillSuccessfulSubmissionWasObservedHistogram, true, 1);
  histogram_tester.ExpectUniqueTimeSample(
      kTouchToFillTimeToSuccessfulLoginHistogram, base::Seconds(5), 1);

  const auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::TouchToFill_TimeToSuccessfulLogin::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::TouchToFill_TimeToSuccessfulLogin::
          kTimeToSuccessfulLoginName,
      ukm::GetExponentialBucketMinForUserTiming(5000));

  // Subsequent reset should not record duplicate metrics.
  client->ResetSubmissionTrackingAfterTouchToFill();
  histogram_tester.ExpectTotalCount(
      kTouchToFillSuccessfulSubmissionWasObservedHistogram, 1);
}

// Tests that `ResetSubmissionTrackingAfterTouchToFill` records false for
// `SuccessfulSubmissionWasObserved` and resets tracking.
TEST_F(IOSChromePasswordManagerClientTest,
       TestTouchToFillResetSubmissionTracking) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  const std::u16string username = u"test_user";

  client->StartSubmissionTrackingAfterTouchToFill(username);
  client->ResetSubmissionTrackingAfterTouchToFill();

  histogram_tester.ExpectUniqueSample(
      kTouchToFillSuccessfulSubmissionWasObservedHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kTouchToFillTimeToSuccessfulLoginHistogram,
                                    0);
  EXPECT_TRUE(
      test_ukm_recorder
          .GetEntriesByName(
              ukm::builders::TouchToFill_TimeToSuccessfulLogin::kEntryName)
          .empty());

  // Subsequent reset does not record again.
  client->ResetSubmissionTrackingAfterTouchToFill();
  histogram_tester.ExpectTotalCount(
      kTouchToFillSuccessfulSubmissionWasObservedHistogram, 1);
}

// Tests that `NotifyOnSuccessfulLogin` resets tracking when submitted username
// does not match the filled username.
TEST_F(IOSChromePasswordManagerClientTest, TestTouchToFillUsernameMismatch) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  client->StartSubmissionTrackingAfterTouchToFill(u"filled_user");
  client->NotifyOnSuccessfulLogin(u"different_user");

  histogram_tester.ExpectUniqueSample(
      kTouchToFillSuccessfulSubmissionWasObservedHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kTouchToFillTimeToSuccessfulLoginHistogram,
                                    0);
  EXPECT_TRUE(
      test_ukm_recorder
          .GetEntriesByName(
              ukm::builders::TouchToFill_TimeToSuccessfulLogin::kEntryName)
          .empty());
}

// Tests that `NotifyOnSuccessfulLogin` treats login after more than 1 minute
// as unrelated and records false for `SuccessfulSubmissionWasObserved`.
TEST_F(IOSChromePasswordManagerClientTest, TestTouchToFillSubmissionTimeout) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  const std::u16string username = u"test_user";

  client->StartSubmissionTrackingAfterTouchToFill(username);
  task_environment_.FastForwardBy(base::Minutes(2));
  client->NotifyOnSuccessfulLogin(username);

  histogram_tester.ExpectUniqueSample(
      kTouchToFillSuccessfulSubmissionWasObservedHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kTouchToFillTimeToSuccessfulLoginHistogram,
                                    0);
  EXPECT_TRUE(
      test_ukm_recorder
          .GetEntriesByName(
              ukm::builders::TouchToFill_TimeToSuccessfulLogin::kEntryName)
          .empty());
}

// Tests that `NotifyOnSuccessfulLogin` does nothing when Touch to Fill tracking
// was never started.
TEST_F(IOSChromePasswordManagerClientTest, TestTouchToFillNoTracking) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  client->NotifyOnSuccessfulLogin(u"some_user");

  histogram_tester.ExpectTotalCount(
      kTouchToFillSuccessfulSubmissionWasObservedHistogram, 0);
  histogram_tester.ExpectTotalCount(kTouchToFillTimeToSuccessfulLoginHistogram,
                                    0);
  EXPECT_TRUE(
      test_ukm_recorder
          .GetEntriesByName(
              ukm::builders::TouchToFill_TimeToSuccessfulLogin::kEntryName)
          .empty());
}

// Tests that starting Touch to Fill tracking while a session is already pending
// overwrites the previous session without logging, matching Android.
TEST_F(IOSChromePasswordManagerClientTest, TestTouchToFillConsecutiveFills) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  client->StartSubmissionTrackingAfterTouchToFill(u"user_1");

  // Second fill before submission of user_1 overwrites tracking.
  client->StartSubmissionTrackingAfterTouchToFill(u"user_2");
  histogram_tester.ExpectTotalCount(
      kTouchToFillSuccessfulSubmissionWasObservedHistogram, 0);
  histogram_tester.ExpectTotalCount(kTouchToFillTimeToSuccessfulLoginHistogram,
                                    0);

  // Submitting user_2 should now succeed and record true.
  task_environment_.FastForwardBy(base::Seconds(3));
  client->NotifyOnSuccessfulLogin(u"user_2");

  histogram_tester.ExpectUniqueSample(
      kTouchToFillSuccessfulSubmissionWasObservedHistogram, true, 1);
  histogram_tester.ExpectUniqueTimeSample(
      kTouchToFillTimeToSuccessfulLoginHistogram, base::Seconds(3), 1);

  const auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::TouchToFill_TimeToSuccessfulLogin::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::TouchToFill_TimeToSuccessfulLogin::
          kTimeToSuccessfulLoginName,
      ukm::GetExponentialBucketMinForUserTiming(3000));
}
