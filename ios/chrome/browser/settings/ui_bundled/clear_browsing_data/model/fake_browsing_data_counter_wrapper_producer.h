// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_MODEL_FAKE_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_MODEL_FAKE_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_

#import <Foundation/Foundation.h>

#import <set>
#import <string>

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/model/browsing_data_counter_wrapper_producer.h"

@interface FakeBrowsingDataCounterWrapperProducer
    : BrowsingDataCounterWrapperProducer

// Returns the set of preference names for which a counter has been requested.
- (std::set<std::string>)registeredPrefNames;

// Triggers the callback associated with the same prefName as
// `BrowsingDataCounter::Result.GetPrefName` with the `result`.
- (void)triggerUpdateUICallbackForResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_MODEL_FAKE_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_
