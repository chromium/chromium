// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_CONTENT_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_CONTENT_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

class Browser;
@class ContentSettingsTableViewController;
class HostContentSettingsMap;
class MailtoHandlerService;
class PrefService;

// Delegate for presentation events related to
// ContentSettingsTableViewController.
@protocol ContentSettingsTableViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)contentSettingsTableViewControllerWasRemoved:
    (ContentSettingsTableViewController*)controller;

// Called when the Default Page Mode option is selected.
- (void)contentSettingsTableViewControllerSelectedDefaultPageMode:
    (ContentSettingsTableViewController*)controller;

// Called when the Web Inspector option is selected.
- (void)contentSettingsTableViewControllerSelectedWebInspector:
    (ContentSettingsTableViewController*)controller;

@end

// Controller for the UI that allows the user to change content settings like
// blocking popups.
@interface ContentSettingsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

@property(nonatomic, weak)
    id<ContentSettingsTableViewControllerPresentationDelegate>
        presentationDelegate;

// The designated initializer. `browser` must not be null.
- (instancetype)
    initWithHostContentSettingsMap:(HostContentSettingsMap*)settingsMap
              mailtoHandlerService:(MailtoHandlerService*)mailtoHandlerService
                       prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Stop observing any C++ objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_CONTENT_SETTINGS_TABLE_VIEW_CONTROLLER_H_
