// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilCondition;

// Test object conforming to PasswordFetcherDelegate used to verify the results
// from the password store.
@interface TestPasswordFetcherDelegate : NSObject<PasswordFetcherDelegate> {
  // Ivar to store the results from the store.
  std::vector<std::unique_ptr<password_manager::PasswordForm>> _passwords;
}

// Returns the count of recieved passwords.
@property(nonatomic, readonly) size_t passwordNumber;

@end

@implementation TestPasswordFetcherDelegate

- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
              passwords {
  _passwords = std::move(passwords);
}

- (size_t)passwordNumber {
  return _passwords.size();
}

@end

namespace {

scoped_refptr<RefcountedKeyedService> BuildPasswordStore(
    password_manager::IsAccountStore is_account_store,
    web::BrowserState* browser_state) {
  auto store = base::MakeRefCounted<password_manager::TestPasswordStore>(
      is_account_store);
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  return store;
}

password_manager::PasswordForm MakeForm1() {
  password_manager::PasswordForm form;
  form.url = GURL("http://www.example.com/accounts/LoginAuth");
  form.action = GURL("http://www.example.com/accounts/Login");
  form.username_element = u"Email";
  form.username_value = u"test@egmail.com";
  form.password_element = u"Passwd";
  form.password_value = u"test";
  form.submit_element = u"signIn";
  form.signon_realm = "http://www.example.com/";
  form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  form.blocked_by_user = false;
  return form;
}

password_manager::PasswordForm MakeForm2() {
  password_manager::PasswordForm form;
  form.url = GURL("http://www.example2.com/accounts/LoginAuth");
  form.action = GURL("http://www.example2.com/accounts/Login");
  form.username_element = u"Email";
  form.username_value = u"test@egmail.com";
  form.password_element = u"Passwd";
  form.password_value = u"test";
  form.submit_element = u"signIn";
  form.signon_realm = "http://www.example2.com/";
  form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  form.blocked_by_user = false;
  return form;
}

password_manager::PasswordForm MakeBlockedForm() {
  password_manager::PasswordForm form;
  form.url = GURL("http://www.secret.com/login");
  form.signon_realm = "http://www.secret.test/";
  form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  form.blocked_by_user = true;
  return form;
}

class PasswordFetcherTest : public PlatformTest {
 protected:
  PasswordFetcherTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(&BuildPasswordStore,
                            password_manager::IsAccountStore(false)));
    // Despite overriding BuildServiceInstanceFor() for the account factory,
    // GetAccountPasswordStore() is still null if the feature is off, which
    // matches production behavior. This just future-proofs the tests for when
    // the feature is enabled.
    test_cbs_builder.AddTestingFactory(
        IOSChromeAccountPasswordStoreFactory::GetInstance(),
        base::BindRepeating(&BuildPasswordStore,
                            password_manager::IsAccountStore(true)));
    chrome_browser_state_ = test_cbs_builder.Build();
    ASSERT_EQ(base::FeatureList::IsEnabled(
                  password_manager::features::kEnablePasswordsAccountStorage),
              !!GetAccountPasswordStore());
  }

  scoped_refptr<password_manager::PasswordStoreInterface>
  GetProfilePasswordStore() {
    return IOSChromePasswordStoreFactory::GetForBrowserState(
        chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  }

  scoped_refptr<password_manager::PasswordStoreInterface>
  GetAccountPasswordStore() {
    return IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
        chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests PasswordFetcher initialization.
TEST_F(PasswordFetcherTest, Initialization) {
  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  PasswordFetcher* passwordFetcher = [[PasswordFetcher alloc]
      initWithProfilePasswordStore:GetProfilePasswordStore()
              accountPasswordStore:GetAccountPasswordStore()
                          delegate:passwordFetcherDelegate
                               URL:GURL::EmptyGURL()];
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher returns 1 passwords.
TEST_F(PasswordFetcherTest, ReturnsPassword) {
  GetProfilePasswordStore()->AddLogin(MakeForm1());

  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  PasswordFetcher* passwordFetcher = [[PasswordFetcher alloc]
      initWithProfilePasswordStore:GetProfilePasswordStore()
              accountPasswordStore:GetAccountPasswordStore()
                          delegate:passwordFetcherDelegate
                               URL:GURL::EmptyGURL()];

  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      /*run_message_loop=*/true, kWaitForActionTimeout);

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher returns 2 passwords.
TEST_F(PasswordFetcherTest, ReturnsTwoPasswords) {
  GetProfilePasswordStore()->AddLogin(MakeForm1());
  GetProfilePasswordStore()->AddLogin(MakeForm2());

  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  PasswordFetcher* passwordFetcher = [[PasswordFetcher alloc]
      initWithProfilePasswordStore:GetProfilePasswordStore()
              accountPasswordStore:GetAccountPasswordStore()
                          delegate:passwordFetcherDelegate
                               URL:GURL::EmptyGURL()];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      /*run_message_loop=*/true, kWaitForActionTimeout);

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 2u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher ignores blocked passwords.
TEST_F(PasswordFetcherTest, IgnoresBlocked) {
  GetProfilePasswordStore()->AddLogin(MakeForm1());
  GetProfilePasswordStore()->AddLogin(MakeBlockedForm());

  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  PasswordFetcher* passwordFetcher = [[PasswordFetcher alloc]
      initWithProfilePasswordStore:GetProfilePasswordStore()
              accountPasswordStore:GetAccountPasswordStore()
                          delegate:passwordFetcherDelegate
                               URL:GURL::EmptyGURL()];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      /*run_message_loop=*/true, kWaitForActionTimeout);

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher ignores duplicated passwords.
TEST_F(PasswordFetcherTest, IgnoresDuplicated) {
  GetProfilePasswordStore()->AddLogin(MakeForm1());
  GetProfilePasswordStore()->AddLogin(MakeForm1());
  GetProfilePasswordStore()->AddLogin(MakeForm1());
  GetProfilePasswordStore()->AddLogin(MakeForm1());

  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  PasswordFetcher* passwordFetcher = [[PasswordFetcher alloc]
      initWithProfilePasswordStore:GetProfilePasswordStore()
              accountPasswordStore:GetAccountPasswordStore()
                          delegate:passwordFetcherDelegate
                               URL:GURL::EmptyGURL()];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      /*run_message_loop=*/true, kWaitForActionTimeout);

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher receives 0 passwords.
TEST_F(PasswordFetcherTest, ReceivesZeroPasswords) {
  GetProfilePasswordStore()->AddLogin(MakeForm1());

  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  PasswordFetcher* passwordFetcher = [[PasswordFetcher alloc]
      initWithProfilePasswordStore:GetProfilePasswordStore()
              accountPasswordStore:GetAccountPasswordStore()
                          delegate:passwordFetcherDelegate
                               URL:GURL::EmptyGURL()];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      /*run_message_loop=*/true, kWaitForActionTimeout);
  ASSERT_EQ(passwordFetcherDelegate.passwordNumber, 1u);

  GetProfilePasswordStore()->RemoveLogin(MakeForm1());

  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber == 0;
      },
      /*run_message_loop=*/true, kWaitForActionTimeout);
  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 0u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher filters 1 passwords.
TEST_F(PasswordFetcherTest, FilterPassword) {
  GetProfilePasswordStore()->AddLogin(MakeForm1());
  GetProfilePasswordStore()->AddLogin(MakeForm2());

  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  PasswordFetcher* passwordFetcher = [[PasswordFetcher alloc]
      initWithProfilePasswordStore:GetProfilePasswordStore()
              accountPasswordStore:GetAccountPasswordStore()
                          delegate:passwordFetcherDelegate
                               URL:GURL("http://www.example.com/accounts/"
                                        "Login")];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      /*run_message_loop=*/true, kWaitForActionTimeout);

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

class PasswordFetcherTestWithAccountStorage : public PasswordFetcherTest {
 protected:
  PasswordFetcherTestWithAccountStorage() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kEnablePasswordsAccountStorage);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests PasswordFetcher ignores duplicated passwords in different stores.
TEST_F(PasswordFetcherTestWithAccountStorage, IgnoresDuplicateInOtherStore) {
  GetProfilePasswordStore()->AddLogin(MakeForm1());
  GetAccountPasswordStore()->AddLogin(MakeForm1());

  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  PasswordFetcher* passwordFetcher = [[PasswordFetcher alloc]
      initWithProfilePasswordStore:GetProfilePasswordStore()
              accountPasswordStore:GetAccountPasswordStore()
                          delegate:passwordFetcherDelegate
                               URL:GURL::EmptyGURL()];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      /*run_message_loop=*/true, kWaitForActionTimeout);

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

}  // namespace
