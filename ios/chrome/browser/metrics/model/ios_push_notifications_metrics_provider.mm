// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_push_notifications_metrics_provider.h"

#import <UserNotifications/UserNotifications.h>

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"

IOSPushNotificationsMetricsProvider::IOSPushNotificationsMetricsProvider(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  CHECK(identity_manager_);
}

IOSPushNotificationsMetricsProvider::~IOSPushNotificationsMetricsProvider() {}

void IOSPushNotificationsMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Retrieve OS notification auth status.
  [PushNotificationUtil getPermissionSettings:^(
                            UNNotificationSettings* settings) {
    base::UmaHistogramExactLinear(kNotifAuthorizationStatusByProviderHistogram,
                                  settings.authorizationStatus, 5);
  }];
  // Report the enabled client IDs.
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    base::UmaHistogramBoolean(
        kNotifClientStatusByProviderHistogram,
        push_notification_settings::
            GetMobileNotificationPermissionStatusForClient(
                PushNotificationClientId::kContent,
                identity_manager_
                    ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                    .gaia));
  }
}
