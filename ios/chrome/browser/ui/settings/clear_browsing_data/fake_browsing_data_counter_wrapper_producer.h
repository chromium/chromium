// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_FAKE_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_FAKE_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_

#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"

@interface FakeBrowsingDataCounterWrapperProducer
    : BrowsingDataCounterWrapperProducer

// Triggers the callback associated with the same prefName as
// `BrowsingDataCounter::Result.GetPrefName` with the `result`.
- (void)triggerUpdateUICallbackForResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_FAKE_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_
