// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DEFAULT_BROWSER_DEFAULT_BROWSER_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DEFAULT_BROWSER_DEFAULT_BROWSER_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// Controller for the UI that shows the user how to set Chrome as the default
// browser and provides a button to take the user to the Settings app.
@interface DefaultBrowserSettingsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol,
                                       ConfirmationAlertActionHandler>

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// The feature that triggered this view controller.
@property(nonatomic, assign) DefaultBrowserSettingsPageSource source;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DEFAULT_BROWSER_DEFAULT_BROWSER_SETTINGS_TABLE_VIEW_CONTROLLER_H_
