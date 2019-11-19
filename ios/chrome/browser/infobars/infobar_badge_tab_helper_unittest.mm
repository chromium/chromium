// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_container_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_confirm_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// InfobarTabHelperDelegate for testing.
@interface InfobarBadgeTabHelperTestDelegate
    : NSObject <InfobarBadgeTabHelperDelegate>
@property(nonatomic, assign) BOOL displayingBadge;
@property(nonatomic, assign) BOOL badgeIsTappable;
@property(nonatomic, assign) BadgeState badgeState;
@property(nonatomic, assign) BadgeType badgeType;
@end

@implementation InfobarBadgeTabHelperTestDelegate
- (void)updateInfobarBadge:(id<BadgeItem>)badgeItem {
  self.badgeIsTappable = badgeItem.isTappable;
  self.badgeState = badgeItem.badgeState;
  self.badgeType = badgeItem.badgeType;
}
- (void)addInfobarBadge:(id<BadgeItem>)badgeItem {
  self.displayingBadge = YES;
  self.badgeIsTappable = badgeItem.isTappable;
  self.badgeState = badgeItem.badgeState;
  self.badgeType = badgeItem.badgeType;
}
- (void)removeInfobarBadge:(id<BadgeItem>)badgeItem {
  self.displayingBadge = NO;
}
@end

// InfobarBadgeUIDelegate for testing.
// TODO(crbug.com/892376): Once InfobarContainerMediator stops using TabModel we
// should be able to use it instead of this fake.
@interface InfobarBadgeUITestDelegate : NSObject <InfobarBadgeUIDelegate>
@property(nonatomic, assign) InfobarBadgeTabHelper* infobarBadgeTabHelper;
@end

@implementation InfobarBadgeUITestDelegate
- (void)infobarBannerWasDismissed:(InfobarType)infobarType
                      forWebState:(web::WebState*)webState {
  // TODO(crbug.com/977340): Test this method.
  self.infobarBadgeTabHelper->UpdateBadgeForInfobarBannerDismissed(infobarType);
}
- (void)infobarBannerWasPresented:(InfobarType)infobarType
                      forWebState:(web::WebState*)webState {
  self.infobarBadgeTabHelper->UpdateBadgeForInfobarBannerPresented(infobarType);
}
- (void)infobarWasAccepted:(InfobarType)infobarType
               forWebState:(web::WebState*)webState {
  self.infobarBadgeTabHelper->UpdateBadgeForInfobarAccepted(infobarType);
}

- (void)infobarWasReverted:(InfobarType)infobarType
               forWebState:(web::WebState*)webState {
  self.infobarBadgeTabHelper->UpdateBadgeForInfobarReverted(infobarType);
}
@end

// Fake Infobar Container.
@interface FakeInfobarContainerCoordinator : NSObject <InfobarContainerConsumer>
@property(nonatomic, strong) UIViewController* baseViewController;
@property(nonatomic, strong) InfobarCoordinator* infobarCoordinator;
@property(nonatomic, assign) BOOL bannerIsPresenting;
- (void)presentModal;
- (void)destroyInfobar;
- (void)removeInfobarView;
@end

@implementation FakeInfobarContainerCoordinator
- (void)addInfoBarWithDelegate:(id<InfobarUIDelegate>)infoBarDelegate {
  self.infobarCoordinator = static_cast<InfobarCoordinator*>(infoBarDelegate);
  self.infobarCoordinator.baseViewController = self.baseViewController;
  [self.infobarCoordinator start];
  self.bannerIsPresenting = YES;
  [self.infobarCoordinator presentInfobarBannerAnimated:NO completion:nil];
}
- (void)infobarManagerWillChange {
}
- (void)setUserInteractionEnabled:(BOOL)enabled {
}
- (void)updateLayoutAnimated:(BOOL)animated {
}
- (void)presentModal {
  [self.infobarCoordinator presentInfobarModal];
}
- (void)dismissBanner {
  [self.infobarCoordinator dismissInfobarBanner:self
                                       animated:NO
                                     completion:^{
                                       self.bannerIsPresenting = NO;
                                     }
                                  userInitiated:NO];
}
- (void)destroyInfobar {
  [self.infobarCoordinator detachView];
}
- (void)removeInfobarView {
  [self.infobarCoordinator removeView];
}
@end

