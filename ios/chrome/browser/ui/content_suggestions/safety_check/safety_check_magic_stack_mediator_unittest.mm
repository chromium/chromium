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
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Tests the SafetyCheckMagicStackMediator.
class SafetyCheckMagicStackMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PrefRegistrySimple* registry = pref_service_->registry();

    registry->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, false);
    registry->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, false);

    local_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PrefRegistrySimple* local_registry = local_pref_service_->registry();

    local_registry->RegisterTimePref(prefs::kIosSafetyCheckManagerLastRunTime,
                                     base::Time(), PrefRegistry::LOSSY_PREF);
    local_registry->RegisterStringPref(
        prefs::kIosSafetyCheckManagerPasswordCheckResult,
        NameForSafetyCheckState(PasswordSafetyCheckState::kDefault),
        PrefRegistry::LOSSY_PREF);
    local_registry->RegisterStringPref(
        prefs::kIosSafetyCheckManagerUpdateCheckResult,
        NameForSafetyCheckState(UpdateChromeSafetyCheckState::kDefault),
        PrefRegistry::LOSSY_PREF);
    local_registry->RegisterStringPref(
        prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult,
        NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kDefault),
        PrefRegistry::LOSSY_PREF);
    local_registry->RegisterIntegerPref(
        prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
        -1);
    local_registry->RegisterBooleanPref(
        safety_check_prefs::kSafetyCheckInMagicStackDisabledPref, false);
    local_registry->RegisterTimePref(prefs::kIosSettingsSafetyCheckLastRunTime,
                                     base::Time());
    local_registry->RegisterDictionaryPref(
        prefs::kIosSafetyCheckManagerInsecurePasswordCounts,
        PrefRegistry::LOSSY_PREF);

    TestChromeBrowserState::Builder builder;

    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    browser_state_ = builder.Build();

    TestingApplicationContext::GetGlobal()->SetLocalState(
        local_pref_service_.get());

    password_check_manager_ =
        IOSChromePasswordCheckManagerFactory::GetForBrowserState(
            browser_state_.get());

    safety_check_manager_ = std::make_unique<IOSChromeSafetyCheckManager>(
        pref_service_.get(), local_pref_service_.get(), password_check_manager_,
        base::SequencedTaskRunner::GetCurrentDefault());

    mock_app_state_ = OCMClassMock([AppState class]);

    consumer_ = OCMProtocolMock(@protocol(ContentSuggestionsConsumer));

    mediator_ = [[SafetyCheckMagicStackMediator alloc]
        initWithSafetyCheckManager:safety_check_manager_.get()
                        localState:local_pref_service_.get()
                          appState:mock_app_state_];

    mediator_.consumer = consumer_;
  }

  void TearDown() override {
    safety_check_manager_->StopSafetyCheck();
    safety_check_manager_->Shutdown();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  id mock_app_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_pref_service_;
  id consumer_;
  SafetyCheckMagicStackMediator* mediator_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_manager_;
  std::unique_ptr<IOSChromeSafetyCheckManager> safety_check_manager_;
};

// Tests the mediator's consumer is called with the correct Safety Check state
// when the Safety Check is run.
TEST_F(SafetyCheckMagicStackMediatorTest, CallsConsumerWithRunningState) {
  SafetyCheckState* expected = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kRunning
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kUnsafe
                   runningState:RunningSafetyCheckState::kRunning];

  OCMExpect([consumer_
      showSafetyCheck:[OCMArg checkWithBlock:^BOOL(SafetyCheckState* state) {
        return state.updateChromeState == expected.updateChromeState &&
               state.passwordState == expected.passwordState &&
               state.safeBrowsingState == expected.safeBrowsingState &&
               state.runningState == expected.runningState;
      }]]);

  safety_check_manager_->StartSafetyCheck();

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests the mediator's consumer is not called when the password state changes,
// i.e. consumer is called only when the running state changes.
TEST_F(SafetyCheckMagicStackMediatorTest,
       DoesNotCallConsumerWithPasswordStateChange) {
  OCMReject([consumer_ showSafetyCheck:[OCMArg any]]);

  safety_check_manager_->PasswordCheckStatusChanged(
      PasswordCheckState::kQuotaLimit);

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests the mediator's consumer is not called when the Insecure Credentials
// list changes, i.e. consumer is called only when the running state changes.
TEST_F(SafetyCheckMagicStackMediatorTest,
       DoesNotCallConsumerWithInsecureCredentialsChange) {
  OCMReject([consumer_ showSafetyCheck:[OCMArg any]]);

  safety_check_manager_->InsecureCredentialsChanged();

  EXPECT_OCMOCK_VERIFY(consumer_);
}
