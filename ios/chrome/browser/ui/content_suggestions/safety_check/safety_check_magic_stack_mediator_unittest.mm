// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_mediator.h"

#import "base/test/bind.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface SafetyCheckMagicStackMediator (Testing)

- (void)addConsumer:(id<SafetyCheckMagicStackConsumer>)consumer;

@end

// Tests the SafetyCheckMagicStackMediator.
class SafetyCheckMagicStackMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    TestProfileIOS::Builder builder;

    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    ProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(std::move(builder));

    pref_service_ = profile->GetPrefs();

    local_pref_service_ =
        TestingApplicationContext::GetGlobal()->GetLocalState();

    safety_check_manager_ =
        IOSChromeSafetyCheckManagerFactory::GetForProfile(profile);

    mock_app_state_ = OCMClassMock([AppState class]);

    mediator_ = [[SafetyCheckMagicStackMediator alloc]
        initWithSafetyCheckManager:safety_check_manager_.get()
                        localState:local_pref_service_.get()
                         userState:pref_service_.get()
                          appState:mock_app_state_];

    safety_check_magic_stack_consumer_ =
        OCMProtocolMock(@protocol(SafetyCheckMagicStackConsumer));
    [mediator_ addConsumer:safety_check_magic_stack_consumer_];
  }

  void TearDown() override {
    safety_check_manager_->StopSafetyCheck();
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  id mock_app_state_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<PrefService> local_pref_service_;
  SafetyCheckMagicStackMediator* mediator_;
  id safety_check_magic_stack_consumer_;
  raw_ptr<IOSChromeSafetyCheckManager> safety_check_manager_;
};

// Tests the mediator's consumer is called with the correct Safety Check state
// when the Safety Check is run.
TEST_F(SafetyCheckMagicStackMediatorTest, CallsConsumerWithRunningState) {
  SafetyCheckState* expected = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kRunning
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kSafe
                   runningState:RunningSafetyCheckState::kRunning];

  OCMExpect([safety_check_magic_stack_consumer_
      safetyCheckStateDidChange:[OCMArg checkWithBlock:^BOOL(
                                            SafetyCheckState* state) {
        return state.updateChromeState == expected.updateChromeState &&
               state.passwordState == expected.passwordState &&
               state.safeBrowsingState == expected.safeBrowsingState &&
               state.runningState == expected.runningState;
      }]]);

  safety_check_manager_->StartSafetyCheck();

  EXPECT_OCMOCK_VERIFY(safety_check_magic_stack_consumer_);
}

// Tests the mediator's consumer is not called when the password state changes,
// i.e. consumer is called only when the running state changes.
TEST_F(SafetyCheckMagicStackMediatorTest,
       DoesNotCallConsumerWithPasswordStateChange) {
  OCMReject([safety_check_magic_stack_consumer_
      safetyCheckStateDidChange:[OCMArg any]]);

  safety_check_manager_->PasswordCheckStatusChanged(
      PasswordCheckState::kQuotaLimit);

  EXPECT_OCMOCK_VERIFY(safety_check_magic_stack_consumer_);
}

// Tests the mediator's consumer is not called when the Insecure Credentials
// list changes, i.e. consumer is called only when the running state changes.
TEST_F(SafetyCheckMagicStackMediatorTest,
       DoesNotCallConsumerWithInsecureCredentialsChange) {
  OCMReject([safety_check_magic_stack_consumer_
      safetyCheckStateDidChange:[OCMArg any]]);

  safety_check_manager_->InsecureCredentialsChanged();

  EXPECT_OCMOCK_VERIFY(safety_check_magic_stack_consumer_);
}
