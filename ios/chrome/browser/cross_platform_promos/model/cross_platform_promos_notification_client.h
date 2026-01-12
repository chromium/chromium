// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_NOTIFICATION_CLIENT_H_

#import <optional>

#import "base/sequence_checker.h"
#import "components/desktop_to_mobile_promos/promos_types.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

// Client for handling cross-platform promos notifications.
class CrossPlatformPromosNotificationClient : public PushNotificationClient {
 public:
  explicit CrossPlatformPromosNotificationClient(ProfileIOS* profile);
  ~CrossPlatformPromosNotificationClient() override;

  // PushNotificationClient implementation.
  bool CanHandleNotification(UNNotification* notification) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* user_info) override;
  bool HandleNotificationInteraction(UNNotificationResponse* response) override;
  std::optional<NotificationType> GetNotificationType(
      UNNotification* notification) override;
  void OnSceneActiveForegroundBrowserReady() override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // The promo type that is waiting to be shown once a browser is ready.
  std::optional<desktop_to_mobile_promos::PromoType> pending_promo_type_;

  // Shows the promo corresponding to `promo_type` in `browser`.
  void ShowPromo(desktop_to_mobile_promos::PromoType promo_type,
                 Browser* browser);
};

#endif  // IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_NOTIFICATION_CLIENT_H_
