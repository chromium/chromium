// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_mediator.h"

#import "ios/chrome/browser/badges/ui_bundled/badge_item.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_type.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_type_util.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_consumer.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake of IncognitoBadgeConsumer.
@interface FakeIncognitoBadgeConsumer : NSObject <IncognitoBadgeConsumer>
@property(nonatomic, assign) BOOL hasFullscreenOffTheRecordBadge;
@end

@implementation FakeIncognitoBadgeConsumer
@synthesize disabled = _disabled;

- (void)setupWithIncognitoBadge:(id<BadgeItem>)incognitoBadgeItem {
  self.hasFullscreenOffTheRecordBadge =
      incognitoBadgeItem != nil &&
      incognitoBadgeItem.badgeType == kBadgeTypeIncognito;
}

@end

class IncognitoBadgeMediatorTest : public PlatformTest {
 protected:
  IncognitoBadgeMediatorTest()
      : badge_consumer_([[FakeIncognitoBadgeConsumer alloc] init]),
        profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile())) {
    badge_mediator_ =
        [[IncognitoBadgeMediator alloc] initWithWebStateList:web_state_list()];
    badge_mediator_.consumer = badge_consumer_;
  }

  ~IncognitoBadgeMediatorTest() override { [badge_mediator_ disconnect]; }

  // Appends a new WebState to the WebStateList and activates it.
  void AppendActivatedWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state->SetBrowserState(profile());
    web_state_list()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  // Returns the BrowserState to use for the test fixture.
  ProfileIOS* profile() { return profile_->GetOffTheRecordProfile(); }
  // Returns the Browser to use for the test fixture.
  Browser* browser() { return browser_.get(); }
  // Returns the Browser's WebStateList.
  WebStateList* web_state_list() { return browser()->GetWebStateList(); }
  // Returns the active WebState.
  web::WebState* web_state() { return web_state_list()->GetActiveWebState(); }

  web::WebTaskEnvironment environment_;
  FakeIncognitoBadgeConsumer* badge_consumer_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  IncognitoBadgeMediator* badge_mediator_ = nil;
};

// Test that the BadgeMediator responds with no displayed and incognito badge
// when there are no Infobars added and the BrowserState is not OffTheRecord.
TEST_F(IncognitoBadgeMediatorTest, ShowsOfftheRecordBadge) {
  AppendActivatedWebState();
  EXPECT_TRUE(badge_consumer_.hasFullscreenOffTheRecordBadge);
}
