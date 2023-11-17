// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_

#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_push_notification_client.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

class CommercePushNotificationClientTest;

class Browser;

namespace base {
class RunLoop;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace commerce {
class ShoppingService;
}  // namespace commerce

class CommercePushNotificationClient
    : public OptimizationGuidePushNotificationClient {
 public:
  CommercePushNotificationClient();
  ~CommercePushNotificationClient() override;

  // Override PushNotificationClient::
  void HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
  void OnSceneActiveForegroundBrowserReady() override;

 private:
  friend class ::CommercePushNotificationClientTest;

  commerce::ShoppingService* GetShoppingService();
  bookmarks::BookmarkModel* GetBookmarkModel();
  // Returns the first active browser found with scene level
  // SceneActivationLevelForegroundActive.
  Browser* GetSceneLevelForegroundActiveBrowser();

  std::vector<const std::string> urls_delayed_for_loading_;

  // Handle the interaction from the user be it tapping the notification or
  // long pressing and then presing 'Visit Site' or 'Untrack Price'.
  void HandleNotificationInteraction(
      NSString* action_identifier,
      NSDictionary* user_info,
      base::RunLoop* on_complete_for_testing = nil);
};
#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_
