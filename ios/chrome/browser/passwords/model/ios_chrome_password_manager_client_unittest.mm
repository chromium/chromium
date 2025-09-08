// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_manager_client.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_form_manager.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/testing_pref_service.h"
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
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

using password_manager::MockPasswordFormManagerForUI;
using password_manager::PasswordFormManager;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordManagerClient;
using password_manager::prefs::kCredentialsEnableService;
using ::testing::_;
using testing::NiceMock;
using testing::Return;

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
    ON_CALL(*store_, IsAbleToSavePasswords).WillByDefault(Return(true));

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
  web::WebTaskEnvironment task_environment_;
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
  EXPECT_TRUE(client->IsSavingAndFillingEnabled(url));

  // The pref kCredentialsEnableService should be false when disable the policy.
  client->GetPrefs()->SetBoolean(kCredentialsEnableService, false);
  // IsSavingAndFillingEnabled should return false, which means the password
  // won't be saved anymore.
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(url));

  // The pref kCredentialsEnableService should be true when enable the policy.
  client->GetPrefs()->SetBoolean(kCredentialsEnableService, true);
  // IsSavingAndFillingEnabled should return true, which means the password
  // should be saved.
  EXPECT_TRUE(client->IsSavingAndFillingEnabled(url));
}

// Tests that `NotifySuccessfulLoginWithExistingPassword` dispatches
// `CredentialProviderPromoCommands`.
TEST_F(IOSChromePasswordManagerClientTest,
       NotifySuccessfulLoginWithExistingPasswordTest) {
  // Create a dispatcher for the client, register the command handler for
  // `CredentialProviderPromoCommands`
  id credential_provider_promo_commands_handler_mock =
      OCMStrictProtocolMock(@protocol(CredentialProviderPromoCommands));

  id dispatcher = [[CommandDispatcher alloc] init];
  [dispatcher
      startDispatchingToTarget:credential_provider_promo_commands_handler_mock
                   forProtocol:@protocol(CredentialProviderPromoCommands)];
  passwordController_.dispatcher = dispatcher;

  // Expect the call with correct trigger type.
  [[credential_provider_promo_commands_handler_mock expect]
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
  [credential_provider_promo_commands_handler_mock verify];

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
