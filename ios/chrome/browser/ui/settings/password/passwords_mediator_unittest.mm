// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using password_manager::PasswordForm;
using password_manager::InsecureType;
using password_manager::TestPasswordStore;

// Sets test password store and returns pointer to it.
scoped_refptr<TestPasswordStore> BuildTestPasswordStore(
    ChromeBrowserState* _browserState) {
  return base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
      IOSChromePasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              _browserState,
              base::BindRepeating(&password_manager::BuildPasswordStore<
                                  web::BrowserState, TestPasswordStore>))
          .get()));
}

// Creates a saved password form.
PasswordForm CreatePasswordForm() {
  PasswordForm form;
  form.username_value = u"test@egmail.com";
  form.password_value = u"test";
  form.signon_realm = "http://www.example.com/";
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

@interface FakePasswordsConsumer : NSObject <PasswordsConsumer> {
  std::vector<password_manager::PasswordForm> _savedForms;
  std::vector<password_manager::PasswordForm> _blockedForms;
}

@end

@implementation FakePasswordsConsumer

- (void)setPasswordCheckUIState:(PasswordCheckUIState)state
      compromisedPasswordsCount:(NSInteger)count {
}

- (void)setPasswordsForms:
            (std::vector<password_manager::PasswordForm>)savedForms
             blockedForms:
                 (std::vector<password_manager::PasswordForm>)blockedForms {
  _savedForms = savedForms;
  _blockedForms = blockedForms;
}

- (std::vector<password_manager::PasswordForm>)savedForms {
  return _savedForms;
}

@end

// Tests for Password Issues mediator.
class PasswordsMediatorTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    browser_state_ = builder.Build();

    store_ = BuildTestPasswordStore(browser_state_.get());

    password_check_ = IOSChromePasswordCheckManagerFactory::GetForBrowserState(
        browser_state_.get());

    consumer_ = [[FakePasswordsConsumer alloc] init];

    mediator_ =
        [[PasswordsMediator alloc] initWithPasswordCheckManager:password_check_
                                                    syncService:syncService()];
    mediator_.consumer = consumer_;
  }

  SyncSetupService* syncService() {
    return SyncSetupServiceFactory::GetForBrowserState(browser_state_.get());
  }

  PasswordsMediator* mediator() { return mediator_; }

  ChromeBrowserState* browserState() { return browser_state_.get(); }

  TestPasswordStore* store() { return store_.get(); }

  FakePasswordsConsumer* consumer() { return consumer_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_;
  FakePasswordsConsumer* consumer_;
  PasswordsMediator* mediator_;
};

TEST_F(PasswordsMediatorTest, ElapsedTimeSinceLastCheck) {
  EXPECT_NSEQ(@"Check never run.",
              [mediator() formatElapsedTimeSinceLastCheck]);

  base::Time expected1 = base::Time::Now() - base::TimeDelta::FromSeconds(10);
  browserState()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      expected1.ToDoubleT());

  EXPECT_NSEQ(@"Last checked just now.",
              [mediator() formatElapsedTimeSinceLastCheck]);

  base::Time expected2 = base::Time::Now() - base::TimeDelta::FromMinutes(5);
  browserState()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      expected2.ToDoubleT());

  EXPECT_NSEQ(@"Last checked 5 minutes ago.",
              [mediator() formatElapsedTimeSinceLastCheck]);
}

// Consumer should be notified when passwords are changed.
TEST_F(PasswordsMediatorTest, NotifiesConsumerOnPasswordChange) {
  PasswordForm form = CreatePasswordForm();
  store()->AddLogin(form);
  RunUntilIdle();
  EXPECT_THAT([consumer() savedForms], testing::ElementsAre(form));

  // Remove form from the store.
  store()->RemoveLogin(form);
  RunUntilIdle();
  EXPECT_THAT([consumer() savedForms], testing::IsEmpty());
}

// Duplicates of a form should be removed as well.
TEST_F(PasswordsMediatorTest, DeleteFormWithDuplicates) {
  PasswordForm form = CreatePasswordForm();
  PasswordForm duplicate = form;
  duplicate.username_element = u"element";

  store()->AddLogin(form);
  store()->AddLogin(duplicate);
  RunUntilIdle();
  ASSERT_THAT([consumer() savedForms], testing::ElementsAre(form));

  [mediator() deletePasswordForm:form];
  RunUntilIdle();
  EXPECT_THAT([consumer() savedForms], testing::IsEmpty());
}
