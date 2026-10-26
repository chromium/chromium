// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class ActuationWorklogCoordinatorTest : public PlatformTest {
 protected:
  ActuationWorklogCoordinatorTest() {
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    actor::ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_ = nil;
};

// Tests starting and stopping the coordinator.
TEST_F(ActuationWorklogCoordinatorTest, TestStartAndStopLifecycle) {
  ActuationWorklogCoordinator* coordinator =
      [[ActuationWorklogCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()];

  EXPECT_EQ(coordinator.viewController, nil);

  [coordinator start];
  EXPECT_NE(coordinator.viewController, nil);
  EXPECT_EQ(coordinator.viewController.suggestionsPresenter, nil);

  [coordinator stop];
  EXPECT_EQ(coordinator.viewController, nil);
}

// Tests that setting suggestions on the coordinator creates and attaches
// a suggestions presenter, and clearing suggestions removes it.
TEST_F(ActuationWorklogCoordinatorTest, TestSetAndClearSuggestions) {
  ActuationWorklogCoordinator* coordinator =
      [[ActuationWorklogCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()];

  [coordinator start];
  ASSERT_NE(coordinator.viewController, nil);
  EXPECT_EQ(coordinator.viewController.suggestionsPresenter, nil);

  ActuationWorklogItem* item = [ActuationWorklogItem simpleItemWithTitle:@"Item"
                                                                  active:YES];
  id<ActuationSuggestionsConsumer> consumer =
      static_cast<id<ActuationSuggestionsConsumer>>(coordinator);

  [consumer setSuggestions:@[] forActiveWorklogItem:item];
  EXPECT_NE(coordinator.viewController.suggestionsPresenter, nil);

  [consumer clearSuggestions];
  EXPECT_EQ(coordinator.viewController.suggestionsPresenter, nil);

  [coordinator stop];
}

}  // namespace
