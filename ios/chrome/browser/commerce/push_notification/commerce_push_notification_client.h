// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_COMMERCE_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_

#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/commerce/shopping_service_factory.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_push_notification_client.h"
#import "ios/chrome/browser/push_notification/push_notification_client.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

class CommercePushNotificationClientTest;

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

 private:
  friend class ::CommercePushNotificationClientTest;

  commerce::ShoppingService* GetShoppingService();
  bookmarks::BookmarkModel* GetBookmarkModel();

  // Handle the interaction from the user be it tapping the notification or
  // long pressing and then presing 'Visit Site' or 'Untrack Price'.
  void HandleNotificationInteraction(
      NSString* action_identifier,
      NSDictionary* user_info,
      base::RunLoop* on_complete_for_testing = nil);
};
#endif  // IOS_CHROME_BROWSER_COMMERCE_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_
