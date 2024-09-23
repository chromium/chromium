// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_METRICS_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_METRICS_H_

// Interactions with the Notifications Opt-In prompt. This is mapped to
// the IOSNotificationsOptInActionOnPrompt enum in enums.xml for metrics.
// LINT.IfChange
enum class NotificationsOptInPromptActionType {
  kSwipedToDismiss = 0,
  kNoThanksTapped = 1,
  kEnableNotificationsTapped = 2,
  kMaxValue = kEnableNotificationsTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

#pragma mark - Actions
extern const char kNotificationsOptInPromptContentEnabled[];
extern const char kNotificationsOptInPromptTipsEnabled[];
extern const char kNotificationsOptInPromptPriceTrackingEnabled[];
extern const char kNotificationsOptInPromptSafetyCheckEnabled[];
extern const char kNotificationsOptInPromptSendTabEnabled[];
extern const char kNotificationsOptInAlertPermissionDenied[];
extern const char kNotificationsOptInAlertPermissionGranted[];
extern const char kNotificationsOptInAlertOpenedSettings[];
extern const char kNotificationsOptInAlertCancelled[];
extern const char kNotificationsOptInAlertError[];

#pragma mark - Histograms
extern const char kNotificationsOptInPromptActionHistogram[];

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_METRICS_H_
