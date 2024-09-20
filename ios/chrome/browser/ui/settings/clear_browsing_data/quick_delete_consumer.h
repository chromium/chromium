// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_CONSUMER_H_

#include "components/browsing_data/core/counters/browsing_data_counter.h"

namespace browsing_data {
enum class TimePeriod;
}

// Consumer for the QuickDeleteMediator to set the
// QuickDeleteViewController.
@protocol QuickDeleteConsumer <NSObject>

// Sets the ViewController with initial value for the deletion `timeRange`.
- (void)setTimeRange:(browsing_data::TimePeriod)timeRange;

// Sets the ViewController with the summary for the browsing data.
- (void)setBrowsingDataSummary:(NSString*)summary;

// Sets the boolean on whether the ViewController should show the disclaimer
// footer string or not.
- (void)setShouldShowFooter:(BOOL)shouldShowFooter;

// Sets the ViewController with the history summary.
- (void)setHistorySummary:(NSString*)historySummary;

// Sets the ViewController with the tabs summary.
- (void)setTabsSummary:(NSString*)tabsSummary;

// Sets the ViewController with the cache summary.
- (void)setCacheSummary:(NSString*)cacheSummary;

// Sets the ViewController with the passwords summary.
- (void)setPasswordsSummary:(NSString*)passwordsSummary;

// Sets the ViewController with the autofill summary.
- (void)setAutofillSummary:(NSString*)autofillSummary;

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
