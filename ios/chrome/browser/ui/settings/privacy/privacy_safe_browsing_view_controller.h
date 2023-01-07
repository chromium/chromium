// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"

@class PrivacySafeBrowsingViewController;
@class TableViewItem;
@protocol PrivacySafeBrowsingViewControllerDelegate;

// Delegate for presentation events related to
// Privacy Safe Browsing View Controller.
@protocol PrivacySafeBrowsingViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)privacySafeBrowsingViewControllerDidRemove:
    (PrivacySafeBrowsingViewController*)controller;

@end

// View controller to related to Privacy safe browsing setting.
@interface PrivacySafeBrowsingViewController
    : SettingsRootTableViewController <PrivacySafeBrowsingConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak)
    id<PrivacySafeBrowsingViewControllerPresentationDelegate>
        presentationDelegate;
// Model delegate.
@property(nonatomic, weak) id<PrivacySafeBrowsingViewControllerDelegate>
    modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_VIEW_CONTROLLER_H_
