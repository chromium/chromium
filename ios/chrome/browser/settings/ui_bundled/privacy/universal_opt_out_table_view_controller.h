// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_UNIVERSAL_OPT_OUT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_UNIVERSAL_OPT_OUT_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

class ProfileIOS;

// The accessibility identifier of the Universal Opt Out table view.
extern NSString* const kUniversalOptOutTableViewAccessibilityIdentifier;

// The accessibility identifier of the Universal Opt Out switch.
extern NSString* const kUniversalOptOutSwitchAccessibilityIdentifier;

// The accessibility identifier of the Universal Opt Out error message item.
extern NSString* const kUniversalOptOutErrorMessageItemAccessibilityIdentifier;

// The accessibility identifier of the Universal Opt Out error Learn More item.
extern NSString* const
    kUniversalOptOutErrorLearnMoreItemAccessibilityIdentifier;

// This View Controller is responsible for managing the settings related to
// Universal Opt Out.
@interface UniversalOptOutTableViewController : SettingsRootTableViewController

// The designated initializer. `profile` must not be nil.
- (instancetype)initWithProfile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_UNIVERSAL_OPT_OUT_TABLE_VIEW_CONTROLLER_H_
