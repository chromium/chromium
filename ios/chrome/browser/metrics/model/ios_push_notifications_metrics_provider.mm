// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_push_notifications_metrics_provider.h"

#import <UserNotifications/UserNotifications.h>

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"

IOSPushNotificationsMetricsProvider::IOSPushNotificationsMetricsProvider() {}

IOSPushNotificationsMetricsProvider::~IOSPushNotificationsMetricsProvider() {}

void IOSPushNotificationsMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Retrieve OS notification auth status.
  [PushNotificationUtil getPermissionSettings:^(
                            UNNotificationSettings* settings) {
    base::UmaHistogramExactLinear(kNotifAuthorizationStatusByProviderHistogram,
                                  settings.authorizationStatus, 5);
  }];
}
