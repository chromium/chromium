// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_ID_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_ID_H_

// The PushNotificationClientId class enumerates Chrome's push notification
// enabled features. The PushNotificationClientId is intended to be used as an
// identifier by the PushNotificationClientManager to assign ownership of an
// incoming push notification to a feature. As a result, the value a feature's
// PushNotificationClientId evaluates to must match the value inside an incoming
// push notification's `push_notification_client_id` for the
// PushNotificationClientManager to accurately associate the notification to the
// desired feature.
//
// Must be in sync with PushNotificationClientId enum in
// tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class PushNotificationClientId {
  kCommerce = 1,
  kContent = 2,
  kTips = 3,
  kSports = 4,
  kSafetyCheck = 5,
  kSendTab = 6,
  kMaxValue = kSendTab,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:PushNotificationClientId)

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_ID_H_
