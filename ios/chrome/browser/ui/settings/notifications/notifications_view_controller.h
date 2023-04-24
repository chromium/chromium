// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/notifications/notifications_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"

@class NotificationsViewController;
@protocol NotificationsNavigationCommands;
@protocol NotificationsViewControllerDelegate;

// Delegate for presentation events related to
// NotificationsViewController.
@protocol NotificationsViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)notificationsViewControllerDidRemove:
    (NotificationsViewController*)controller;

@end

// View controller for Notifications setting.
@interface NotificationsViewController
    : SettingsRootTableViewController <NotificationsConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<NotificationsViewControllerPresentationDelegate>
    presentationDelegate;

// Delegate for view controller to send responses to model.
@property(nonatomic, weak) id<NotificationsViewControllerDelegate>
    modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_VIEW_CONTROLLER_H_