// Test fixture for testing InfobarBadgeTabHelper.
class InfobarBadgeTabHelperTest : public PlatformTest {
 protected:
  InfobarBadgeTabHelperTest()
      : infobar_badge_tab_delegate_(
            [[InfobarBadgeTabHelperTestDelegate alloc] init]),
        browser_state_(TestChromeBrowserState::Builder().Build()),
        infobar_container_coordinator_(
            [[FakeInfobarContainerCoordinator alloc] init]),
        infobar_badge_ui_delegate_([[InfobarBadgeUITestDelegate alloc] init]) {
    // Enable kInfobarUIReboot flag.
    feature_list_.InitAndEnableFeature(kInfobarUIReboot);

    // Setup navigation manager. Needed for InfobarManager.
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager->SetBrowserState(browser_state_.get());
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetBrowserState(browser_state_.get());

    // Create the InfobarManager for web_state_.
    InfoBarManagerImpl::CreateForWebState(&web_state_);

    // Create the InfobarBadgeTabHelper for web_state_ and set its delegate.
    InfobarBadgeTabHelper::CreateForWebState(&web_state_);
    tab_helper()->SetDelegate(infobar_badge_tab_delegate_);

    // Configure the fake InfobarContainerCoordinator, and set its baseVC as
    // rootVC.
    infobar_container_coordinator_.baseViewController =
        [[UIViewController alloc] init];
    [scoped_key_window_.Get()
        setRootViewController:infobar_container_coordinator_
                                  .baseViewController];

    // Configure the fake InfobarBadgeUIDelegate.
    infobar_badge_ui_delegate_.infobarBadgeTabHelper = tab_helper();

    // Create InfobarContainerIOS.
    infobar_container_model_.reset(
        new InfoBarContainerIOS(infobar_container_coordinator_, nil));
    infobar_container_model_->ChangeInfoBarManager(GetInfobarManager());
  }

  ~InfobarBadgeTabHelperTest() override {
    if (infobar_container_coordinator_.bannerIsPresenting) {
      [infobar_container_coordinator_ dismissBanner];
      EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, ^bool {
            return !infobar_container_coordinator_.bannerIsPresenting;
          }));
    }
  }

  // Adds an Infobar to the InfobarManager.
  void AddInfoBar(bool has_badge) {
    // Create and configure the InfobarCoordinator.
    TestInfoBarDelegate* test_infobar_delegate =
        new TestInfoBarDelegate(@"Title");
    InfobarConfirmCoordinator* coordinator = [[InfobarConfirmCoordinator alloc]
        initWithInfoBarDelegate:test_infobar_delegate
                   badgeSupport:has_badge
                           type:InfobarType::kInfobarTypePasswordSave];
    coordinator.browserState = browser_state_.get();
    coordinator.badgeDelegate = infobar_badge_ui_delegate_;

    // Create the InfobarIOS using the InfobarCoordinator and add it to the
    // InfobarManager, this will trigger the Infobar presentation.
    std::unique_ptr<ConfirmInfoBarDelegate> infobar_delegate =
        std::unique_ptr<ConfirmInfoBarDelegate>(test_infobar_delegate);
    GetInfobarManager()->AddInfoBar(
        std::make_unique<InfoBarIOS>(coordinator, std::move(infobar_delegate)));
  }

  // Returns InfobarBadgeTabHelper attached to web_state_.
  InfobarBadgeTabHelper* tab_helper() {
    return InfobarBadgeTabHelper::FromWebState(&web_state_);
  }

  // Returns InfoBarManager attached to web_state_.
  infobars::InfoBarManager* GetInfobarManager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }

  base::test::TaskEnvironment environment_;
  InfobarBadgeTabHelperTestDelegate* infobar_badge_tab_delegate_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  FakeInfobarContainerCoordinator* infobar_container_coordinator_;
  InfobarBadgeUITestDelegate* infobar_badge_ui_delegate_;
  base::test::ScopedFeatureList feature_list_;
  web::TestWebState web_state_;
  web::TestNavigationManager* navigation_manager_;
  std::unique_ptr<InfoBarContainerIOS> infobar_container_model_;
  ScopedKeyWindow scoped_key_window_;
};

