// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_TEST_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_TEST_PUSH_NOTIFICATION_CLIENT_H_

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

class TestPushNotificationClient : public PushNotificationClient {
 public:
  TestPushNotificationClient(size_t client_id);
  ~TestPushNotificationClient() override;

  // Override PushNotificationClient::
  void HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;

  // Indicates whether the client has been
  bool HasNotificationReceivedInteraction();
  // Sets the client's UIBackgroundFetchResult to given FetchResult.
  void SetBackgroundFetchResult(UIBackgroundFetchResult result);
  void OnSceneActiveForegroundBrowserReady() override;
  bool IsBrowserReady();

 private:
  UIBackgroundFetchResult fetch_result_ = UIBackgroundFetchResultNoData;
  bool has_notification_received_interaction_ = false;
  bool is_browser_ready_ = false;
};
#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_TEST_PUSH_NOTIFICATION_CLIENT_H_
