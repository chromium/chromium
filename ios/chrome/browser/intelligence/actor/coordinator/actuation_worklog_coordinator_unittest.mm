// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Creates an ActorFormSuggestion with password credentials for testing.
ActorFormSuggestion* CreateCredentialSuggestion(
    const std::u16string& username,
    const std::u16string& display_origin) {
  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(1);
  cred.type = actor_login::CredentialType::kPassword;
  cred.username = username;
  cred.display_origin = display_origin;
  return [[ActorFormSuggestion alloc] initWithCredential:cred];
}

class ActuationWorklogCoordinatorTest : public PlatformTest {
 protected:
  ActuationWorklogCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
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

// Tests that setting the presented suggestions again only updates the worklog
// item on the suggestions coordinator.
TEST_F(ActuationWorklogCoordinatorTest,
       TestSetPresentedSuggestionsUpdatesWorklogItem) {
  ActuationWorklogCoordinator* coordinator =
      [[ActuationWorklogCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()];

  [coordinator start];
  ASSERT_NE(coordinator.viewController, nil);

  id<ActuationSuggestionsConsumer> consumer =
      static_cast<id<ActuationSuggestionsConsumer>>(coordinator);

  NSArray<ActorFormSuggestion*>* suggestions =
      @[ CreateCredentialSuggestion(u"user@example.com", u"example.com") ];
  ActuationWorklogItem* item = [ActuationWorklogItem simpleItemWithTitle:@"Item"
                                                                  active:YES];
  [consumer setSuggestions:suggestions forActiveWorklogItem:item];
  id<ActuationSuggestionsPresenter> presenter =
      coordinator.viewController.suggestionsPresenter;
  ASSERT_NE(presenter, nil);

  ActuationSuggestionsCoordinator* suggestions_coordinator =
      base::apple::ObjCCastStrict<ActuationSuggestionsCoordinator>(presenter);
  EXPECT_EQ(suggestions_coordinator.worklogItem, item);

  ActuationWorklogItem* updated_item =
      [ActuationWorklogItem simpleItemWithTitle:@"Updated" active:YES];
  [consumer setSuggestions:suggestions forActiveWorklogItem:updated_item];
  EXPECT_EQ(coordinator.viewController.suggestionsPresenter, presenter);
  EXPECT_EQ(suggestions_coordinator.worklogItem, updated_item);

  [coordinator stop];
}

// Tests that new suggestions replace the presented ones with a new suggestions
// presenter.
TEST_F(ActuationWorklogCoordinatorTest,
       TestSetNewSuggestionsReplacesPresenter) {
  ActuationWorklogCoordinator* coordinator =
      [[ActuationWorklogCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()];

  [coordinator start];
  ASSERT_NE(coordinator.viewController, nil);

  id<ActuationSuggestionsConsumer> consumer =
      static_cast<id<ActuationSuggestionsConsumer>>(coordinator);
  ActuationWorklogItem* item = [ActuationWorklogItem simpleItemWithTitle:@"Item"
                                                                  active:YES];

  [consumer setSuggestions:@[ CreateCredentialSuggestion(u"user1@example.com",
                                                         u"example.com") ]
      forActiveWorklogItem:item];
  id<ActuationSuggestionsPresenter> presenter1 =
      coordinator.viewController.suggestionsPresenter;
  ASSERT_NE(presenter1, nil);

  [consumer setSuggestions:@[ CreateCredentialSuggestion(u"user2@example.com",
                                                         u"example.com") ]
      forActiveWorklogItem:item];
  id<ActuationSuggestionsPresenter> presenter2 =
      coordinator.viewController.suggestionsPresenter;
  EXPECT_NE(presenter2, nil);
  EXPECT_NE(presenter1, presenter2);

  [coordinator stop];
}

// Tests that stopping a suggestions intervention and restarting it cleanly
// recreates and reattaches the suggestions presenter.
TEST_F(ActuationWorklogCoordinatorTest,
       TestStopAndRestartSuggestionsIntervention) {
  ActuationWorklogCoordinator* coordinator =
      [[ActuationWorklogCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()];

  [coordinator start];
  ASSERT_NE(coordinator.viewController, nil);
  EXPECT_EQ(coordinator.viewController.suggestionsPresenter, nil);

  id<ActuationSuggestionsConsumer> consumer =
      static_cast<id<ActuationSuggestionsConsumer>>(coordinator);

  ActorFormSuggestion* suggestion1 =
      CreateCredentialSuggestion(u"user1@example.com", u"example.com");
  ActuationWorklogItem* item1 =
      [ActuationWorklogItem simpleItemWithTitle:@"Item 1" active:YES];
  [consumer setSuggestions:@[ suggestion1 ] forActiveWorklogItem:item1];
  id<ActuationSuggestionsPresenter> presenter1 =
      coordinator.viewController.suggestionsPresenter;
  EXPECT_NE(presenter1, nil);

  [consumer setSuggestions:nil forActiveWorklogItem:nil];
  EXPECT_EQ(coordinator.viewController.suggestionsPresenter, nil);

  ActorFormSuggestion* suggestion2 =
      CreateCredentialSuggestion(u"user2@example.com", u"example.com");
  ActuationWorklogItem* item2 =
      [ActuationWorklogItem simpleItemWithTitle:@"Item 2" active:YES];
  [consumer setSuggestions:@[ suggestion2 ] forActiveWorklogItem:item2];
  id<ActuationSuggestionsPresenter> presenter2 =
      coordinator.viewController.suggestionsPresenter;
  EXPECT_NE(presenter2, nil);
  EXPECT_NE(presenter1, presenter2);

  [coordinator stop];
}

// Tests that stopping task observation clears suggestions so that suggestions
// can be set again when observation resumes (e.g. when the floaty is closed
// and reopened).
TEST_F(ActuationWorklogCoordinatorTest,
       TestStopObservingTaskClearsSuggestions) {
  ActuationWorklogCoordinator* coordinator =
      [[ActuationWorklogCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()];

  [coordinator start];
  ASSERT_NE(coordinator.viewController, nil);

  id<ActuationSuggestionsConsumer> consumer =
      static_cast<id<ActuationSuggestionsConsumer>>(coordinator);

  ActorFormSuggestion* suggestion1 =
      CreateCredentialSuggestion(u"user1@example.com", u"example.com");
  ActuationWorklogItem* item1 =
      [ActuationWorklogItem simpleItemWithTitle:@"Item 1" active:YES];
  [consumer setSuggestions:@[ suggestion1 ] forActiveWorklogItem:item1];
  EXPECT_NE(coordinator.viewController.suggestionsPresenter, nil);

  // Closing the floaty or ending observation cleans up suggestions.
  [coordinator stopObservingDataSource];
  EXPECT_EQ(coordinator.viewController.suggestionsPresenter, nil);

  // Reopening the floaty and receiving suggestions succeeds cleanly.
  ActorFormSuggestion* suggestion2 =
      CreateCredentialSuggestion(u"user2@example.com", u"example.com");
  ActuationWorklogItem* item2 =
      [ActuationWorklogItem simpleItemWithTitle:@"Item 2" active:YES];
  [consumer setSuggestions:@[ suggestion2 ] forActiveWorklogItem:item2];
  EXPECT_NE(coordinator.viewController.suggestionsPresenter, nil);

  [coordinator stop];
}

}  // namespace
