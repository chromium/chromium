// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_SCRIPT_BLOCKING_SCRIPT_BLOCKING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_SCRIPT_BLOCKING_SCRIPT_BLOCKING_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class ScriptBlockingViewController;

// Delegate for presentation events related to ScriptBlockingViewController.
@protocol ScriptBlockingViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)scriptBlockingViewControllerDidRemove:
    (ScriptBlockingViewController*)controller;

@end

// View controller for script blocking settings.
@interface ScriptBlockingViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<ScriptBlockingViewControllerPresentationDelegate>
    presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_SCRIPT_BLOCKING_SCRIPT_BLOCKING_VIEW_CONTROLLER_H_
