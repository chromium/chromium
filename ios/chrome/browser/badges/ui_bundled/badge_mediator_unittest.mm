// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/badge_mediator.h"

#import <map>

#import "base/containers/contains.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_consumer.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_item.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_type.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_type_util.h"
#import "ios/chrome/browser/infobars/model/badge_state.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state_user_data.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {
// The two infobar types used in tests.  Both support badges.
InfobarType kFirstInfobarType = InfobarType::kInfobarTypePasswordSave;
std::u16string kFirstInfobarMessageText = u"FakeInfobarDelegate1";
InfobarType kSecondInfobarType = InfobarType::kInfobarTypePasswordUpdate;
std::u16string kSecondInfobarMessageText = u"FakeInfobarDelegate2";
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
      fullscreenBadgeItem.badgeType == kBadgeTypeIncognito;
  self.displayedBadge = displayedBadgeItem;
}
- (void)updateDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
             fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem
                     infoBar:(InfoBarIOS*)infoBar {
  self.hasFullscreenOffTheRecordBadge =
      fullscreenBadgeItem != nil &&
      fullscreenBadgeItem.badgeType == kBadgeTypeIncognito;
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
        profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile())) {
    overlay_presenter_ = OverlayPresenter::FromBrowser(
        browser(), OverlayModality::kInfobarBanner);
    overlay_presenter_->SetPresentationContext(&overlay_presentation_context_);
    badge_mediator_ =
        [[BadgeMediator alloc] initWithWebStateList:web_state_list()
                                   overlayPresenter:overlay_presenter_
                                        isIncognito:is_off_the_record()];
    badge_mediator_.consumer = badge_consumer_;
  }

  ~BadgeMediatorTest() override {
    overlay_presenter_->SetPresentationContext(nullptr);
    [badge_mediator_ disconnect];
  }

  // Appends a new WebState to the WebStateList and activates it.
  void AppendActivatedWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state->SetBrowserState(profile());
    InfoBarManagerImpl::CreateForWebState(web_state.get());
    InfobarBadgeTabHelper::GetOrCreateForWebState(web_state.get());
    web_state_list()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  // Adds an Infobar of `type` to the InfoBarManager and returns the infobar.
  // Pass in different `message_text` to avoid replacing existing infobar.
  InfoBarIOS* AddInfobar(InfobarType type, std::u16string message_text) {
    std::unique_ptr<InfoBarIOS> added_infobar =
        std::make_unique<FakeInfobarIOS>(type, message_text);
    InfoBarIOS* infobar = added_infobar.get();
    infobar_manager()->AddInfoBar(std::move(added_infobar));
    return infobar;
  }

  // Removes `infobar` from its manager.
  void RemoveInfobar(InfoBarIOS* infobar) {
    infobar_manager()->RemoveInfoBar(infobar);
  }

  // Returns whether the test fixture is for an incognito BrowserState.
  bool is_off_the_record() const {
    return GetParam() == TestParam::kOffTheRecord;
  }
  // Returns the BrowserState to use for the test fixture.
  ProfileIOS* profile() {
    return is_off_the_record() ? profile_->GetOffTheRecordProfile()
                               : profile_.get();
  }
  // Returns the Browser to use for the test fixture.
  Browser* browser() { return browser_.get(); }
  // Returns the Browser's WebStateList.
  WebStateList* web_state_list() { return browser()->GetWebStateList(); }
  // Returns the active WebState.
  web::WebState* web_state() { return web_state_list()->GetActiveWebState(); }
  // Returns the active WebState's InfoBarManagerImpl.
  InfoBarManagerImpl* infobar_manager() {
    return InfoBarManagerImpl::FromWebState(web_state());
  }
  // Returns the active WebState's InfobarBadgeTabHelper.
  InfobarBadgeTabHelper* tab_helper() {
    return InfobarBadgeTabHelper::GetOrCreateForWebState(web_state());
  }

  base::test::TaskEnvironment environment_;
  FakeBadgeConsumer* badge_consumer_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  FakeOverlayPresentationContext overlay_presentation_context_;
  BadgeMediator* badge_mediator_ = nil;
  raw_ptr<OverlayPresenter> overlay_presenter_ = nullptr;
};

// Test that the BadgeMediator responds with no displayed and fullscreen badge
// when there are no Infobars added and the BrowserState is not OffTheRecord.
TEST_P(BadgeMediatorTest, BadgeMediatorTestNoInfobar) {
  AppendActivatedWebState();
  EXPECT_FALSE(badge_consumer_.displayedBadge);
  EXPECT_EQ(is_off_the_record(),
            badge_consumer_.hasFullscreenOffTheRecordBadge);
}

// Test that the BadgeMediator responds with one new badge when an infobar is
// added
TEST_P(BadgeMediatorTest, BadgeMediatorTestAddInfobar) {
  AppendActivatedWebState();
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType, kBadgeTypePasswordSave);
}

// Test that the BadgeMediator handled the removal of the correct badge when two
// infobars are added and then one is removed.
TEST_P(BadgeMediatorTest, BadgeMediatorTestRemoveInfobar) {
  AppendActivatedWebState();
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  InfoBarIOS* second_infobar =
      AddInfobar(kSecondInfobarType, kSecondInfobarMessageText);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType, kBadgeTypeOverflow);
  RemoveInfobar(second_infobar);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType, kBadgeTypePasswordSave);
}

