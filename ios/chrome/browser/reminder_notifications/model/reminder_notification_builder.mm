// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_builder.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "base/notreached.h"
#import "base/time/time.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/image/image.h"

namespace {

// The prefixed used for reminder notification identifiers.
NSString* const kReminderIdentifierPrefix = @"reminder_";

// Converts the given `time` to an `NSDateComponents` object.
NSDateComponents* TimeToDateComponents(base::Time time) {
  NSCalendarUnit unitFlags = NSCalendarUnitYear | NSCalendarUnitMonth |
                             NSCalendarUnitDay | NSCalendarUnitHour |
                             NSCalendarUnitMinute;
  NSDate* date = time.ToNSDate();
  NSCalendar* calendar = NSCalendar.currentCalendar;
  return [calendar components:unitFlags fromDate:date];
}

}  // namespace

@implementation ReminderNotificationBuilder {
  // Date components to use with a `UNCalendarNotificationTrigger`.
  NSDateComponents* _dateComponents;
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
  self = [super init];
  if (self) {
    _url = net::NSURLWithGURL(url);
    _dateComponents = TimeToDateComponents(time);
  }
  return self;
}

- (UNNotificationRequest*)buildRequest {
  CHECK(_dateComponents);
  CHECK(_url);
  return [UNNotificationRequest requestWithIdentifier:[self identifier]
                                              content:[self content]
                                              trigger:[self trigger]];
}

- (void)setImage:(const gfx::Image&)image {
  _image = image;
}

- (void)setPageTitle:(NSString*)pageTitle {
  _pageTitle = pageTitle;
}

#pragma mark - Helpers

// Returns the identifier for this notification.
- (NSString*)identifier {
  if (!_identifier) {
    _identifier = [kReminderIdentifierPrefix
        stringByAppendingString:[NSUUID UUID].UUIDString];
  }
  return _identifier;
}

// Returns the content for a reminder notification.
- (UNNotificationContent*)content {
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.title = l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_TITLE);
  content.body = _pageTitle ? _pageTitle : _url.host;
  content.userInfo = [self userInfo];
  content.interruptionLevel = UNNotificationInterruptionLevelTimeSensitive;
  content.sound = UNNotificationSound.defaultSound;
  if (!_image.IsEmpty()) {
    content.attachments = @[ [self imageAttachment] ];
  }
  return content;
}

// Returns a `UNNotificationTrigger` to be used when building the notification
// request.
- (UNCalendarNotificationTrigger*)trigger {
  return [UNCalendarNotificationTrigger
      triggerWithDateMatchingComponents:_dateComponents
                                repeats:NO];
}

// Returns an attachment to be included with the notification content.
- (UNNotificationAttachment*)imageAttachment {
  // TODO(crbug.com/392921766): Allow attaching image to reminder notification.
  NOTREACHED();
}

// Metadata to be included with the notification content.
- (NSDictionary*)userInfo {
  return @{
    @"url" : _url,
    @"page_title" : _pageTitle,
    @"date_scheduled" : NSDate.now,
  };
}

@end
