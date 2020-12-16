// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_mediator.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/infobars/core/infobar_feature.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_badge_model.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/badges/badge_type.h"
#include "ios/chrome/browser/ui/badges/badge_type_util.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state_user_data.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The two infobar types used in tests.  Both support badges.
InfobarType kFirstInfobarType = InfobarType::kInfobarTypePasswordSave;
base::string16 kFirstInfobarMessageText =
    base::ASCIIToUTF16("FakeInfobarDelegate1");
InfobarType kSecondInfobarType = InfobarType::kInfobarTypePasswordUpdate;
base::string16 kSecondInfobarMessageText =
    base::ASCIIToUTF16("FakeInfobarDelegate2");
// Parameters used for BadgeMediator test fixtures.
enum class TestParam {
  kNormal,
  kOffTheRecord,
};
}  // namespace

// Fake of BadgeConsumer.
@interface FakeBadgeConsumer : NSObject <BadgeConsumer>
@property(nonatomic, strong) id<BadgeItem> displayedBadge;
@property(nonatomic, assign) BOOL hasFullscreenOffTheRecordBadge;
@property(nonatomic, assign) BOOL hasUnreadBadge;
@end

@implementation FakeBadgeConsumer
- (void)setupWithDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
                fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem {
  self.hasFullscreenOffTheRecordBadge =
      fullscreenBadgeItem != nil &&
      fullscreenBadgeItem.badgeType == BadgeType::kBadgeTypeIncognito;
  self.displayedBadge = displayedBadgeItem;
}
- (void)updateDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
             fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem {
  self.hasFullscreenOffTheRecordBadge =
      fullscreenBadgeItem != nil &&
      fullscreenBadgeItem.badgeType == BadgeType::kBadgeTypeIncognito;
  self.displayedBadge = displayedBadgeItem;
}
- (void)markDisplayedBadgeAsRead:(BOOL)read {
  self.hasUnreadBadge = !read;
}
@end

class BadgeMediatorTest : public testing::TestWithParam<TestParam> {
 protected:
  BadgeMediatorTest()
      : badge_consumer_([[FakeBadgeConsumer alloc] init]),
        browser_state_(TestChromeBrowserState::Builder().Build()),
        web_state_list_(&web_state_list_delegate_) {
    feature_list_.InitWithFeatures({kIOSInfobarUIReboot},
                                   {kInfobarUIRebootOnlyiOS13});
    OverlayPresenter::FromBrowser(browser(), OverlayModality::kInfobarBanner)
        ->SetPresentationContext(&overlay_presentation_context_);
    badge_mediator_ = [[BadgeMediator alloc] initWithBrowser:browser()];
    badge_mediator_.consumer = badge_consumer_;
  }

  ~BadgeMediatorTest() override {
    OverlayPresenter::FromBrowser(browser(), OverlayModality::kInfobarBanner)
        ->SetPresentationContext(nullptr);
    [badge_mediator_ disconnect];
  }

  // Inserts a new WebState to the WebStateList at |index| and activates it.
  void InsertActivatedWebState(int index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state->SetBrowserState(browser_state());
    InfoBarManagerImpl::CreateForWebState(web_state.get());
    InfobarBadgeTabHelper::CreateForWebState(web_state.get());
    web_state_list_.InsertWebState(index, std::move(web_state),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());
  }

  // Adds an Infobar of |type| to the InfoBarManager and returns the infobar.
  // Pass in different |message_text| to avoid replacing existing infobar.
  InfoBarIOS* AddInfobar(InfobarType type, base::string16 message_text) {
    std::unique_ptr<InfoBarIOS> added_infobar =
        std::make_unique<FakeInfobarIOS>(type, message_text);
    InfoBarIOS* infobar = added_infobar.get();
    infobar_manager()->AddInfoBar(std::move(added_infobar));
    return infobar;
  }

  // Removes |infobar| from its manager.
  void RemoveInfobar(InfoBarIOS* infobar) {
    infobar_manager()->RemoveInfoBar(infobar);
  }

