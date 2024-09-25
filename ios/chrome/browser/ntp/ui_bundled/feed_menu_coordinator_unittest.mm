// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_menu_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

// Contains the attributes of a UIAction that will be checked.
struct ExpectedAction {
  int title;

  // Expects the attributes of `action` to match this `ExpectedAction`.
  void ExpectMatch(UIAction* action) {
    NSString* expected_title = l10n_util::GetNSString(this->title);
    EXPECT_NSEQ(action.title, expected_title);
  }
};

}  // namespace

// Test fixture for testing FeedMenuCoordinator class.
class FeedMenuCoordinatorTest : public PlatformTest {
 protected:
  FeedMenuCoordinatorTest() {
    TestProfileIOS::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_cbs_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    button_ = [[UIButton alloc] init];
    [base_view_controller_.view addSubview:button_];
    CreateCoordinator();
    [coordinator_ start];
  }

  ~FeedMenuCoordinatorTest() override { [coordinator_ stop]; }

  // Creates an instance of the coordinator.
  void CreateCoordinator() {
    coordinator_ = [[FeedMenuCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
  }

  // Enables or disables the feed in prefs.
  void SetFeedEnabled(bool enabled) {
    PrefService* prefs = profile_->GetPrefs();
    prefs->SetBoolean(feed::prefs::kArticlesListVisible, enabled);
  }

  // Fakes a sign-in with a fake identity.
  void SignInFakeIdentity() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    auth_service->SignIn(identity,
                         signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  // Expects that the action attributes match the given `items`.
  void ExpectActions(std::vector<ExpectedAction> expected) {
    NSArray<UIAction*>* actions =
        static_cast<NSArray<UIAction*>*>(button_.menu.children);
    ASSERT_EQ(actions.count, expected.size());
    int index = 0;
    for (UIAction* action in actions) {
      expected[index].ExpectMatch(action);
      index++;
    }
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  UIButton* button_;
  FeedMenuCoordinator* coordinator_;
};

#pragma mark - Tests

// Tests the menu actions when the feed is turned on.
TEST_F(FeedMenuCoordinatorTest, FeedOn) {
  SetFeedEnabled(true);
  [coordinator_ configureManagementMenu:button_];
  ExpectActions({{IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM},
                 {IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM}});
}

// Tests the menu actions when the feed is turned off.
TEST_F(FeedMenuCoordinatorTest, FeedOff) {
  SetFeedEnabled(false);
  [coordinator_ configureManagementMenu:button_];
  ExpectActions({{IDS_IOS_DISCOVER_FEED_MENU_TURN_ON_ITEM},
                 {IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM}});
}

// Tests the menu actions when the user is signed-in.
TEST_F(FeedMenuCoordinatorTest, SignedIn) {
  SetFeedEnabled(true);
  SignInFakeIdentity();
  [coordinator_ configureManagementMenu:button_];
  ExpectActions({{IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM},
                 {IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ITEM},
                 {IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM}});
}
