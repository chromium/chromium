// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/notifications/content_notifications/content_notifications_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"

@class ContentNotificationsViewController;
@protocol ContentNotificationsViewControllerDelegate;

// Delegate for presentation events related to
// ContentNotificationsViewController.
@protocol ContentNotificationsViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)contentNotificationsViewControllerDidRemove:
    (ContentNotificationsViewController*)controller;

@end

// View controller for Content Notification settings.
@interface ContentNotificationsViewController
    : SettingsRootTableViewController <ContentNotificationsConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak)
    id<ContentNotificationsViewControllerPresentationDelegate>
        presentationDelegate;

// Delegate for view controller to send responses to model.
@property(nonatomic, weak) id<ContentNotificationsViewControllerDelegate>
    modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_VIEW_CONTROLLER_H_
