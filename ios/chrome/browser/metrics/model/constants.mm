// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/constants.h"

const char kActivityBucketKey[] = "FeedActivityBucket";
const char kAllFeedsActivityBucketsByProviderHistogram[] =
    "ContentSuggestions.Feed.AllFeeds.Activity.ByProvider";
const char kNotifAuthorizationStatusByProviderHistogram[] =
    "IOS.PushNotification.NotificationSettingsAuthorizationStatus.ByProvider";
const char kContentNotifClientStatusByProviderHistogram[] =
    "ContentNotifications.ClientStatus.Enabled.ByProvider";
const char kSportsNotifClientStatusByProviderHistogram[] =
    "ContentNotifications.ClientStatus.Sports.ByProvider";
const char kTipsNotifClientStatusByProviderHistogram[] =
    "IOS.Notifications.Tips.ClientStatus.Enabled.ByProvider";
const char kSafetyCheckNotifClientStatusByProviderHistogram[] =
    "IOS.Notifications.SafetyCheck.ClientStatus.Enabled.ByProvider";
const char kSendTabNotifClientStatusByProviderHistogram[] =
    "IOS.Notifications.SendTab.ClientStatus.Enabled.ByProvider";
const char kFeedEnabledHistogram[] = "ContentSuggestions.Feed.CanBeShown";
