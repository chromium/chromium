// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/complex_tasks/model/ios_task_tab_helper.h"

#import "base/time/time.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

class IOSTaskTabHelperTest : public PlatformTest {
 protected:
  web::NavigationItem* AddItemToFakeNavigationManager(
      web::FakeNavigationManager* test_navigation_manager,
      ui::PageTransition transition) {
    test_navigation_manager->AddItem(GURL(), transition);
    web::NavigationItem* navigation_item =
        test_navigation_manager->GetItemAtIndex(
            test_navigation_manager->GetLastCommittedItemIndex());
    test_navigation_manager->SetLastCommittedItem(navigation_item);
    navigation_item->SetTimestamp(base::Time::Now());
    return navigation_item;
  }

  web::NavigationItem* NavigateWithTransition(ui::PageTransition transition) {
    web::FakeNavigationContext context;
    web_state_.OnNavigationStarted(&context);
    web::NavigationItem* item =
        AddItemToFakeNavigationManager(static_cast<web::FakeNavigationManager*>(
                                           web_state_.GetNavigationManager()),
                                       transition);
    context.SetPageTransition(transition);
    web_state_.OnNavigationFinished(&context);
    return item;
  }

  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
};

// Tests Task ID relationship when navigating via clicking a link
TEST_F(IOSTaskTabHelperTest, TestLinkedNavigation) {
  web_state_.SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  IOSTaskTabHelper::CreateForWebState(&web_state_);

  web::NavigationItem* a_navigation_item =
      NavigateWithTransition(ui::PAGE_TRANSITION_LINK);
  const IOSContentRecordTaskId* a_ios_context_record_task_id =
      IOSTaskTabHelper::FromWebState(&web_state_)
          ->GetContextRecordTaskId(a_navigation_item->GetUniqueID());

  web::NavigationItem* b_navigation_item =
      NavigateWithTransition(ui::PAGE_TRANSITION_LINK);
  const IOSContentRecordTaskId* b_ios_context_record_task_id =
      IOSTaskTabHelper::FromWebState(&web_state_)
          ->GetContextRecordTaskId(b_navigation_item->GetUniqueID());

  EXPECT_EQ(a_ios_context_record_task_id->task_id(),
            b_ios_context_record_task_id->parent_task_id());
  EXPECT_EQ(a_ios_context_record_task_id->root_task_id(),
            b_ios_context_record_task_id->root_task_id());
}

// Tests Task ID relationship when navigating via typing e.g. typing
// a URL into the omnibox
TEST_F(IOSTaskTabHelperTest, TestTypedNavigation) {
  web_state_.SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  IOSTaskTabHelper::CreateForWebState(&web_state_);

  web::NavigationItem* a_navigation_item =
      NavigateWithTransition(ui::PAGE_TRANSITION_TYPED);
  const IOSContentRecordTaskId* a_ios_context_record_task_id =
      IOSTaskTabHelper::FromWebState(&web_state_)
          ->GetContextRecordTaskId(a_navigation_item->GetUniqueID());

  web::NavigationItem* b_navigation_item =
      NavigateWithTransition(ui::PAGE_TRANSITION_TYPED);
  const IOSContentRecordTaskId* b_ios_context_record_task_id =
      IOSTaskTabHelper::FromWebState(&web_state_)
          ->GetContextRecordTaskId(b_navigation_item->GetUniqueID());

  EXPECT_NE(a_ios_context_record_task_id->task_id(),
            b_ios_context_record_task_id->parent_task_id());
  EXPECT_NE(a_ios_context_record_task_id->root_task_id(),
            b_ios_context_record_task_id->root_task_id());
}
