// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/post_restore_profile_agent.h"

#import "base/memory/raw_ptr.h"
#import "base/values.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using ::testing::_;
using testing::NiceMock;

namespace {

const char kFakePreRestoreAccountEmail[] = "person@example.org";

// Creates a mock PromosManager.
std::unique_ptr<KeyedService> CreateMockPromosManager(
    web::BrowserState* context) {
  return std::make_unique<NiceMock<MockPromosManager>>();
}

}  // namespace

// Tests the PostRestoreProfileAgent.
class PostRestoreProfileAgentTest : public PlatformTest {
 public:
  explicit PostRestoreProfileAgentTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(PromosManagerFactory::GetInstance(),
                              base::BindOnce(&CreateMockPromosManager));
    profile_ = std::move(builder).Build();

    promos_manager_ = static_cast<NiceMock<MockPromosManager>*>(
        PromosManagerFactory::GetForProfile(profile_.get()));
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();

    profile_agent_ = [[PostRestoreProfileAgent alloc] init];
    [profile_state_ addAgent:profile_agent_];
  }

  void TriggerProfileStateChange() {
    [profile_agent_ profileState:profile_state_
        didTransitionToInitStage:ProfileInitStage::kProfileLoaded
                   fromInitStage:ProfileInitStage::kLoadProfile];

    [profile_agent_ profileState:profile_state_
        didTransitionToInitStage:ProfileInitStage::kFinal
                   fromInitStage:ProfileInitStage::kNormalUI];
  }

  void SetFakePreRestoreAccountInfo() {
    AccountInfo accountInfo;
    accountInfo.email = kFakePreRestoreAccountEmail;
    StorePreRestoreIdentity(pref_service(), accountInfo,
                            /*history_sync_enabled=*/false);
  }

  // Signs in a fake identity.
  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);
    auth_service_->SignIn(fake_identity,
                          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  PrefService* pref_service() { return profile_.get()->GetPrefs(); }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<NiceMock<MockPromosManager>> promos_manager_;
  raw_ptr<AuthenticationService> auth_service_;
  PostRestoreProfileAgent* profile_agent_;
  ProfileState* profile_state_;
};

// Tests the logic of whether a promo should be registered when there is no pre
// restore info.
TEST_F(PostRestoreProfileAgentTest, MaybeRegisterPromo) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForContinuousDisplay(_))
      .Times(0);

  // Scenario which should not register a promo.
  ClearPreRestoreIdentity(pref_service());
  TriggerProfileStateChange();
}

// Tests that the alert promo is registered.
TEST_F(PostRestoreProfileAgentTest, RegisterPromoAlert) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(
                  promos_manager::Promo::PostRestoreSignInAlert))
      .Times(1);

  SetFakePreRestoreAccountInfo();
  TriggerProfileStateChange();
}

// Tests that the reauth prompt is disabled.
TEST_F(PostRestoreProfileAgentTest, RegisterPromoDisablesReauthPrompt) {
  SetFakePreRestoreAccountInfo();
  auth_service_->SetReauthPromptForSignInAndSync();
  EXPECT_TRUE(auth_service_->ShouldReauthPromptForSignInAndSync());
  TriggerProfileStateChange();
  EXPECT_FALSE(auth_service_->ShouldReauthPromptForSignInAndSync());
}

// Tests that the alert promo is deregistered with the right conditions.
TEST_F(PostRestoreProfileAgentTest, DeregisterPromoAlert) {
  EXPECT_CALL(*promos_manager_.get(), DeregisterPromo(_)).Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::PostRestoreSignInAlert))
      .Times(1);

  SetFakePreRestoreAccountInfo();
  ClearPreRestoreIdentity(pref_service());
  TriggerProfileStateChange();
}

// Tests that if a signin occurs, the promo is deregistered.
TEST_F(PostRestoreProfileAgentTest, DeregisterPromoOnSignin) {
  SetFakePreRestoreAccountInfo();
  TriggerProfileStateChange();

  EXPECT_CALL(*promos_manager_.get(), DeregisterPromo(_)).Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::PostRestoreSignInAlert))
      .Times(1);

  SignIn();
}
