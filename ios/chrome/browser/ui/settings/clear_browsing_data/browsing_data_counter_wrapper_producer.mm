// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

@implementation BrowsingDataCounterWrapperProducer {
  base::WeakPtr<ChromeBrowserState> _browserState;
}

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState->AsWeakPtr();
  }
  return self;
}

- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName
                    updateUiCallback:
                        (BrowsingDataCounterWrapper::UpdateUICallback)
                            updateUiCallback {
  ChromeBrowserState* browserState = _browserState.get();
  if (!browserState) {
    return nullptr;
  }

  PrefService* prefService = browserState->GetPrefs();
  if (!prefService) {
    return nullptr;
  }

  return BrowsingDataCounterWrapper::CreateCounterWrapper(
      prefName, browserState, prefService, updateUiCallback);
}

@end
