// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation BrowsingDataCounterWrapperProducer {
  base::WeakPtr<ProfileIOS> _profile;
}

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  self = [super init];
  if (self) {
    _profile = profile->AsWeakPtr();
  }
  return self;
}

- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName
                    updateUiCallback:
                        (BrowsingDataCounterWrapper::UpdateUICallback)
                            updateUiCallback {
  ProfileIOS* profile = _profile.get();
  if (!profile) {
    return nullptr;
  }

  PrefService* prefService = profile->GetPrefs();
  CHECK(prefService);

  return BrowsingDataCounterWrapper::CreateCounterWrapper(
      prefName, profile, prefService, updateUiCallback);
}

- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName
                           beginTime:(base::Time)beginTime
                    updateUiCallback:
                        (BrowsingDataCounterWrapper::UpdateUICallback)
                            updateUiCallback {
  ProfileIOS* profile = _profile.get();
  if (!profile) {
    return nullptr;
  }

  PrefService* prefService = profile->GetPrefs();
  CHECK(prefService);

  return BrowsingDataCounterWrapper::CreateCounterWrapper(
      prefName, profile, prefService, beginTime, updateUiCallback);
}

@end
