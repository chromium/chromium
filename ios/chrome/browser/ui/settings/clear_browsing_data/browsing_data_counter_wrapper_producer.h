// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_

#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"

// ClearBrowsingDataManager's dependency on creating BrowsingDataCounterWrapper
@interface BrowsingDataCounterWrapperProducer : NSObject

- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(base::StringPiece)prefName
                        browserState:(ios::ChromeBrowserState*)browserState
                         prefService:(PrefService*)prefService
                    updateUiCallback:
                        (BrowsingDataCounterWrapper::UpdateUICallback)
                            updateUiCallback;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_