  // Returns whether the test fixture is for an incognito BrowserState.
  bool is_off_the_record() const {
    return GetParam() == TestParam::kOffTheRecord;
  }
  // Returns the BrowserState to use for the test fixture.
  ChromeBrowserState* browser_state() {
    return is_off_the_record()
               ? browser_state_->GetOffTheRecordChromeBrowserState()
               : browser_state_.get();
  }
  // Returns the Browser to use for the test fixture.
  Browser* browser() {
    if (!browser_) {
      browser_ =
          std::make_unique<TestBrowser>(browser_state(), &web_state_list_);
    }
    return browser_.get();
  }
  // Returns the active WebState.
  web::WebState* web_state() { return web_state_list_.GetActiveWebState(); }
  // Returns the active WebState's InfoBarManagerImpl.
  InfoBarManagerImpl* infobar_manager() {
    return InfoBarManagerImpl::FromWebState(web_state());
  }
  // Returns the active WebState's InfobarBadgeTabHelper.
  InfobarBadgeTabHelper* tab_helper() {
    return InfobarBadgeTabHelper::FromWebState(web_state());
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment environment_;
  FakeBadgeConsumer* badge_consumer_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<Browser> browser_;
  FakeOverlayPresentationContext overlay_presentation_context_;
  BadgeMediator* badge_mediator_ = nil;
};

// Test that the BadgeMediator responds with no displayed and fullscreen badge
// when there are no Infobars added and the BrowserState is not OffTheRecord.
TEST_P(BadgeMediatorTest, BadgeMediatorTestNoInfobar) {
  InsertActivatedWebState(/*index=*/0);
  EXPECT_FALSE(badge_consumer_.displayedBadge);
  EXPECT_EQ(is_off_the_record(),
            badge_consumer_.hasFullscreenOffTheRecordBadge);
}

// Test that the BadgeMediator responds with one new badge when an infobar is
// added
TEST_P(BadgeMediatorTest, BadgeMediatorTestAddInfobar) {
  InsertActivatedWebState(/*index=*/0);
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
}

// Test that the BadgeMediator handled the removal of the correct badge when two
// infobars are added and then one is removed.
TEST_P(BadgeMediatorTest, BadgeMediatorTestRemoveInfobar) {
  InsertActivatedWebState(/*index=*/0);
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  InfoBarIOS* second_infobar =
      AddInfobar(kSecondInfobarType, kSecondInfobarMessageText);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypeOverflow);
  RemoveInfobar(second_infobar);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
}

TEST_P(BadgeMediatorTest, BadgeMediatorTestMarkAsRead) {
  InsertActivatedWebState(/*index=*/0);
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  // Since there is only one badge, it should be marked as read.
  EXPECT_FALSE(badge_consumer_.hasUnreadBadge);
  AddInfobar(kSecondInfobarType, kSecondInfobarMessageText);
  ASSERT_EQ(BadgeType::kBadgeTypeOverflow,
            badge_consumer_.displayedBadge.badgeType);
  // Second badge should be unread since the overflow badge is being shown as
  // the displayed badge.
  EXPECT_TRUE(badge_consumer_.hasUnreadBadge);
  tab_helper()->UpdateBadgeForInfobarAccepted(kSecondInfobarType);
  // Second badge should be read since its infobar is accepted.
  EXPECT_FALSE(badge_consumer_.hasUnreadBadge);
}

// Test that the BadgeMediator updates the current badges to none when switching
// to a second WebState after an infobar is added to the first WebState.
TEST_P(BadgeMediatorTest, BadgeMediatorTestSwitchWebState) {
  InsertActivatedWebState(/*index=*/0);
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
  InsertActivatedWebState(/*index=*/1);
  EXPECT_FALSE(badge_consumer_.displayedBadge);
}

// Test that the BadgeMediator does not inform its consumer of a new infobar if
// the added infobar came from an inactive WebState.
TEST_P(BadgeMediatorTest,
       BadgeMediatorTestSwitchWebStateAndAddInfobarToInactiveWebState) {
  InsertActivatedWebState(/*index=*/0);
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
  InsertActivatedWebState(/*index=*/1);
  std::unique_ptr<InfoBarIOS> added_infobar = std::make_unique<FakeInfobarIOS>(
      kSecondInfobarType, kSecondInfobarMessageText);
  InfoBarManagerImpl::FromWebState(web_state_list_.GetWebStateAt(0))
      ->AddInfoBar(std::move(added_infobar));
  EXPECT_FALSE(badge_consumer_.displayedBadge);
}

