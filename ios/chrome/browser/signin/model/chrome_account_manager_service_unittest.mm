// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

#import <set>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/values.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

FakeSystemIdentity* foo1_identity =
    [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"];
FakeSystemIdentity* foo2_identity =
    [FakeSystemIdentity identityWithEmail:@"foo2@gmail.com"];
FakeSystemIdentity* bar1_identity =
    [FakeSystemIdentity identityWithEmail:@"bar1@gmail.com"];
FakeSystemIdentity* bar2_identity =
    [FakeSystemIdentity identityWithEmail:@"bar2@gmail.com"];

class ChromeAccountManagerServiceObserver
    : public ChromeAccountManagerService::Observer {
 public:
  void OnIdentitiesInProfileChanged() final {
    on_identities_in_profile_changed_called_count += 1;
  }
  void OnIdentityInProfileUpdated(id<SystemIdentity> identity) final {
    on_identity_in_profile_updated_called_count += 1;
  }
  void OnAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                  id<RefreshAccessTokenError> error,
                                  const std::set<std::string>& scoopes) final {
    on_access_token_refresh_failed_called_count += 1;
  }

  int on_identities_in_profile_changed_called_count = 0;
  int on_identity_in_profile_updated_called_count = 0;
  int on_access_token_refresh_failed_called_count = 0;
};

}  // namespace

class ChromeAccountManagerServiceTest : public PlatformTest {
 public:
  ChromeAccountManagerServiceTest() {
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());

    account_manager_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_);
  }

  ~ChromeAccountManagerServiceTest() override {
    account_manager_ = nullptr;
    profile_ = nullptr;
  }

  // Adds identities to the identity service.
  void AddIdentities() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(foo1_identity);
    system_identity_manager->AddIdentity(foo2_identity);
    system_identity_manager->AddIdentity(bar1_identity);
    system_identity_manager->AddIdentity(bar2_identity);
  }

  // Sets a restricted pattern.
  void SetPattern(const std::string& pattern) {
    base::ListValue allowed_patterns;
    allowed_patterns.Append(pattern);
    GetApplicationContext()->GetLocalState()->SetList(
        prefs::kRestrictAccountsToPatterns, std::move(allowed_patterns));
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  raw_ptr<ChromeAccountManagerService> account_manager_ = nullptr;
};

// Tests to get identities when the restricted pattern is not set.
TEST_F(ChromeAccountManagerServiceTest, TestHasIdentities) {
  EXPECT_FALSE(account_manager_->HasIdentities());
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 0);

  AddIdentities();
  EXPECT_TRUE(account_manager_->HasIdentities());
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 4);
}

// Tests to get identity when the restricted pattern matches only one identity.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentityWithValidRestrictedPattern) {
  AddIdentities();
  EXPECT_TRUE(account_manager_->HasIdentities());

  SetPattern("foo1*");
  EXPECT_TRUE(account_manager_->IsValidIdentity(foo1_identity.gaiaId));
  EXPECT_FALSE(account_manager_->IsValidIdentity(foo2_identity.gaiaId));
  EXPECT_FALSE(account_manager_->IsValidIdentity(bar1_identity.gaiaId));
  EXPECT_FALSE(account_manager_->IsValidIdentity(bar2_identity.gaiaId));
  EXPECT_TRUE(account_manager_->HasIdentities());
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 1);

  SetPattern("foo2*");
  EXPECT_FALSE(account_manager_->IsValidIdentity(foo1_identity.gaiaId));
  EXPECT_TRUE(account_manager_->IsValidIdentity(foo2_identity.gaiaId));
  EXPECT_FALSE(account_manager_->IsValidIdentity(bar1_identity.gaiaId));
  EXPECT_FALSE(account_manager_->IsValidIdentity(bar2_identity.gaiaId));
  EXPECT_TRUE(account_manager_->HasIdentities());
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 1);
}

// Tests to get identities when the restricted pattern matches several
// identities.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentitiesWithValidRestrictedPattern) {
  AddIdentities();
  EXPECT_TRUE(account_manager_->HasIdentities());

  SetPattern("bar*");
  EXPECT_FALSE(account_manager_->IsValidIdentity(foo1_identity.gaiaId));
  EXPECT_FALSE(account_manager_->IsValidIdentity(foo2_identity.gaiaId));
  EXPECT_TRUE(account_manager_->IsValidIdentity(bar1_identity.gaiaId));
  EXPECT_TRUE(account_manager_->IsValidIdentity(bar2_identity.gaiaId));
  EXPECT_TRUE(account_manager_->HasIdentities());
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 2);
}

// Tests to get identity when the restricted pattern doesn't match any identity.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentityWithInvalidRestrictedPattern) {
  AddIdentities();
  EXPECT_TRUE(account_manager_->HasIdentities());

  SetPattern("none*");
  EXPECT_FALSE(account_manager_->IsValidIdentity(foo1_identity.gaiaId));
  EXPECT_FALSE(account_manager_->IsValidIdentity(foo2_identity.gaiaId));
  EXPECT_FALSE(account_manager_->IsValidIdentity(bar1_identity.gaiaId));
  EXPECT_FALSE(account_manager_->IsValidIdentity(bar2_identity.gaiaId));
  EXPECT_FALSE(account_manager_->HasIdentities());
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 0);
}

// Tests to get identity when all identities are matched by pattern.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentityWithAllInclusivePattern) {
  AddIdentities();
  EXPECT_TRUE(account_manager_->HasIdentities());

  SetPattern("*");
  EXPECT_TRUE(account_manager_->IsValidIdentity(foo1_identity.gaiaId));
  EXPECT_TRUE(account_manager_->IsValidIdentity(foo2_identity.gaiaId));
  EXPECT_TRUE(account_manager_->IsValidIdentity(bar1_identity.gaiaId));
  EXPECT_TRUE(account_manager_->IsValidIdentity(bar2_identity.gaiaId));
  EXPECT_TRUE(account_manager_->HasIdentities());
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 4);
}

// Tests that `OnIdentityUpdated()` and `OnIdentityAccessTokenRefreshFailed()`
// don't send notification for identities that are filtered out.
TEST_F(ChromeAccountManagerServiceTest, TestFilterIdentityUpdate) {
  // Keep only bar identities.
  SetPattern("bar*");
  ChromeAccountManagerServiceObserver observer;
  account_manager_->AddObserver(&observer);
  AddIdentities();
  EXPECT_EQ(observer.on_identity_in_profile_updated_called_count, 0);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 0);

  // Foo identity is filtered out, an update doesn't call the observer.
  account_manager_->OnIdentityInProfileUpdated(foo2_identity);
  EXPECT_EQ(observer.on_identity_in_profile_updated_called_count, 0);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 0);
  // Bar identity is not filtered out, an update calls the observer.
  account_manager_->OnIdentityInProfileUpdated(bar1_identity);
  EXPECT_EQ(observer.on_identity_in_profile_updated_called_count, 1);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 0);

  // Foo identity is filtered out, an update doesn't call the observer.
  account_manager_->OnIdentityAccessTokenRefreshFailed(foo2_identity, nil,
                                                       std::set<std::string>());
  EXPECT_EQ(observer.on_identity_in_profile_updated_called_count, 1);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 0);
  // Bar identity is not filtered out, an update calls the observer.
  account_manager_->OnIdentityAccessTokenRefreshFailed(bar1_identity, nil,
                                                       std::set<std::string>());
  EXPECT_EQ(observer.on_identity_in_profile_updated_called_count, 1);
  EXPECT_EQ(observer.on_access_token_refresh_failed_called_count, 1);

  account_manager_->RemoveObserver(&observer);
}
