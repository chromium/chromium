// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

#import "base/time/time.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"

using tips_notifications::IsTipsNotification;
using tips_notifications::NotificationType;

TipsNotificationClient::TipsNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kTips) {}

TipsNotificationClient::~TipsNotificationClient() = default;

void TipsNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  if (!IsTipsNotification(response.notification.request)) {
    return;
  }

  std::optional<NotificationType> type =
      tips_notifications::ParseType(response.notification.request);
  if (!type.has_value()) {
    // TODO(crbug.com/1519157): Add logging for this error condition.
    return;
  }
  HandleNotificationInteraction(type.value());
}

void TipsNotificationClient::HandleNotificationInteraction(
    NotificationType type) {
  switch (type) {
    case NotificationType::kDefaultBrowser:
      // TODO(crbug.com/1517910) implement DefaultBrowser interaction.
      break;
    case NotificationType::kWhatsNew:
      // TODO(crbug.com/1517911) implement What's New interaction.
      break;
    case NotificationType::kSignin:
      // TODO(crbug.com/1517912) implement Signin interaction.
      break;
  }
}

UIBackgroundFetchResult TipsNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
TipsNotificationClient::RegisterActionableNotifications() {
  return @[];
}

void TipsNotificationClient::OnSceneActiveForegroundBrowserReady() {
  ClearNotification();
  MaybeRequestNotification();
}

void TipsNotificationClient::ClearNotification() {
  UNUserNotificationCenter* notificationCenter =
      [UNUserNotificationCenter currentNotificationCenter];
  [notificationCenter removePendingNotificationRequestsWithIdentifiers:@[
    tips_notifications::kIdentifier
  ]];
}

void TipsNotificationClient::MaybeRequestNotification() {
  if (!IsFirstRunRecent(base::Days(14))) {
    return;
  }

  // The types of notifications that could be sent will be evaluated in the
  // order they appear in this array.
  static const NotificationType kTypes[] = {
      NotificationType::kDefaultBrowser,
      NotificationType::kWhatsNew,
      NotificationType::kSignin,
  };

  for (NotificationType type : kTypes) {
    if (ShouldSendNotification(type)) {
      RequestNotification(type);
      break;
    }
  }
}

void TipsNotificationClient::RequestNotification(NotificationType type) {
  UNUserNotificationCenter* notificationCenter =
      [UNUserNotificationCenter currentNotificationCenter];
  UNNotificationRequest* request = tips_notifications::Request(type);
  [notificationCenter
      addNotificationRequest:request
       withCompletionHandler:^(NSError* error){
           // TODO(crbug.com/1519157): Add logging if there is an
           // error.
       }];
}

bool TipsNotificationClient::ShouldSendNotification(NotificationType type) {
  switch (type) {
    case NotificationType::kDefaultBrowser:
      return !IsChromeLikelyDefaultBrowser();
    case NotificationType::kWhatsNew:
      return true;
    case NotificationType::kSignin:
      return true;
  }
}