// Test the badge state after changes to the state of an Infobar. Infobar badges
// should always be tappable.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeState) {
  EXPECT_FALSE(infobar_badge_tab_delegate_.displayingBadge);
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeIsTappable);
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &= BadgeStateAccepted);
  AddInfoBar(/*has_badge=*/true);
  // Test that adding the infobar (which causes the banner to be presented) is
  // reflected in the badge state.
  EXPECT_TRUE(infobar_badge_tab_delegate_.displayingBadge);
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeIsTappable);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_badge_tab_delegate_.badgeState &= BadgeStatePresented;
      }));
  // Test that accepting the Infobar sets the badge to accepted state.
  tab_helper()->UpdateBadgeForInfobarAccepted(
      InfobarType::kInfobarTypePasswordSave);
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeIsTappable);
  EXPECT_EQ(BadgeStateAccepted,
            infobar_badge_tab_delegate_.badgeState & BadgeStateAccepted);
  EXPECT_EQ(BadgeStateRead,
            infobar_badge_tab_delegate_.badgeState & BadgeStateRead);

  tab_helper()->UpdateBadgeForInfobarReverted(
      InfobarType::kInfobarTypePasswordSave);
  EXPECT_NE(BadgeStateAccepted,
            infobar_badge_tab_delegate_.badgeState & BadgeStateAccepted);
}

// Test the badge state after doesn't change after adding an Infobar with no
// badge.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeStateNoBadge) {
  EXPECT_FALSE(infobar_badge_tab_delegate_.displayingBadge);
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeIsTappable);
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &= BadgeStateAccepted);
  AddInfoBar(/*has_badge=*/false);
  EXPECT_FALSE(infobar_badge_tab_delegate_.displayingBadge);
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeIsTappable);
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &= BadgeStateAccepted);
}

// Tests that the InfobarBadge has not been removed after dismissing the
// InfobarBanner.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnBannerDismissal) {
  AddInfoBar(/*has_badge=*/true);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_badge_tab_delegate_.badgeState &= BadgeStatePresented;
      }));
  [infobar_container_coordinator_ dismissBanner];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !infobar_container_coordinator_.bannerIsPresenting;
      }));
  EXPECT_TRUE(infobar_badge_tab_delegate_.displayingBadge);
  // Banner was dismissed, so the badgeState should not be marked as presented.
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &= BadgeStatePresented);
}

// Test that the Accepted badge state remains after dismissing the
// InfobarBanner.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnBannerAccepted) {
  AddInfoBar(/*has_badge=*/true);
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &= BadgeStateAccepted);
  tab_helper()->UpdateBadgeForInfobarAccepted(
      InfobarType::kInfobarTypePasswordSave);
  [infobar_container_coordinator_ dismissBanner];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !infobar_container_coordinator_.bannerIsPresenting;
      }));
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &= BadgeStateAccepted);
}

// Test that removing the InfobarView doesn't stop displaying the badge.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnInfobarViewRemoval) {
  AddInfoBar(/*has_badge=*/true);
  [infobar_container_coordinator_ dismissBanner];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !infobar_container_coordinator_.bannerIsPresenting;
      }));
  [infobar_container_coordinator_ removeInfobarView];
  EXPECT_TRUE(infobar_badge_tab_delegate_.displayingBadge);
}

// Test that destroying the InfobarView stops displaying the badge.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnInfobarDestroyal) {
  AddInfoBar(/*has_badge=*/true);
  [infobar_container_coordinator_ dismissBanner];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !infobar_container_coordinator_.bannerIsPresenting;
      }));
  [infobar_container_coordinator_ destroyInfobar];
  EXPECT_FALSE(infobar_badge_tab_delegate_.displayingBadge);
}