TEST_P(BadgeMediatorTest, BadgeMediatorTestMarkAsRead) {
  AppendActivatedWebState();
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  // Since there is only one badge, it should be marked as read.
  EXPECT_FALSE(badge_consumer_.hasUnreadBadge);
  AddInfobar(kSecondInfobarType, kSecondInfobarMessageText);
  ASSERT_EQ(kBadgeTypeOverflow, badge_consumer_.displayedBadge.badgeType);
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
  AppendActivatedWebState();
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType, kBadgeTypePasswordSave);
  AppendActivatedWebState();
  EXPECT_FALSE(badge_consumer_.displayedBadge);
}

// Test that the BadgeMediator does not inform its consumer of a new infobar if
// the added infobar came from an inactive WebState.
TEST_P(BadgeMediatorTest,
       BadgeMediatorTestSwitchWebStateAndAddInfobarToInactiveWebState) {
  AppendActivatedWebState();
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType, kBadgeTypePasswordSave);
  AppendActivatedWebState();
  std::unique_ptr<InfoBarIOS> added_infobar = std::make_unique<FakeInfobarIOS>(
      kSecondInfobarType, kSecondInfobarMessageText);
  InfoBarManagerImpl::FromWebState(web_state_list()->GetWebStateAt(0))
      ->AddInfoBar(std::move(added_infobar));
  EXPECT_FALSE(badge_consumer_.displayedBadge);
}

// Test that the BadgeMediator does not inform its consumer of a new infobar it
// has already been disconnected.
TEST_P(BadgeMediatorTest, BadgeMediatorTestDoNotAddInfobarIfWebStateListGone) {
  AppendActivatedWebState();
  ASSERT_FALSE(badge_consumer_.displayedBadge);
  [badge_mediator_ disconnect];
  std::unique_ptr<InfoBarIOS> added_infobar = std::make_unique<FakeInfobarIOS>(
      kSecondInfobarType, kSecondInfobarMessageText);
  InfoBarManagerImpl::FromWebState(web_state_list()->GetActiveWebState())
      ->AddInfoBar(std::move(added_infobar));
  EXPECT_FALSE(badge_consumer_.displayedBadge);
}

// Test that the BadgeMediator updates the badge when it is accepted.
TEST_P(BadgeMediatorTest, BadgeMediatorTestAcceptedBadge) {
  AppendActivatedWebState();
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
  AppendActivatedWebState();
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);

  // Simulate reload of app, but preservation of WebStateList.
  [badge_mediator_ disconnect];
  badge_mediator_ = nil;
  badge_consumer_ = nil;

  badge_consumer_ = [[FakeBadgeConsumer alloc] init];
  badge_mediator_ =
      [[BadgeMediator alloc] initWithWebStateList:web_state_list()
                                 overlayPresenter:overlay_presenter_
                                      isIncognito:is_off_the_record()];
  badge_mediator_.consumer = badge_consumer_;
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType, kBadgeTypePasswordSave);
}

// Test that the BadgeMediator clears its badges when the last WebState is
// detached and a new WebState is added. This test also makes sure that closing
// the last WebState doesn't break anything.
TEST_P(BadgeMediatorTest, BadgeMediatorTestCloseLastTab) {
  AppendActivatedWebState();
  AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);
  ASSERT_TRUE(badge_consumer_.displayedBadge);
  EXPECT_EQ(badge_consumer_.displayedBadge.badgeType, kBadgeTypePasswordSave);
  web_state_list()->DetachWebStateAt(0);
  AppendActivatedWebState();
  ASSERT_FALSE(badge_consumer_.displayedBadge);
}

// Tests that the badge mediator successfully updates the InfobarBadgeTabHelper
// for the active WebState for infobar banner presentation and dismissal.
TEST_P(BadgeMediatorTest, InfobarBannerOverlayObserving) {
  // Add an active WebState at index 0 and add an InfoBar with `type` to the
  // WebState's InfoBarManager, checking that the badge item has been created
  // with the default BadgeState.
  AppendActivatedWebState();
  InfobarType type = kFirstInfobarType;
  InfobarBadgeTabHelper* tab_helper =
      InfobarBadgeTabHelper::GetOrCreateForWebState(web_state());
  InfoBarIOS* infobar = AddInfobar(kFirstInfobarType, kFirstInfobarMessageText);

  std::map<InfobarType, BadgeState> badge_states =
      tab_helper->GetInfobarBadgeStates();
  ASSERT_EQ(1U, badge_states.size());
  ASSERT_TRUE(base::Contains(badge_states, type));
  BadgeState state = badge_states[type];
  ASSERT_FALSE(state & BadgeStatePresented);

  // Simulate the presentation of the infobar banner via OverlayPresenter in the
  // fake presentation context, verifying that the badge state is updated
  // accordingly.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state(), OverlayModality::kInfobarBanner);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          infobar, InfobarOverlayType::kBanner, infobar->high_priority()));
  badge_states = tab_helper->GetInfobarBadgeStates();
  EXPECT_TRUE(badge_states[type] & BadgeStatePresented);

  // Simulate dismissal of the banner and verify that the badge state is no
  // longer presented.
  queue->CancelAllRequests();
  badge_states = tab_helper->GetInfobarBadgeStates();
  EXPECT_FALSE(badge_states[type] & BadgeStatePresented);
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         BadgeMediatorTest,
                         testing::Values(TestParam::kNormal,
                                         TestParam::kOffTheRecord));
