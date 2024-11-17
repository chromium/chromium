// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/fake_browsing_data_counter_wrapper_producer.h"

#import <map>
#import <string_view>

@implementation FakeBrowsingDataCounterWrapperProducer {
  // Keeps track of the last callback associated with a pref.
  std::map<std::string, BrowsingDataCounterWrapper::UpdateUICallback>
      _prefsCallback;
}

- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName
                    updateUiCallback:
                        (BrowsingDataCounterWrapper::UpdateUICallback)
                            updateUiCallback {
  _prefsCallback.emplace(std::string(prefName), std::move(updateUiCallback));
  return nullptr;
}

- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName
                           beginTime:(base::Time)beginTime
                    updateUiCallback:
                        (BrowsingDataCounterWrapper::UpdateUICallback)
                            updateUiCallback {
  _prefsCallback.emplace(std::string(prefName), std::move(updateUiCallback));
  return nullptr;
}

- (void)triggerUpdateUICallbackForResult:
    (const browsing_data::BrowsingDataCounter::Result&)result {
  auto callback =
      _prefsCallback.find(std::string(result.source()->GetPrefName()));
  if (callback != _prefsCallback.end()) {
    callback->second.Run(result);
  }
}

@end
