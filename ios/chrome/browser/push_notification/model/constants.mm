// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/constants.h"

#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"

// LINT.IfChange(ClientId)
const char kCommerceNotificationKey[] = "PRICE_DROP";
const char kContentNotificationKey[] = "CONTENT";
const char kReminderNotificationKey[] = "REMINDER";
const char kSafetyCheckNotificationKey[] = "SAFETY_CHECK";
const char kSendTabNotificationKey[] = "SEND_TAB";
const char kSportsNotificationKey[] = "SPORTS";
const char kTipsNotificationKey[] = "TIPS";
const char kCrossPlatformPromosNotificationKey[] = "XPLAT_PROMOS";
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/histograms.xml:ClientId)

NSString* const kSendTabNotificationCategoryIdentifier = @"SendTabNotification";

NSString* const kContentNotificationFeedbackActionIdentifier = @"feedback";
NSString* const kContentNotificationFeedbackCategoryIdentifier =
    @"FEEDBACK_IDENTIFIER";

NSString* const kContentNotificationNAUBodyParameter =
    @"kContentNotificationNAUBodyParameter";

NSString* const kContentNotificationContentArrayKey =
    @"kContentNotificationContentArray";

const char kNAUHistogramName[] =
    "ContentNotifications.NotificationActionUpload.Success";

const char kContentNotificationActionHistogramName[] =
    "ContentNotifications.Notification.Action";

const int kDeliveredNAUMaxSendsPerSession = 30;

NSString* const kPushNotificationClientIdKey = @"push_notification_client_id";

NSString* const kOriginatingProfileNameKey = @"originating_profile_name";

NSString* const kOriginatingGaiaIDKey = @"SenderGaiaId";

std::string PushNotificationClientIdToString(
    PushNotificationClientId client_id) {
  switch (client_id) {
    case PushNotificationClientId::kCommerce:
      return kCommerceNotificationKey;
    case PushNotificationClientId::kContent:
      return kContentNotificationKey;
    case PushNotificationClientId::kTips:
      return kTipsNotificationKey;
    case PushNotificationClientId::kSports:
      return kSportsNotificationKey;
    case PushNotificationClientId::kSafetyCheck:
      return kSafetyCheckNotificationKey;
    case PushNotificationClientId::kSendTab:
      return kSendTabNotificationKey;
    case PushNotificationClientId::kReminders:
      return kReminderNotificationKey;
    case PushNotificationClientId::kCrossPlatformPromos:
      return kCrossPlatformPromosNotificationKey;
  }
}
