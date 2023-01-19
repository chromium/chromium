// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_BANDWIDTH_DATAPLAN_USAGE_TABLE_VIEW_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_BANDWIDTH_DATAPLAN_USAGE_TABLE_VIEW_CONTROLLER_PRIVATE_H_

// Class extension exposing private methods of DataplanUsageTableViewController
// for testing.
@interface DataplanUsageTableViewController ()
- (void)updateSetting:(prerender_prefs::NetworkPredictionSetting)newSetting;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_BANDWIDTH_DATAPLAN_USAGE_TABLE_VIEW_CONTROLLER_PRIVATE_H_
