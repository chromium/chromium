// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/constants.h"

const char kCommerceNotificationKey[] = "PRICE_DROP";
const char kContentNotificationKey[] = "CONTENT";
const char kTipsNotificationKey[] = "TIPS";
const char kSportsNotificationKey[] = "SPORTS";
const char kSafetyCheckNotificationKey[] = "SAFETY_CHECK";
const char kSendTabNotificationKey[] = "SEND_TAB";

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
