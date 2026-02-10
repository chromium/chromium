// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unified_consent/model/url_keyed_data_collection_consent_helper_ios.h"

#import "components/prefs/pref_service.h"
#import "components/unified_consent/url_keyed_data_collection_consent_helper.h"

UrlKeyedDataCollectionConsentHelperIOS::UrlKeyedDataCollectionConsentHelperIOS(
    PrefService* pref_service) {
  consent_helper_ = unified_consent::UrlKeyedDataCollectionConsentHelper::
      NewAnonymizedDataCollectionConsentHelper(pref_service);
}

UrlKeyedDataCollectionConsentHelperIOS::
    ~UrlKeyedDataCollectionConsentHelperIOS() = default;

bool UrlKeyedDataCollectionConsentHelperIOS::IsEnabled() {
  if (!consent_helper_) {
    return false;
  }
  return consent_helper_->IsEnabled();
}
