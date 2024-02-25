// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_item_identifier.h"

// Consumer protocol for the view controller that displays the notifications
// opt-in view.
@protocol NotificationsOptInConsumer <NSObject>

// Indicates to the consumer that the notification with identifier `identifier`
// should be set to `enabled`.
- (void)setOptInItem:(NotificationsOptInItemIdentifier)identifier
             enabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_CONSUMER_H_
