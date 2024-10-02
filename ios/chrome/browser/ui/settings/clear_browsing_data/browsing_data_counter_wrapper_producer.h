// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <string_view>

#import "ios/chrome/browser/browsing_data/model/browsing_data_counter_wrapper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// ClearBrowsingDataManager's dependency on creating BrowsingDataCounterWrapper
@interface BrowsingDataCounterWrapperProducer : NSObject

- (instancetype)initWithProfile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName
                    updateUiCallback:
                        (BrowsingDataCounterWrapper::UpdateUICallback)
                            updateUiCallback;

- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName
                           beginTime:(base::Time)beginTime
                    updateUiCallback:
                        (BrowsingDataCounterWrapper::UpdateUICallback)
                            updateUiCallback;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_COUNTER_WRAPPER_PRODUCER_H_
