// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_nau_configuration.h"

#import <UserNotifications/UserNotifications.h>

// Implementation class of the Notification Action Upload for
// Content Notifications.
@implementation ContentNotificationNAUConfiguration

// The ContentNotificationNAUConfiguration can have a `UNNotification` object or
// just a `UNNotificationContent` object. Every `UNNotification` has a
// `UNNotificationContent` contained in it. Content is the only part needed for
// NAUs, so whenever a full notification object is provided, the `content`
// property is populated for later reference.
- (UNNotificationContent*)content {
  if (_notification) {
    return _notification.request.content;
  } else {
    return _content;
  }
}

@end
