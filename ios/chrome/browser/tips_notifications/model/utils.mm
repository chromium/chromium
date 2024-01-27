// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/utils.h"

#import "base/time/time.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using tips_notifications::NotificationType;

// Holds the l10n string ids for tips notification content.
struct ContentIDs {
  int title;
  int body;
};

// Returns the ContentIDs for the given `type`.
ContentIDs ContentIDsForType(NotificationType type) {
  switch (type) {
    case NotificationType::kDefaultBrowser:
      return {IDS_IOS_NOTIFICATIONS_TIPS_DEFAULT_BROWSER_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_DEFAULT_BROWSER_BODY};
    case NotificationType::kWhatsNew:
      return {IDS_IOS_NOTIFICATIONS_TIPS_WHATS_NEW_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_WHATS_NEW_BODY};
    case NotificationType::kSignin:
      return {IDS_IOS_NOTIFICATIONS_TIPS_SIGNIN_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_SIGNIN_BODY};
  }
}

}  // namespace

namespace tips_notifications {

NSString* const kIdentifier = @"kTipsNotificationId";
NSString* const kNotificationTypeKey = @"kNotificationTypeKey";
const base::TimeDelta kDefaultTriggerDelta = base::Hours(72);

bool IsTipsNotification(UNNotificationRequest* request) {
  return [request.identifier isEqualToString:kIdentifier];
}

NSDictionary* UserInfoForType(NotificationType type) {
  return @{
    kIdentifier : @YES,
    kNotificationTypeKey : @(static_cast<int>(type)),
  };
}

std::optional<NotificationType> ParseType(UNNotificationRequest* request) {
  NSDictionary* user_info = request.content.userInfo;
  NSNumber* type = user_info[kNotificationTypeKey];
  if (type == nil) {
    return std::nullopt;
  }
  return static_cast<NotificationType>(type.integerValue);
}

UNNotificationRequest* Request(NotificationType type) {
  return [UNNotificationRequest requestWithIdentifier:kIdentifier
                                              content:ContentForType(type)
                                              trigger:Trigger()];
}

UNNotificationContent* ContentForType(NotificationType type) {
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  ContentIDs content_ids = ContentIDsForType(type);
  content.title = l10n_util::GetNSString(content_ids.title);
  content.body = l10n_util::GetNSString(content_ids.body);
  content.userInfo = UserInfoForType(type);
  return content;
}

UNNotificationTrigger* Trigger() {
  return [UNTimeIntervalNotificationTrigger
      triggerWithTimeInterval:kDefaultTriggerDelta.InSecondsF()
                      repeats:NO];
}

}  // namespace tips_notifications
