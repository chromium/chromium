// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/chrome_account_manager_service.h"

#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
FakeSystemIdentity* identity1 =
    [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                   gaiaID:@"foo1ID"
                                     name:@"Fake Foo 1"];
FakeSystemIdentity* identity2 =
    [FakeSystemIdentity identityWithEmail:@"foo2@google.com"
                                   gaiaID:@"foo2ID"
                                     name:@"Fake Foo 2"];
FakeSystemIdentity* identity3 =
    [FakeSystemIdentity identityWithEmail:@"foo3@chromium.com"
                                   gaiaID:@"foo3ID"
                                     name:@"Fake Foo 3"];
FakeSystemIdentity* identity4 =
    [FakeSystemIdentity identityWithEmail:@"foo4@chromium.com"
                                   gaiaID:@"foo4ID"
                                     name:@"Fake Foo 4"];
}  // namespace

class ChromeAccountManagerServiceTest : public PlatformTest {
 public:
  ChromeAccountManagerServiceTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();

    account_manager_ = ChromeAccountManagerServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  // Adds identities to the identity service.
  void AddIdentities() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity1);
    system_identity_manager->AddIdentity(identity2);
    system_identity_manager->AddIdentity(identity3);
    system_identity_manager->AddIdentity(identity4);
  }

  // Sets a restricted pattern.
  void SetPattern(const std::string& pattern) {
    base::Value::List allowed_patterns;
    allowed_patterns.Append(pattern);
    GetApplicationContext()->GetLocalState()->SetList(
        prefs::kRestrictAccountsToPatterns, std::move(allowed_patterns));
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  ChromeAccountManagerService* account_manager_;
};

// Tests to get identities when the restricted pattern is not set.
TEST_F(ChromeAccountManagerServiceTest, TestHasIdentities) {
  EXPECT_EQ(account_manager_->HasIdentities(), false);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 0);

  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 4);
}

// Tests to get identity when the restricted pattern matches only one identity.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentityWithValidRestrictedPattern) {
  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);

  SetPattern("*gmail.com");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), false);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 1);

  SetPattern("foo2@google.com");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), false);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 1);
}

// Tests to get identities when the restricted pattern matches several
// identities.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentitiesWithValidRestrictedPattern) {
  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);

  SetPattern("*chromium.com");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), true);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 2);
}

// Tests to get identity when the restricted pattern doesn't match any identity.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentityWithInvalidRestrictedPattern) {
  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);

  SetPattern("*none.com");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), false);
  EXPECT_EQ(account_manager_->HasIdentities(), false);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 0);
}

// Tests to get identity when all identities are matched by pattern.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentityWithAllInclusivePattern) {
  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);

  SetPattern("*");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), true);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 4);
}
