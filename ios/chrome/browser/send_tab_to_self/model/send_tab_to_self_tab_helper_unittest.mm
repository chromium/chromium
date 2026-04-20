// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/send_tab_to_self/features.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class SendTabToSelfTabHelperTest : public PlatformTest {
 protected:
  SendTabToSelfTabHelperTest() {
    feature_list_.InitAndEnableFeature(
        send_tab_to_self::kSendTabToSelfPropagateScrollPosition);

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_state_.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                   std::move(frames_manager));

    SendTabToSelfTabHelper::CreateForWebState(&web_state_);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_;
};

// Tests that the tab helper handles cases where there is no navigation item.
TEST_F(SendTabToSelfTabHelperTest, NoNavigationItem) {
  // Trigger page loaded with no items in the navigation manager.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
}

// Tests that the tab helper does nothing when the page load failed.
TEST_F(SendTabToSelfTabHelperTest, PageLoadFailed) {
  // Add a fake item.
  auto item = web::NavigationItem::Create();
  navigation_manager_->SetLastCommittedItem(item.get());

  // Trigger page loaded with FAILURE.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
}

// Tests that the tab helper does nothing when there is no text fragment to
// scroll to.
TEST_F(SendTabToSelfTabHelperTest, NoTextFragment) {
  auto item = web::NavigationItem::Create();
  // Don't set internal scroll to text fragment.
  navigation_manager_->SetLastCommittedItem(item.get());

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
}

// Tests that the tab helper handles an empty text fragment correctly.
TEST_F(SendTabToSelfTabHelperTest, EmptyTextFragment) {
  auto item = web::NavigationItem::Create();
  // Set empty fragment.
  item->SetInternalScrollToTextFragment("");
  navigation_manager_->SetLastCommittedItem(item.get());

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
}

// Tests that the tab helper successfully triggers a scroll when a valid text
// fragment is present.
TEST_F(SendTabToSelfTabHelperTest, ValidTextFragment) {
  auto item = web::NavigationItem::Create();
  item->SetInternalScrollToTextFragment("start,end");
  navigation_manager_->SetLastCommittedItem(item.get());

  EXPECT_TRUE(item->GetInternalScrollToTextFragment().has_value());

  // We can't easily mock the JS invocation in this unit test without
  // refactoring the generator to be injectable, but we can verify it doesn't
  // crash and that the execution path handles the fragment.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  // Verify the fragment was cleared so it doesn't run again on reload.
  EXPECT_FALSE(item->GetInternalScrollToTextFragment().has_value());
}
