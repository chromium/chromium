// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_builder.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "base/notreached.h"
#import "base/time/time.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/image/image.h"

NSString* const kReminderNotificationsIdentifierPrefix = @"reminder_";

@implementation ReminderNotificationBuilder {
  // The time the notification should be scheduled for.
  base::Time _time;
  // An optional image to attached (i.e. a favicon).
  gfx::Image _image;
  // The page's title, to include in the notification content.
  NSString* _pageTitle;
  // The page's URL.
  NSURL* _url;
  // An identifier for this notification request.
  NSString* _identifier;
}

#pragma mark - Public

- (instancetype)initWithURL:(const GURL&)url time:(base::Time)time {
  if ((self = [super init])) {
    _url = net::NSURLWithGURL(url);
    _time = time;
  }
  return self;
}

- (ScheduledNotificationRequest)buildRequest {
  base::TimeDelta time_interval = _time - base::Time::Now();

  if (time_interval.is_negative() || time_interval.is_zero()) {
    time_interval = base::Seconds(0);
  }

  return ScheduledNotificationRequest{.identifier = [self identifier],
                                      .content = [self content],
                                      .time_interval = time_interval};
}

- (void)setImage:(const gfx::Image&)image {
  _image = image;
}

- (void)setPageTitle:(NSString*)pageTitle {
  _pageTitle = [pageTitle copy];
}

#pragma mark - Helpers

// Returns the identifier for this notification.
- (NSString*)identifier {
  if (!_identifier) {
    _identifier = [kReminderNotificationsIdentifierPrefix
        stringByAppendingString:[NSUUID UUID].UUIDString];
  }
  return _identifier;
}

// Returns the content for a reminder notification.
- (UNNotificationContent*)content {
  UNMutableNotificationContent* mutableContent =
      [[UNMutableNotificationContent alloc] init];
  mutableContent.title =
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_TITLE);
  mutableContent.body = _pageTitle ? _pageTitle : _url.host;
  mutableContent.userInfo = [self userInfo];
  mutableContent.interruptionLevel =
      UNNotificationInterruptionLevelTimeSensitive;
  mutableContent.sound = UNNotificationSound.defaultSound;

  if (!_image.IsEmpty()) {
    // TODO(crbug.com/392921766): Implement image attachment.
    // mutableContent.attachments = @[ [self imageAttachment] ];
  }

  return mutableContent;
}

// Returns an attachment to be included with the notification content.
- (UNNotificationAttachment*)imageAttachment {
  // TODO(crbug.com/392921766): Allow attaching image to reminder notification.
  NOTREACHED();
}

// Metadata to be included with the notification content.
- (NSDictionary*)userInfo {
  NSMutableDictionary* userInfo = [NSMutableDictionary dictionary];

  userInfo[@"url"] = _url.absoluteString;

  if (_pageTitle) {
    userInfo[@"page_title"] = _pageTitle;
  }

  userInfo[@"notification_generation_time"] = NSDate.now;

  return userInfo;
}

@end
