// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_CONSUMER_H_

#include "components/browsing_data/core/counters/browsing_data_counter.h"

namespace browsing_data {
enum class TimePeriod;
}

// Consumer for the QuickDeleteMediator to update the
// QuickDeleteViewController.
@protocol QuickDeleteConsumer

// Sets the ViewController with initial value for the deletion `timeRange`.
- (void)setTimeRange:(browsing_data::TimePeriod)timeRange;

// Sets the ViewController with the summary for the browsing data.
- (void)setBrowsingDataSummary:(NSString*)summary;

// Sets the boolean on whether the ViewController should show the disclaimer
// footer string or not.
- (void)setShouldShowFooter:(BOOL)shouldShowFooter;

// Updates the ViewController with the result of browsing data counter.
- (void)updateAutofillWithResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

// Sets the boolean value for the autofill pref selection.
- (void)setAutofillSelection:(BOOL)selected;

// TODO(crbug.com/341107834): Add other browsing data type methods here.

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_CONSUMER_H_
