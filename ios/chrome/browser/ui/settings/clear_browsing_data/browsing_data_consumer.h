// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_CONSUMER_H_

namespace browsing_data {
enum class TimePeriod;
}

// Consumer for the BrowsingDataMediator to update the
// QuickDeleteViewController.
@protocol BrowsingDataConsumer

// Sets the ViewController with initial value for the deletion `timeRange`.
- (void)setTimeRange:(browsing_data::TimePeriod)timeRange;

// Sets the ViewController with the summary for the browsing data.
- (void)setBrowsingDataSummary:(NSString*)summary;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_CONSUMER_H_
