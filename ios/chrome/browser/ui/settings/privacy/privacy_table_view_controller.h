// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class Browser;
@class PrivacyTableViewController;

// The accessibility identifier of the privacy settings collection view.
extern NSString* const kPrivacyTableViewId;

@protocol PrivacyNavigationCommands;
@protocol ReauthenticationProtocol;

// Delegate for presentation events related to
// PrivacyTableViewController.
@protocol PrivacyTableViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)privacyTableViewControllerWasRemoved:
    (PrivacyTableViewController*)controller;

@end

@interface PrivacyTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<PrivacyTableViewControllerPresentationDelegate>
    presentationDelegate;

// `profile` cannot be nil
- (instancetype)initWithBrowser:(Browser*)browser
         reauthenticationModule:(id<ReauthenticationProtocol>)reauthModule
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Handler used to navigate inside the privacy.
@property(nonatomic, weak) id<PrivacyNavigationCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_TABLE_VIEW_CONTROLLER_H_