// Test that the BadgeMediator does not inform its consumer of a new infobar it
// has already been disconnected.
TEST_P(BadgeMediatorTest, BadgeMediatorTestDoNotAddInfobarIfWebStateListGone) {
  InsertActivatedWebState(/*index=*/0);
  ASSERT_FALSE(badge_consumer_.displayedBadge);
  [badge_mediator_ disconnect];
  std::unique_ptr<InfoBarIOS> added_infobar = std::make_unique<FakeInfobarIOS>(
      kSecondInfobarType, kSecondInfobarMessageText);
  InfoBarManagerImpl::FromWebState(web_state_list_.GetActiveWebState())
      ->AddInfoBar(std::move(added_infobar));
  EXPECT_FALSE(badge_consumer_.displayedBadge);
}

// Test that the BadgeMediator updates the badge when it is accepted.
TEST_P(BadgeMediatorTest, BadgeMediatorTestAcceptedBadge) {
  InsertActivatedWebState(/*index=*/0);
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_FALSE(badge_consumer_.displayedBadge.badgeState &= BadgeStateAccepted);

  tab_helper()->UpdateBadgeForInfobarAccepted(kFirstInfobarType);
  EXPECT_TRUE(badge_consumer_.displayedBadge.badgeState &= BadgeStateAccepted);
}

// Test that the BadgeMediator updates the current badges when the starting
// active WebState already has a badge. This simulates an app launch after an
// update when the WebStateList is preserved but the LocationBar (and therefore
// the BadgeMediator) is restarted from scratch.
TEST_P(BadgeMediatorTest, BadgeMediatorTestRestartWithInfobar) {
  InsertActivatedWebState(/*index=*/0);
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);

  // Simulate reload of app, but preservation of WebStateList.
  [badge_mediator_ disconnect];
  badge_mediator_ = nil;
  badge_consumer_ = nil;

  badge_consumer_ = [[FakeBadgeConsumer alloc] init];
  badge_mediator_ = [[BadgeMediator alloc] initWithBrowser:browser()];
  badge_mediator_.consumer = badge_consumer_;
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
}

// Test that the BadgeMediator clears its badges when the last WebState is
// detached and a new WebState is added. This test also makes sure that closing
// the last WebState doesn't break anything.
TEST_P(BadgeMediatorTest, BadgeMediatorTestCloseLastTab) {
  InsertActivatedWebState(/*index=*/0);
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType,
            BadgeType::kBadgeTypePasswordSave);
  web_state_list_.DetachWebStateAt(0);
  InsertActivatedWebState(/*index=*/0);
  ASSERT_FALSE(badge_consumer_.displayedBadge);
}

// Tests that the badge mediator successfully updates the InfobarBadgeTabHelper
// for the active WebState for infobar banner presentation and dismissal.
TEST_P(BadgeMediatorTest, InfobarBannerOverlayObserving) {
  // Add an active WebState at index 0 and add an InfoBar with |type| to the
  // WebState's InfoBarManager, checking that the badge item has been created
  // with the default BadgeState.
  InsertActivatedWebState(/*index=*/0);
  InfobarType type = kFirstInfobarType;
  InfobarBadgeTabHelper* tab_helper =
      InfobarBadgeTabHelper::FromWebState(web_state());
  InfoBarIOS* infobar = AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  NSArray<id<BadgeItem>>* items = tab_helper->GetInfobarBadgeItems();
  ASSERT_EQ(1U, items.count);
  id<BadgeItem> item = [items firstObject];
  ASSERT_EQ(BadgeTypeForInfobarType(type), [item badgeType]);
  ASSERT_FALSE(item.badgeState & BadgeStatePresented);

  // Simulate the presentation of the infobar banner via OverlayPresenter in the
  // fake presentation context, verifying that the badge state is updated
  // accordingly.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state(), OverlayModality::kInfobarBanner);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          infobar, InfobarOverlayType::kBanner, infobar->high_priority()));
  EXPECT_TRUE(item.badgeState & BadgeStatePresented);

  // Simulate dismissal of the banner and verify that the badge state is no
  // longer presented.
  queue->CancelAllRequests();
  EXPECT_FALSE(item.badgeState & BadgeStatePresented);
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         BadgeMediatorTest,
                         testing::Values(TestParam::kNormal,
                                         TestParam::kOffTheRecord));
