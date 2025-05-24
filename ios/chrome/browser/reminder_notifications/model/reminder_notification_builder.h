// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_BUILDER_H_
#define IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_BUILDER_H_

#import <UserNotifications/UserNotifications.h>

class GURL;
namespace base {
class Time;
}  // namespace base
namespace gfx {
class Image;
}  // namespace gfx

// Builds a `UNNotificationRequest` that will trigger a local notification to
// remind the user about a URL.
@interface ReminderNotificationBuilder : NSObject

// Initialize a builder for a reminder notification with the given `url` to
// trigger at the given `time`.
- (instancetype)initWithURL:(const GURL&)url time:(base::Time)time;

// Builds and returns a notification request.
- (UNNotificationRequest*)buildRequest;

// Sets an image (e.g. favicon) for the reminder.
- (void)setImage:(const gfx::Image&)image;

// Sets the page's title, which is displayed as part of the notification.
- (void)setPageTitle:(NSString*)string;

@end

#endif  // IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_BUILDER_H_
