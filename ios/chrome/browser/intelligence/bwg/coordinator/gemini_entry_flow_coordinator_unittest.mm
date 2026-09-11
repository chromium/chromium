// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_entry_flow_coordinator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

class GeminiEntryFlowCoordinatorTest : public PlatformTest {
 public:
  GeminiEntryFlowCoordinatorTest() {
    feature_list_.InitAndEnableFeature(kPageActionMenu);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        GeminiServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeGeminiService>();
            }));

    profile_ = std::move(builder).Build();
    fake_gemini_service_ = static_cast<FakeGeminiService*>(
        GeminiServiceFactory::GetForProfile(profile_.get()));
    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());

    browser_ = std::make_unique<TestBrowser>(profile_.get());

    root_view_controller_ = [[UIViewController alloc] init];
    scoped_key_window_.Get().rootViewController = root_view_controller_;

    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state_->SetBrowserState(profile_.get());
    web_state_->WasShown();
    web_state_->SetCurrentURL(GURL("https://www.google.com"));
    GeminiTabHelper::CreateForWebState(web_state_);
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  ~GeminiEntryFlowCoordinatorTest() override { [coordinator_ stop]; }

  void SignIn(id<SystemIdentity> identity) {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    signin::AccountAvailabilityOptionsBuilder builder;
    builder.WithGaiaId(identity.gaiaId)
        .AsPrimary(signin::ConsentLevel::kSignin);
    signin::MakeAccountAvailable(
        identity_manager,
        builder.Build(base::SysNSStringToUTF8(identity.userEmail)));

    if ([identity.userEmail hasSuffix:@"@google.com"] ||
        [identity.userEmail hasSuffix:@"@foo.com"]) {
      CoreAccountInfo account_info =
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
      signin::SimulateSuccessfulFetchOfAccountInfo(
          identity_manager, account_info.account_id,
          base::SysNSStringToUTF8(identity.userEmail), identity.gaiaId,
          "foo.com", "Full Name", "Given Name", "en", "http://picture");
    }

    auth_service_->SignIn(identity, signin_metrics::AccessPoint::kStartPage);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FakeGeminiService> fake_gemini_service_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<web::FakeWebState> web_state_;
  UIViewController* root_view_controller_;
  ScopedKeyWindow scoped_key_window_;
  id mock_snackbar_handler_;
  GeminiEntryFlowCoordinator* coordinator_;
  GeminiEntryFlowResult flow_result_ = kGeminiEntryFlowResultUnknown;
  bool flow_completed_ = false;
};

// Tests that when policy check is pending during cold start, evaluation awaits
// policy check completion before routing.
TEST_F(GeminiEntryFlowCoordinatorTest, ColdStartPolicyCheckPending) {
  SignIn([FakeSystemIdentity fakeIdentity1]);
  fake_gemini_service_->SetIsEligible(true);
  fake_gemini_service_->SetWorkspacePolicyCheckPending(true);

  GeminiStartupState* startup_state = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::ExternalAppStoreEvent];

  coordinator_ = [[GeminiEntryFlowCoordinator alloc]
      initWithBaseViewController:root_view_controller_
                         browser:browser_.get()
                    startupState:startup_state
        showSnackbarOnCompletion:YES
                      completion:^(GeminiEntryFlowResult result) {
                        flow_result_ = result;
                        flow_completed_ = true;
                      }];

  [coordinator_ start];

  // While policy check is pending, flow has not completed yet.
  EXPECT_FALSE(flow_completed_);

  // Simulate policy check completion.
  fake_gemini_service_->SetWorkspacePolicyCheckPending(false);
  fake_gemini_service_->SetIsEligible(true);

  // Evaluation completes with success once policy check completes.
  EXPECT_TRUE(flow_completed_);
  EXPECT_EQ(kGeminiEntryFlowResultSuccess, flow_result_);
}

// Tests that a signed-in eligible user completes the entry flow successfully.
TEST_F(GeminiEntryFlowCoordinatorTest, ColdStartSignedInEligible) {
  SignIn([FakeSystemIdentity fakeIdentity1]);
  fake_gemini_service_->SetIsEligible(true);
  fake_gemini_service_->SetWorkspacePolicyCheckPending(false);

  GeminiStartupState* startup_state = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::ExternalAppStoreEvent];

  coordinator_ = [[GeminiEntryFlowCoordinator alloc]
      initWithBaseViewController:root_view_controller_
                         browser:browser_.get()
                    startupState:startup_state
        showSnackbarOnCompletion:YES
                      completion:^(GeminiEntryFlowResult result) {
                        flow_result_ = result;
                        flow_completed_ = true;
                      }];

  [coordinator_ start];

  EXPECT_TRUE(flow_completed_);
  EXPECT_EQ(kGeminiEntryFlowResultSuccess, flow_result_);
}

// Tests that an account ineligible due to enterprise policy completes with
// kGeminiEntryFlowResultAccountIneligibleByEnterprise.
TEST_F(GeminiEntryFlowCoordinatorTest, ColdStartAccountIneligibleEnterprise) {
  SignIn([FakeSystemIdentity fakeIdentity1]);
  fake_gemini_service_->SetWorkspacePolicyCheckPending(false);

  gemini::IneligibilityReasons reasons;
  reasons.chrome_enterprise = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);

  GeminiStartupState* startup_state = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::ExternalAppStoreEvent];

  coordinator_ = [[GeminiEntryFlowCoordinator alloc]
      initWithBaseViewController:root_view_controller_
                         browser:browser_.get()
                    startupState:startup_state
        showSnackbarOnCompletion:YES
                      completion:^(GeminiEntryFlowResult result) {
                        flow_result_ = result;
                        flow_completed_ = true;
                      }];

  [coordinator_ start];

  EXPECT_TRUE(flow_completed_);
  EXPECT_EQ(kGeminiEntryFlowResultAccountIneligibleByEnterprise, flow_result_);
}

// Tests that an account ineligible due to general capability capability
// completes with kGeminiEntryFlowResultUnknown.
TEST_F(GeminiEntryFlowCoordinatorTest, ColdStartAccountIneligibleNormal) {
  SignIn([FakeSystemIdentity fakeIdentity1]);
  fake_gemini_service_->SetWorkspacePolicyCheckPending(false);

  gemini::IneligibilityReasons reasons;
  reasons.account_capability = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);

  GeminiStartupState* startup_state = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::ExternalAppStoreEvent];

  coordinator_ = [[GeminiEntryFlowCoordinator alloc]
      initWithBaseViewController:root_view_controller_
                         browser:browser_.get()
                    startupState:startup_state
        showSnackbarOnCompletion:YES
                      completion:^(GeminiEntryFlowResult result) {
                        flow_result_ = result;
                        flow_completed_ = true;
                      }];

  [coordinator_ start];

  EXPECT_TRUE(flow_completed_);
  EXPECT_EQ(kGeminiEntryFlowResultUnknown, flow_result_);
}

}  // namespace
