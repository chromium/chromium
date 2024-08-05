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

// Updates the ViewController with the result of history counter.
- (void)updateHistoryWithResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

// Updates the ViewController with the result of cache counter.
- (void)updateTabsWithResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

// Updates the ViewController with the result of tabs counter.
- (void)updateCacheWithResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

// Updates the ViewController with the result of passwords counter.
- (void)updatePasswordsWithResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

// Updates the ViewController with the result of autofill counter.
- (void)updateAutofillWithResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

// Sets the boolean value for the history pref selection.
- (void)setHistorySelection:(BOOL)selected;

// Sets the boolean value for the tabs pref selection.
- (void)setTabsSelection:(BOOL)selected;

// Sets the boolean value for the site data pref selection.
- (void)setSiteDataSelection:(BOOL)selected;

// Sets the boolean value for the cache pref selection.
- (void)setCacheSelection:(BOOL)selected;

// Sets the boolean value for the passwords pref selection.
- (void)setPasswordsSelection:(BOOL)selected;

// Sets the boolean value for the autofill pref selection.
- (void)setAutofillSelection:(BOOL)selected;

// Shows a loading UI while the deletion is in progress.
- (void)deletionInProgress;

// Shows a confirmation UI after the deletion is finished.
- (void)deletionFinished;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_CONSUMER_H_
