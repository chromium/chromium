// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"

#import <Foundation/Foundation.h>

#import "base/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  password_manager::PasswordStoreInterface* GetPasswordStore() {
    return IOSChromePasswordStoreFactory::GetForBrowserState(
               chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
        .get();
  }

  password_manager::PasswordForm Form1() {
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

  // Creates and adds a saved password form.
  void AddSavedForm1() { GetPasswordStore()->AddLogin(Form1()); }

  // Creates and adds a saved password form.
  void AddSavedForm2() {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL("http://www.example2.com/accounts/LoginAuth");
    form->action = GURL("http://www.example2.com/accounts/Login");
    form->username_element = u"Email";
    form->username_value = u"test@egmail.com";
    form->password_element = u"Passwd";
    form->password_value = u"test";
    form->submit_element = u"signIn";
    form->signon_realm = "http://www.example2.com/";
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->blocked_by_user = false;
    GetPasswordStore()->AddLogin(*std::move(form));
  }

  // Creates and adds a blocked site form to never offer to save
  // user's password to those sites.
  void AddBlockedForm() {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL("http://www.secret.com/login");
    form->signon_realm = "http://www.secret.test/";
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->blocked_by_user = true;
    GetPasswordStore()->AddLogin(*std::move(form));
  }

  base::test::TaskEnvironment task_environment_;
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
                                            delegate:passwordFetcherDelegate
                                                 URL:GURL::EmptyGURL()];
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
                                            delegate:passwordFetcherDelegate
                                                 URL:GURL::EmptyGURL()];

  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::Seconds(1000));

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
                                            delegate:passwordFetcherDelegate
                                                 URL:GURL::EmptyGURL()];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::Seconds(1000));

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 2u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher ignores blocked passwords.
TEST_F(PasswordFetcherTest, IgnoresBlocked) {
  AddSavedForm1();
  AddBlockedForm();
  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  PasswordFetcher* passwordFetcher =
      [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                            delegate:passwordFetcherDelegate
                                                 URL:GURL::EmptyGURL()];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::Seconds(1000));

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
                                            delegate:passwordFetcherDelegate
                                                 URL:GURL::EmptyGURL()];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::Seconds(1000));

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher receives 0 passwords.
TEST_F(PasswordFetcherTest, ReceivesZeroPasswords) {
  AddSavedForm1();
  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  PasswordFetcher* passwordFetcher =
      [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                            delegate:passwordFetcherDelegate
                                                 URL:GURL::EmptyGURL()];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::Seconds(1000));
  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);

  GetPasswordStore()->RemoveLogin(Form1());

  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber == 0;
      },
      true, base::Seconds(1000));
  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 0u);
  EXPECT_TRUE(passwordFetcher);
}

// Tests PasswordFetcher filters 1 passwords.
TEST_F(PasswordFetcherTest, FilterPassword) {
  AddSavedForm1();
  AddSavedForm2();
  TestPasswordFetcherDelegate* passwordFetcherDelegate =
      [[TestPasswordFetcherDelegate alloc] init];
  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  PasswordFetcher* passwordFetcher = [[PasswordFetcher alloc]
      initWithPasswordStore:passwordStore
                   delegate:passwordFetcherDelegate
                        URL:GURL("http://www.example.com/accounts/Login")];
  WaitUntilCondition(
      ^bool {
        return passwordFetcherDelegate.passwordNumber > 0;
      },
      true, base::Seconds(1000));

  EXPECT_EQ(passwordFetcherDelegate.passwordNumber, 1u);
  EXPECT_TRUE(passwordFetcher);
}

}  // namespace
