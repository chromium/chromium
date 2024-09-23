// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/push_notification/metrics.h"

#pragma mark - Actions

const char kNotificationsOptInPromptContentEnabled[] =
    "IOS.Notifications.OptInPrompt.ContentEnabled";
const char kNotificationsOptInPromptTipsEnabled[] =
    "IOS.Notifications.OptInPrompt.TipsEnabled";
const char kNotificationsOptInPromptPriceTrackingEnabled[] =
    "IOS.Notifications.OptInPrompt.PriceTrackingEnabled";
const char kNotificationsOptInPromptSafetyCheckEnabled[] =
    "IOS.Notifications.OptInPrompt.SafetyCheckEnabled";
const char kNotificationsOptInPromptSendTabEnabled[] =
    "IOS.Notifications.OptInPrompt.SendTabEnabled";
const char kNotificationsOptInAlertPermissionDenied[] =
    "IOS.Notifications.OptInAlert.PermissionDenied";
const char kNotificationsOptInAlertPermissionGranted[] =
    "IOS.Notifications.OptInAlert.PermissionGranted";
const char kNotificationsOptInAlertOpenedSettings[] =
    "IOS.Notifications.OptInAlert.OpenedSettings";
const char kNotificationsOptInAlertCancelled[] =
    "IOS.Notifications.OptInAlert.Cancelled";
const char kNotificationsOptInAlertError[] =
    "IOS.Notifications.OptInAlert.Error";

#pragma mark - Histograms

const char kNotificationsOptInPromptActionHistogram[] =
    "IOS.Notifications.OptInPrompt.Action";
