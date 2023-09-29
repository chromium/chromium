// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/strings/string_piece.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_mediator+private.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_observer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

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

// Create the Feature Engagement Mock Tracker.
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    web::BrowserState* browser_state) {
  return std::make_unique<feature_engagement::test::MockTracker>();
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

@property(nonatomic, assign) BOOL shouldShowPasswordManagerWidgetPromoCalled;

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

- (void)setSavingPasswordsToAccount:(BOOL)savingPasswordsToAccount {
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

- (std::vector<password_manager::AffiliatedGroup>)affiliatedGroups {
  return _affiliatedGroups;
}

- (void)setShouldShowPasswordManagerWidgetPromo:
    (BOOL)shouldShowPasswordManagerWidgetPromo {
  _shouldShowPasswordManagerWidgetPromoCalled = YES;
}

@end

// Tests for Passwords mediator.
class PasswordsMediatorTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();
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

    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
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
                       faviconLoader:IOSChromeFaviconLoaderFactory::
                                         GetForBrowserState(
                                             browser_state_.get())
                         syncService:SyncServiceFactory::GetForBrowserState(
                                         browser_state_.get())
                         prefService:browser_state_->GetPrefs()];

    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserState(browserState()));
    mediator_.tracker = mock_tracker_;

    mediator_.consumer = consumer_;
  }

  PasswordsMediator* mediator() { return mediator_; }

  ChromeBrowserState* browserState() { return browser_state_.get(); }

  TestPasswordStore* store() { return store_.get(); }

  FakePasswordsConsumer* consumer() { return consumer_; }

  feature_engagement::test::MockTracker* mockTracker() {
    return mock_tracker_;
  };

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_;
  FakePasswordsConsumer* consumer_;
  PasswordsMediator* mediator_;
  feature_engagement::test::MockTracker* mock_tracker_;
};

// Consumer should be notified when passwords are changed.
TEST_F(PasswordsMediatorTest, NotifiesConsumerOnPasswordChange) {
  PasswordForm form = CreatePasswordForm();
  store()->AddLogin(form);
  RunUntilIdle();
  password_manager::CredentialUIEntry credential(form);
  std::vector<password_manager::AffiliatedGroup> affiliatedGroups =
      [consumer() affiliatedGroups];
  EXPECT_EQ(1u, affiliatedGroups.size());
  EXPECT_THAT(affiliatedGroups[0].GetCredentials(),
              testing::ElementsAre(credential));
  // Remove form from the store.
  store()->RemoveLogin(form);
  RunUntilIdle();
  affiliatedGroups = [consumer() affiliatedGroups];
  EXPECT_THAT(affiliatedGroups, testing::IsEmpty());
}

// Tests that `ShouldTriggerHelpUI` is called on the FET and that the consumer
// is being notified of whether the Password Manager widget promo should be
// shown when the mediator's consumer is set.
TEST_F(PasswordsMediatorTest, NotifiesConsumerToShowPromoOrNot) {
  // Make sure that `shouldShowPasswordManagerWidgetPromoCalled` isn't already
  // true.
  EXPECT_FALSE(consumer().shouldShowPasswordManagerWidgetPromoCalled);

  EXPECT_CALL(
      *mockTracker(),
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature)))
      .Times(testing::Exactly(1));

  [mediator() askFETToShowPasswordManagerWidgetPromo];

  EXPECT_TRUE(consumer().shouldShowPasswordManagerWidgetPromoCalled);
}

// Tests that `Dismissed` is called on the FET on disconnect when the Password
// Manager widget promo was shown and was not dismissed by the user.
TEST_F(PasswordsMediatorTest, NotifiesFETToDismissPromoOnDisconnect) {
  mediator().shouldNotifyFETToDismissPasswordManagerWidgetPromo = YES;

  EXPECT_CALL(
      *mockTracker(),
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature)))
      .Times(testing::Exactly(1));

  [mediator() disconnect];
}

// Tests that `NotifyEvent` and `Dismissed` is called on the FET when the user
// taps the close button of the Password Manager widget promo.
TEST_F(PasswordsMediatorTest, NotifiesFETToDismissPromoOnPromoClosed) {
  mediator().shouldNotifyFETToDismissPasswordManagerWidgetPromo = YES;

  EXPECT_CALL(
      *mockTracker(),
      NotifyEvent(
          feature_engagement::events::kPasswordManagerWidgetPromoClosed));
  EXPECT_CALL(
      *mockTracker(),
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature)))
      .Times(testing::Exactly(1));

  [mediator() notifyFETOfPasswordManagerWidgetPromoDismissal];

  EXPECT_FALSE(mediator().shouldNotifyFETToDismissPasswordManagerWidgetPromo);
}
