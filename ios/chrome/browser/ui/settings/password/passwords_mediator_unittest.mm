// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"

#import "base/mac/foundation_util.h"
#import "base/strings/string_piece.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_observer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using password_manager::PasswordForm;
using password_manager::InsecureType;
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

}  // namespace

@interface FakePasswordsConsumer : NSObject <PasswordsConsumer> {
  std::vector<password_manager::CredentialUIEntry> _passwords;
  std::vector<password_manager::CredentialUIEntry> _blockedSites;
  std::vector<password_manager::AffiliatedGroup> _affiliatedGroups;
}

// Number of time the method updateOnDeviceEncryptionSessionAndUpdateTableView
// was called. Used to test that primary account change and sync change
// causes the update to occur.
@property(nonatomic, assign) NSInteger numberOfCallToChangeOnDeviceEncryption;

@property(nonatomic, copy) NSString* detailedText;

@end

@implementation FakePasswordsConsumer

- (void)setPasswordCheckUIState:(PasswordCheckUIState)state
         insecurePasswordsCount:(NSInteger)insecureCount {
}

- (void)setPasswords:(std::vector<password_manager::CredentialUIEntry>)passwords
        blockedSites:
            (std::vector<password_manager::CredentialUIEntry>)blockedSites {
  _passwords = passwords;
  _blockedSites = blockedSites;
}

- (void)setAffiliatedGroups:
            (const std::vector<password_manager::AffiliatedGroup>&)
                affiliatedGroups
               blockedSites:
                   (const std::vector<password_manager::CredentialUIEntry>&)
                       blockedSites {
  _affiliatedGroups = affiliatedGroups;
  _blockedSites = blockedSites;
}

- (void)updatePasswordsInOtherAppsDetailedText {
  _detailedText = @"On";
}

- (std::vector<password_manager::CredentialUIEntry>)passwords {
  return _passwords;
}

- (void)updateOnDeviceEncryptionSessionAndUpdateTableView {
  self.numberOfCallToChangeOnDeviceEncryption += 1;
}

@end

// Tests for Passwords mediator.
class PasswordsMediatorTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    browser_state_ = builder.Build();

    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromePasswordStoreFactory::GetForBrowserState(
                browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));
    password_check_ = IOSChromePasswordCheckManagerFactory::GetForBrowserState(
        browser_state_.get());

    consumer_ = [[FakePasswordsConsumer alloc] init];

    mediator_ = [[PasswordsMediator alloc]
        initWithPasswordCheckManager:password_check_
                    syncSetupService:syncService()
                       faviconLoader:IOSChromeFaviconLoaderFactory::
                                         GetForBrowserState(
                                             browser_state_.get())
                     identityManager:IdentityManagerFactory::GetForBrowserState(
                                         browser_state_.get())
                         syncService:SyncServiceFactory::GetForBrowserState(
                                         browser_state_.get())];
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

  base::Time expected1 = base::Time::Now() - base::Seconds(10);
  browserState()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      expected1.ToDoubleT());

  EXPECT_NSEQ(@"Last checked just now.",
              [mediator() formatElapsedTimeSinceLastCheck]);

  base::Time expected2 = base::Time::Now() - base::Minutes(5);
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
  EXPECT_THAT([consumer() passwords],
              testing::ElementsAre(password_manager::CredentialUIEntry(form)));

  // Remove form from the store.
  store()->RemoveLogin(form);
  RunUntilIdle();
  EXPECT_THAT([consumer() passwords], testing::IsEmpty());
}

// Mediator should update consumer password autofill state.
TEST_F(PasswordsMediatorTest, TestPasswordAutoFillDidChangeToStatusMethod) {
  ASSERT_EQ([consumer() detailedText], nil);
  [mediator() passwordAutoFillStatusDidChange];
  EXPECT_NSEQ([consumer() detailedText], @"On");
}

TEST_F(PasswordsMediatorTest, SyncChangeTriggersChangeOnDeviceEncryption) {
  DCHECK([mediator() conformsToProtocol:@protocol(SyncObserverModelBridge)]);
  PasswordsMediator<SyncObserverModelBridge>* syncObserver =
      static_cast<PasswordsMediator<SyncObserverModelBridge>*>(mediator());
  [syncObserver onSyncStateChanged];
  ASSERT_EQ(1, consumer().numberOfCallToChangeOnDeviceEncryption);
}

TEST_F(PasswordsMediatorTest, IdentityChangeTriggersChangeOnDeviceEncryption) {
  DCHECK([mediator() conformsToProtocol:@protocol(SyncObserverModelBridge)]);
  PasswordsMediator<IdentityManagerObserverBridgeDelegate>* syncObserver =
      static_cast<PasswordsMediator<IdentityManagerObserverBridgeDelegate>*>(
          mediator());
  const signin::PrimaryAccountChangeEvent event;
  [syncObserver onPrimaryAccountChanged:event];
  ASSERT_EQ(1, consumer().numberOfCallToChangeOnDeviceEncryption);
}
