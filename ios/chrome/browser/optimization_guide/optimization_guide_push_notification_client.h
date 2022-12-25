// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PUSH_NOTIFICATION_CLIENT_H_

#import "components/optimization_guide/proto/push_notification.pb.h"
#import "ios/chrome/browser/push_notification/push_notification_client.h"

#import <UIKit/UIKit.h>

class ChromeBrowserState;

// Abstract class to be inherited from by push notification clients which
// utilize the OptimizationGuide push notification infrastructure.
class OptimizationGuidePushNotificationClient : public PushNotificationClient {
 public:
  OptimizationGuidePushNotificationClient(
      const PushNotificationClientId& push_notification_client_id);
  ~OptimizationGuidePushNotificationClient() override;

  // PushNotificationClient
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;

  // Convert escaped serialized payload from push notification into
  // optimization_guide::proto::HintNotificationPayload. This method is used
  // by OptimizationGuidePushNotificationClient as well as inherited classes
  // to access and deserialize their payload.
  static std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
  ParseHintNotificationPayload(NSString* serialized_payload_escaped);

  // Allows tests to set the last used ChromeBrowserState returned in
  // GetLastUsedBrowserState().
  void SetLastUsedChromeBrowserStateForTesting(
      ChromeBrowserState* chrome_browser_state) {
    last_used_browser_state_for_testing_ = chrome_browser_state;
  }

 protected:
  ChromeBrowserState* GetLastUsedBrowserState();

 private:
  // Allows tests to override the last used ChromeBrowserState returned in
  // GetLastUsedBrowserState().
  ChromeBrowserState* last_used_browser_state_for_testing_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PUSH_NOTIFICATION_CLIENT_H_
