// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"

#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/password_checkup_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_consumer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using password_manager::InsecurePasswordCounts;
using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

// Creates a saved password form.
PasswordForm CreatePasswordForm() {
  PasswordForm form;
  form.username_value = u"test@egmail.com";
  form.password_value = u"test";
  form.signon_realm = "http://www.example.com/";
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

void AddIssueToForm(PasswordForm* form,
                    InsecureType insecure_type = InsecureType::kLeaked,
                    bool is_muted = false) {
  form->password_issues.insert_or_assign(
      insecure_type, password_manager::InsecurityMetadata(
                         base::Time::Now(), password_manager::IsMuted(is_muted),
                         password_manager::TriggerBackendNotification(false)));
}

}  // namespace

// Test fixture for testing PasswordCheckupMediator class.
class PasswordCheckupMediatorTest : public PlatformTest {
 protected:
  PasswordCheckupMediatorTest() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindRepeating(base::BindLambdaForTesting([](web::BrowserState*) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<password_manager::FakeAffiliationService>());
        })));

    browser_state_ = builder.Build();

    password_check_ = IOSChromePasswordCheckManagerFactory::GetForBrowserState(
        browser_state_.get());

    consumer_ = OCMProtocolMock(@protocol(PasswordCheckupConsumer));

    mediator_ = [[PasswordCheckupMediator alloc]
        initWithPasswordCheckManager:password_check_];
    mediator_.consumer = consumer_;

    // Enable Password Grouping feature to get the affiliated groups.
    feature_list.InitAndEnableFeature(
        password_manager::features::kPasswordsGrouping);
  }

  PasswordCheckupMediator* mediator() { return mediator_; }

  ChromeBrowserState* browserState() { return browser_state_.get(); }

  id consumer() { return consumer_; }

  TestPasswordStore& GetTestStore() {
    return *static_cast<TestPasswordStore*>(
        IOSChromePasswordStoreFactory::GetForBrowserState(
            browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_;
  id consumer_;
  PasswordCheckupMediator* mediator_;
  base::test::ScopedFeatureList feature_list;
};

// Tests that the consumer is correclty notified when the password check state
// changed.
TEST_F(PasswordCheckupMediatorTest,
       NotifiesConsumerOnPasswordCheckStateChange) {
  EXPECT_TRUE([mediator() conformsToProtocol:@protocol(PasswordCheckObserver)]);

  InsecurePasswordCounts counts{};

  OCMExpect([consumer()
         setPasswordCheckupHomepageState:PasswordCheckupHomepageState::
                                             PasswordCheckupHomepageStateDone
                  insecurePasswordCounts:counts
      formattedElapsedTimeSinceLastCheck:@"Check never run."]);
  OCMExpect([consumer() setAffiliatedGroupCount:0]);

  PasswordCheckupMediator<PasswordCheckObserver>* password_check_observer =
      static_cast<PasswordCheckupMediator<PasswordCheckObserver>*>(mediator());

  [password_check_observer
      passwordCheckStateDidChange:PasswordCheckState::kIdle];

  EXPECT_OCMOCK_VERIFY(consumer());
}

// Tests that the consumer is correctly notified when the saved insecure
// passwords changed.
TEST_F(PasswordCheckupMediatorTest, NotifiesConsumerOnInsecurePasswordChange) {
  InsecurePasswordCounts counts = {1, 0, 0, 1};

  OCMExpect([consumer()
         setPasswordCheckupHomepageState:PasswordCheckupHomepageState::
                                             PasswordCheckupHomepageStateDone
                  insecurePasswordCounts:counts
      formattedElapsedTimeSinceLastCheck:@"Check never run."]);
  OCMExpect([consumer() setAffiliatedGroupCount:1]);

  PasswordCheckupMediator<PasswordCheckObserver>* password_check_observer =
      static_cast<PasswordCheckupMediator<PasswordCheckObserver>*>(mediator());

  [password_check_observer
      passwordCheckStateDidChange:PasswordCheckState::kIdle];

  PasswordForm form = CreatePasswordForm();
  AddIssueToForm(&form, InsecureType::kLeaked);
  AddIssueToForm(&form, InsecureType::kWeak);
  GetTestStore().AddLogin(form);
  RunUntilIdle();

  EXPECT_OCMOCK_VERIFY(consumer());
}

// Tests that the consumer is notified with the results of the last successful
// password check when the current password check fails.
TEST_F(PasswordCheckupMediatorTest,
       NotifiesConsumerAfterPasswordCheckupFailed) {
  PasswordCheckupMediator<PasswordCheckObserver>* password_check_observer =
      static_cast<PasswordCheckupMediator<PasswordCheckObserver>*>(mediator());

  [password_check_observer
      passwordCheckStateDidChange:PasswordCheckState::kIdle];

  // Add a leaked password. This password should be taken into account in the
  // `insecurePasswordCounts` passed to the conusmer.
  PasswordForm form1 = CreatePasswordForm();
  AddIssueToForm(&form1, InsecureType::kLeaked);
  GetTestStore().AddLogin(form1);
  RunUntilIdle();

  InsecurePasswordCounts counts = {1, 0, 0, 0};

  OCMExpect([consumer()
         setPasswordCheckupHomepageState:PasswordCheckupHomepageState::
                                             PasswordCheckupHomepageStateDone
                  insecurePasswordCounts:counts
      formattedElapsedTimeSinceLastCheck:@"Check never run."]);
  OCMExpect([consumer() setAffiliatedGroupCount:1]);

  // Enter an error state of PasswordCheckState.
  [password_check_observer
      passwordCheckStateDidChange:PasswordCheckState::kSignedOut];

  // Add a weak password. This password shouldn't be taken into account in the
  // `insecurePasswordCounts` passed to the conusmer since an error occured.
  PasswordForm form2 = CreatePasswordForm();
  AddIssueToForm(&form2, InsecureType::kWeak);
  GetTestStore().AddLogin(form2);
  RunUntilIdle();

  EXPECT_OCMOCK_VERIFY(consumer());
}
