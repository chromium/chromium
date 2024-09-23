// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_client.h"

#import <UserNotifications/UserNotifications.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/threading/thread_restrictions.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class ContentNotificationClientTest : public PlatformTest {
 protected:
  ContentNotificationClientTest() {
    ProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
    BrowserList* list = BrowserListFactory::GetForProfile(profile);
    mock_scene_state_ = OCMClassMock([SceneState class]);
    OCMStub([mock_scene_state_ activationLevel])
        .andReturn(SceneActivationLevelForegroundActive);
    browser_ = std::make_unique<TestBrowser>(profile, mock_scene_state_);
    list->AddBrowser(browser_.get());
    client_ = std::make_unique<ContentNotificationClient>();
    ScopedDictPrefUpdate update(GetApplicationContext()->GetLocalState(),
                                prefs::kAppLevelPushNotificationPermissions);
    update->Set(kContentNotificationKey, true);
  }

  NSDictionary<NSString*, id>* CreatePayload(BOOL IsContentNotification) {
    if (IsContentNotification) {
      NSMutableDictionary<NSString*, id>* payload =
          [[NSMutableDictionary alloc] init];
      [payload setObject:@"Mock Body"
                  forKey:@"kContentNotificationNAUBodyParameter"];
      return payload;
    }
    return nil;
  }

  id CreateContent(BOOL IsContentNotification) {
    UNMutableNotificationContent* content =
        [[UNMutableNotificationContent alloc] init];
    content.title = @"Mock Title";
    content.body = @"Mock Body";
    content.categoryIdentifier = @"FEEDBACK_IDENTIFIER";
    content.userInfo = CreatePayload(IsContentNotification);
    return content;
  }

  id CreateRequest(BOOL IsContentNotification) {
    UNNotificationRequest* request = [UNNotificationRequest
        requestWithIdentifier:@"CONTENT"
                      content:CreateContent(IsContentNotification)
                      trigger:nil];
    return request;
  }

  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  id mock_scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<ContentNotificationClient> client_;
  id mock_notification_center_;
  std::unique_ptr<ScopedBlockSwizzler> notification_center_swizzler_;
  id mock_application_handler_;
};

#pragma mark - Test cases

// Tests that HandleNotificationReception does nothing and returns "NoData".
TEST_F(ContentNotificationClientTest, HandleNotificationReception) {
  EXPECT_EQ(client_->HandleNotificationReception(CreatePayload(NO)),
            std::nullopt);
}

// Tests the appropriate secondary actions are registered.
TEST_F(ContentNotificationClientTest, RegisterActionableNotifications) {
  NSArray<UNNotificationCategory*>* secondaryActions =
      client_->RegisterActionableNotifications();
  EXPECT_EQ(secondaryActions.firstObject.identifier,
            kContentNotificationFeedbackCategoryIdentifier);
}
