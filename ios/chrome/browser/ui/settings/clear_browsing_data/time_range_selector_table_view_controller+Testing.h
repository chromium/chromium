// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TABLE_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TABLE_VIEW_CONTROLLER_TESTING_H_

// Testing category exposing private methods of
// TimeRangeSelectorTableViewController for tests.
@interface TimeRangeSelectorTableViewController (Testing)
- (void)updatePrefValue:(int)prefValue;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TABLE_VIEW_CONTROLLER_TESTING_H_
