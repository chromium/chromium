// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_consumer.h"
#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@class LockdownModeViewController;

// Delegate for presentation events related to LockdownMode View Controller
// which is usually handled by a class that holds the view controller.
@protocol LockdownModeViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)lockdownModeViewControllerDidRemove:
    (LockdownModeViewController*)controller;

@end

// View controller to related to Lockdown Mode setting.
@interface LockdownModeViewController
    : SettingsRootTableViewController <LockdownModeConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<LockdownModeViewControllerPresentationDelegate>
    presentationDelegate;
// Model delegate.
@property(nonatomic, weak) id<LockdownModeViewControllerDelegate> modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_VIEW_CONTROLLER_H_
