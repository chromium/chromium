// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_consumer.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_item_identifier.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// Delegate for the NotificationsOptInViewController.
@protocol NotificationsOptInViewControllerDelegate <NSObject>

// Called when a notification opt-in toggle is switched on/off by the user.
- (void)selectionChangedForItemType:
            (NotificationsOptInItemIdentifier)itemIdentifier
                           selected:(BOOL)selected;

@end

//  View controller for the notifications opt-in screen.
@interface NotificationsOptInViewController
    : PromoStyleViewController <NotificationsOptInConsumer>

// Delegate for this ViewController.
@property(nonatomic, weak) id<NotificationsOptInViewControllerDelegate>
    notificationsDelegate;

// YES if Content Notifications are enabled.
@property(nonatomic, assign) BOOL isContentNotificationEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_VIEW_CONTROLLER_H_
