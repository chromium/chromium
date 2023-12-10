// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_BANDWIDTH_DATAPLAN_USAGE_TABLE_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_BANDWIDTH_DATAPLAN_USAGE_TABLE_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/prerender/model/prerender_pref.h"

// Testing category exposing private methods of DataplanUsageTableViewController
// for tests.
@interface DataplanUsageTableViewController (Testing)
- (void)updateSetting:(prerender_prefs::NetworkPredictionSetting)newSetting;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_BANDWIDTH_DATAPLAN_USAGE_TABLE_VIEW_CONTROLLER_TESTING_H_
