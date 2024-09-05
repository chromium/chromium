// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "base/functional/callback_forward.h"
#import "components/optimization_guide/proto/push_notification.pb.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

class CommercePushNotificationClientTest;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace commerce {
class ShoppingService;
}  // namespace commerce

class CommercePushNotificationClient : public PushNotificationClient {
 public:
  CommercePushNotificationClient();
  ~CommercePushNotificationClient() override;

  // Override PushNotificationClient::
  bool HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;

  // Convert escaped serialized payload from push notification into
  // optimization_guide::proto::HintNotificationPayload.
  static std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
  ParseHintNotificationPayload(NSString* serialized_payload_escaped);

 private:
  friend class ::CommercePushNotificationClientTest;

  commerce::ShoppingService* GetShoppingService();
  bookmarks::BookmarkModel* GetBookmarkModel();

  // Handle the interaction from the user be it tapping the notification or
  // long pressing and then presing 'Visit Site' or 'Untrack Price'.
  bool HandleNotificationInteraction(NSString* action_identifier,
                                     NSDictionary* user_info,
                                     base::OnceClosure completion);
};

#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_
