// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"

#import <Foundation/Foundation.h>

#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilCondition;

// Test object conforming to PasswordFetcherDelegate used to verify the results
// from the password store.
@interface TestPasswordFetcherDelegate : NSObject<PasswordFetcherDelegate> {
  // Ivar to store the results from the store.
  std::vector<std::unique_ptr<autofill::PasswordForm>> _passwords;
}

// Returns the count of recieved passwords.
@property(nonatomic, readonly) size_t passwordNumber;

@end

@implementation TestPasswordFetcherDelegate

- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<autofill::PasswordForm>>&)passwords {
  _passwords = std::move(passwords);
}

- (size_t)passwordNumber {
  return _passwords.size();
}

@end

namespace {

class PasswordFetcherTest : public PlatformTest {
 protected:
  PasswordFetcherTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    IOSChromePasswordStoreFactory::GetInstance()->SetTestingFactory(
        chrome_browser_state_.get(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));
  }

  password_manager::PasswordStore* GetPasswordStore() {
    return IOSChromePasswordStoreFactory::GetForBrowserState(
               chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
        .get();
  }

  // Creates and adds a saved password form.
  void AddSavedForm1() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->origin = GURL("http://www.example.com/accounts/LoginAuth");
    form->action = GURL("http://www.example.com/accounts/Login");
    form->username_element = base::ASCIIToUTF16("Email");
    form->username_value = base::ASCIIToUTF16("test@egmail.com");
    form->password_element = base::ASCIIToUTF16("Passwd");
    form->password_value = base::ASCIIToUTF16("test");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.example.com/";
    form->preferred = false;
    form->scheme = autofill::PasswordForm::SCHEME_HTML;
    form->blacklisted_by_user = false;
    GetPasswordStore()->AddLogin(*std::move(form));
  }

  // Creates and adds a saved password form.
  void AddSavedForm2() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->origin = GURL("http://www.example2.com/accounts/LoginAuth");
    form->action = GURL("http://www.example2.com/accounts/Login");
    form->username_element = base::ASCIIToUTF16("Email");
    form->username_value = base::ASCIIToUTF16("test@egmail.com");
    form->password_element = base::ASCIIToUTF16("Passwd");
    form->password_value = base::ASCIIToUTF16("test");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.example2.com/";
    form->preferred = false;
    form->scheme = autofill::PasswordForm::SCHEME_HTML;
    form->blacklisted_by_user = false;
    GetPasswordStore()->AddLogin(*std::move(form));
  }

  // Creates and adds a blacklisted site form to never offer to save
  // user's password to those sites.
  void AddBlacklistedForm() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->origin = GURL("http://www.secret.com/login");
    form->action = GURL("http://www.secret.com/action");
    form->username_element = base::ASCIIToUTF16("email");
    form->username_value = base::ASCIIToUTF16("test@secret.com");
    form->password_element = base::ASCIIToUTF16("password");
    form->password_value = base::ASCIIToUTF16("cantsay");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.secret.com/";
    form->preferred = false;
    form->scheme = autofill::PasswordForm::SCHEME_HTML;
    form->blacklisted_by_user = true;
    GetPasswordStore()->AddLogin(*std::move(form));
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests PasswordFetcher initialization.
TEST_F(PasswordFetcherTest, Initialization) {
  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  PasswordFetcher* passwordFetcher =
      [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                            delegate:passwordFetcherDelegate];
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher returns 1 passwords.
TEST_F(PasswordFetcherTest, ReturnsPassword) {
  AddSavedForm1();
  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  PasswordFetcher* passwordFetcher =
      [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                            delegate:passwordFetcherDelegate];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::TimeDelta::FromSeconds(1000));

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher returns 2 passwords.
TEST_F(PasswordFetcherTest, ReturnsTwoPasswords) {
  AddSavedForm1();
  AddSavedForm2();
  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  PasswordFetcher* passwordFetcher =
      [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                            delegate:passwordFetcherDelegate];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::TimeDelta::FromSeconds(1000));

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 2u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher ignores blacklisted passwords.
TEST_F(PasswordFetcherTest, IgnoresBlacklisted) {
  AddSavedForm1();
  AddBlacklistedForm();
  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  PasswordFetcher* passwordFetcher =
      [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                            delegate:passwordFetcherDelegate];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::TimeDelta::FromSeconds(1000));

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher ignores duplicated passwords.
TEST_F(PasswordFetcherTest, IgnoresDuplicated) {
  AddSavedForm1();
  AddSavedForm1();
  AddSavedForm1();
  AddSavedForm1();
  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  PasswordFetcher* passwordFetcher =
      [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                            delegate:passwordFetcherDelegate];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::TimeDelta::FromSeconds(1000));

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

}  // namespace
