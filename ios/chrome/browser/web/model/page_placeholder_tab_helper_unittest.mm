// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Test fixture for PagePlaceholderTabHelper class.
class PagePlaceholderTabHelperTest : public PlatformTest {
 protected:
  PagePlaceholderTabHelperTest() {
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());

    CGRect frame = {CGPointZero, CGSizeMake(400, 300)};
    web_state_view_ = [[UIView alloc] initWithFrame:frame];
    web_state_view_.backgroundColor = [UIColor blueColor];
    web_state_->SetView(web_state_view_);

    [scoped_key_window_.Get() addSubview:web_state_view_];

    // The Content Area named guide should be available.
    NamedGuide* guide = [[NamedGuide alloc] initWithName:kContentAreaGuide];
    [web_state_view_ addLayoutGuide:guide];

    // PagePlaceholderTabHelper uses SnapshotTabHelper, so ensure it has been
    // created.
    SnapshotTabHelper::CreateForWebState(web_state_.get());
    PagePlaceholderTabHelper::CreateForWebState(web_state_.get());
  }

  PagePlaceholderTabHelper* tab_helper() {
    return PagePlaceholderTabHelper::FromWebState(web_state_.get());
  }

  web::WebTaskEnvironment task_environment_;
  ScopedKeyWindow scoped_key_window_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  UIView* web_state_view_ = nil;
};

// Tests that placeholder is not shown after WasShown() if it was not requested.
TEST_F(PagePlaceholderTabHelperTest, TabShownAndPlaceholderNotShown) {
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->WasShown();
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that placeholder is shown after WasShown() if it was requested.
TEST_F(PagePlaceholderTabHelperTest, TabShownAndPlaceholderShown) {
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->WasShown();
  EXPECT_TRUE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that placeholder is removed after WasHidden().
TEST_F(PagePlaceholderTabHelperTest, TabHiddenAndPlaceholderRemoved) {
  tab_helper()->AddPlaceholderForNextNavigation();
  web_state_->WasShown();
  ASSERT_TRUE(tab_helper()->displaying_placeholder());
  web_state_->WasHidden();
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
}

// Tests that placeholder is not shown after DidStartNavigation if it was not
// requested.
TEST_F(PagePlaceholderTabHelperTest, NotShown) {
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->OnNavigationStarted(nullptr);
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that placehold is not shown after DidStartNavigation if it was
// cancelled before the navigation.
TEST_F(PagePlaceholderTabHelperTest, NotShownIfCancelled) {
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->CancelPlaceholderForNextNavigation();
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->OnNavigationStarted(nullptr);
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that placeholder is shown between DidStartNavigation/PageLoaded
// WebStateObserver callbacks.
TEST_F(PagePlaceholderTabHelperTest, Shown) {
  web_state_->WasShown();
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());

  web_state_->OnNavigationStarted(nullptr);
  EXPECT_TRUE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());

  web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that placeholder is not shown if the tab is not visible at
// DidStartNavigation
TEST_F(PagePlaceholderTabHelperTest, NotShownIfTabNotVisible) {
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());

  web_state_->OnNavigationStarted(nullptr);
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  web_state_->WasShown();
  EXPECT_TRUE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that placeholder is removed if cancelled while presented.
TEST_F(PagePlaceholderTabHelperTest, RemovedIfCancelledWhileShown) {
  web_state_->WasShown();
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->OnNavigationStarted(nullptr);
  EXPECT_TRUE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());

  tab_helper()->CancelPlaceholderForNextNavigation();
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that destructing WebState removes the placeholder.
TEST_F(PagePlaceholderTabHelperTest, DestructWebStateWhenShowingPlaceholder) {
  web_state_->WasShown();
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(tab_helper()->displaying_placeholder());

  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->OnNavigationStarted(nullptr);

  EXPECT_TRUE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  EXPECT_TRUE([[web_state_view_ subviews] count] != 0);
  web_state_.reset();

  // The tab helper has been deleted at this point, so do not check the value
  // of displaying_placeholder(). Check that the view has no subviews (i.e. no
  // placeholder is presented) after some time (since the placeholder view is
  // removed with an animation).
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [[web_state_view_ subviews] count] == 0;
  }));
}
