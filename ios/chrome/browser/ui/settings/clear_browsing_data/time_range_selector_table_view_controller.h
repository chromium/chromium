// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class PrefService;

// Table view for time range selection.
@interface TimeRangeSelectorTableViewController
    : SettingsRootTableViewController

- (instancetype)initWithPrefs:(PrefService*)prefs NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Returns the text for the current setting, based on the values of the
// preference. Kept in this class, so that all of the code to translate from
// preference to UI is in one place.
+ (NSString*)timePeriodLabelForPrefs:(PrefService*)prefs;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TABLE_VIEW_CONTROLLER_H_
