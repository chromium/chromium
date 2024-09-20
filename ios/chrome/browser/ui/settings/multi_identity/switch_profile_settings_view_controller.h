// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol SwitchProfileSettingsDelegate;

// Controller for the UI that allows the user to switch from one profile to
// another.
@interface SwitchProfileSettingsTableViewController
    : SettingsRootTableViewController <SwitchProfileSettingsConsumer,
                                       UITableViewDelegate>

// InfobarTranslateModalDelegate for this ViewController.
@property(nonatomic, strong) id<SwitchProfileSettingsDelegate> delegate;

// The designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_VIEW_CONTROLLER_H_
