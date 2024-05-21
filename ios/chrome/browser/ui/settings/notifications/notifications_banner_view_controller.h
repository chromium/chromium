// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_BANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_BANNER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/notifications/notifications_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@class NotificationsBannerViewController;
@protocol NotificationsViewControllerDelegate;

// Delegate for presentation events related to
// NotificationsBannerViewController.
@protocol NotificationsBannerViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)notificationsBannerViewControllerDidRemove:
    (NotificationsBannerViewController*)controller;

@end

//  View controller for the notifications settings page.
@interface NotificationsBannerViewController
    : PromoStyleViewController <NotificationsConsumer,
                                SettingsControllerProtocol,
                                UIAdaptivePresentationControllerDelegate>

// Presentation delegate.
@property(nonatomic, weak)
    id<NotificationsBannerViewControllerPresentationDelegate>
        presentationDelegate;

// Delegate for view controller to send responses to model.
@property(nonatomic, weak) id<NotificationsViewControllerDelegate>
    modelDelegate;

// YES if Content Notifications are enabled.
@property(nonatomic, assign) BOOL isContentNotificationEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_BANNER_VIEW_CONTROLLER_H_
