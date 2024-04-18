// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/fake_browsing_data_counter_wrapper_producer.h"

#import <string_view>

@implementation FakeBrowsingDataCounterWrapperProducer

- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName
                        browserState:(ChromeBrowserState*)browserState
                         prefService:(PrefService*)prefService
                    updateUiCallback:
                        (BrowsingDataCounterWrapper::UpdateUICallback)
                            updateUiCallback {
  return nullptr;
}

@end
