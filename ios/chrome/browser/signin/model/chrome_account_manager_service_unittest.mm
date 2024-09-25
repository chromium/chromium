// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

#import "base/memory/raw_ptr.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

FakeSystemIdentity* gmail_identity =
    [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"];
FakeSystemIdentity* google_identity =
    [FakeSystemIdentity identityWithEmail:@"foo2@google.com"];
FakeSystemIdentity* chromium_identity1 =
    [FakeSystemIdentity identityWithEmail:@"foo3@chromium.com"];
FakeSystemIdentity* chromium_identity2 =
    [FakeSystemIdentity identityWithEmail:@"foo4@chromium.com"];

class ChromeAccountManagerServiceObserver
    : public ChromeAccountManagerService::Observer {
 public:
  void OnIdentityListChanged() final {
    on_identity_list_changed_called_count += 1;
  }
  void OnIdentityUpdated(id<SystemIdentity> identity) final {
    on_identity_updated_called_count += 1;
  }
  void OnAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                  id<RefreshAccessTokenError> error) final {
    on_access_token_refresh_failed_called_count += 1;
  }

  int on_identity_list_changed_called_count = 0;
  int on_identity_updated_called_count = 0;
  int on_access_token_refresh_failed_called_count = 0;
};

}  // namespace

class ChromeAccountManagerServiceTest : public PlatformTest {
 public:
  ChromeAccountManagerServiceTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    account_manager_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
  }

  // Adds identities to the identity service.
  void AddIdentities() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(gmail_identity);
    system_identity_manager->AddIdentity(google_identity);
    system_identity_manager->AddIdentity(chromium_identity1);
    system_identity_manager->AddIdentity(chromium_identity2);
  }

  // Sets a restricted pattern.
  void SetPattern(const std::string& pattern) {
    base::Value::List allowed_patterns;
    allowed_patterns.Append(pattern);
    GetApplicationContext()->GetLocalState()->SetList(
        prefs::kRestrictAccountsToPatterns, std::move(allowed_patterns));
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<ChromeAccountManagerService> account_manager_;
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
  EXPECT_EQ(account_manager_->IsValidIdentity(gmail_identity), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(google_identity), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity1), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity2), false);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 1);

  SetPattern("foo2@google.com");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(gmail_identity), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(google_identity), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity1), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity2), false);
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
  EXPECT_EQ(account_manager_->IsValidIdentity(gmail_identity), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(google_identity), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity1), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity2), true);
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
  EXPECT_EQ(account_manager_->IsValidIdentity(gmail_identity), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(google_identity), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity1), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity2), false);
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
  EXPECT_EQ(account_manager_->IsValidIdentity(gmail_identity), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(google_identity), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity1), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(chromium_identity2), true);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 4);
}

// Tests that `OnIdentityUpdated()` and `OnIdentityAccessTokenRefreshFailed()`
// don't send notification for identities that are filtered out.
TEST_F(ChromeAccountManagerServiceTest, TestFilterIdentityUpdate) {
  // Keep only chromium identities.
  SetPattern("*chromium.com");
  ChromeAccountManagerServiceObserver observer;
  account_manager_->AddObserver(&observer);
  AddIdentities();
  EXPECT_EQ(observer.on_identity_updated_called_count, 0);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 0);

  // Google identity is filtered out, an update doesn't call the observer.
  account_manager_->OnIdentityUpdated(google_identity);
  EXPECT_EQ(observer.on_identity_updated_called_count, 0);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 0);
  // Chromium identity is not filtered out, an update calls the observer.
  account_manager_->OnIdentityUpdated(chromium_identity1);
  EXPECT_EQ(observer.on_identity_updated_called_count, 1);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 0);

  // Google identity is filtered out, an update doesn't call the observer.
  account_manager_->OnIdentityAccessTokenRefreshFailed(google_identity, nil);
  EXPECT_EQ(observer.on_identity_updated_called_count, 1);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 0);
  // Chromium identity is not filtered out, an update calls the observer.
  account_manager_->OnIdentityAccessTokenRefreshFailed(chromium_identity1, nil);
  EXPECT_EQ(observer.on_identity_updated_called_count, 1);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 1);

  account_manager_->RemoveObserver(&observer);
}
