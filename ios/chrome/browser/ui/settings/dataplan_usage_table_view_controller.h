// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DATAPLAN_USAGE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DATAPLAN_USAGE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class PrefService;

// Class that handles the UI and preference setting for options that require
// three states:  'Always', 'Only on WiFi', and 'Never'.
@interface DataplanUsageTableViewController : SettingsRootTableViewController

- (instancetype)initWithPrefs:(PrefService*)prefs
                     basePref:(const char*)basePreference
                     wifiPref:(const char*)wifiPreference
                        title:(NSString*)title NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// Returns the text for the current setting, based on the values of the
// preferences.  Kept in this class, so that all of the code to translate from
// preferences to UI is in one place.
+ (NSString*)currentLabelForPreference:(PrefService*)prefs
                              basePref:(const char*)basePreference
                              wifiPref:(const char*)wifiPreference;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DATAPLAN_USAGE_TABLE_VIEW_CONTROLLER_H_
