// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_MUTATOR_H_

namespace browsing_data {
enum class TimePeriod;
}

// Mutator for the QuickDeleteViewController to update the BrowsingDataMediator.
@protocol BrowsingDataMutator

// Called when the user selects a `timeRange` for the deletion of browsing data.
- (void)timeRangeSelected:(browsing_data::TimePeriod)timeRange;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_MUTATOR_H_
