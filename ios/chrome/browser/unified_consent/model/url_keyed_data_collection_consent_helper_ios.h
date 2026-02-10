// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_URL_KEYED_DATA_COLLECTION_CONSENT_HELPER_IOS_H_
#define IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_URL_KEYED_DATA_COLLECTION_CONSENT_HELPER_IOS_H_

#include <memory>

#import "components/keyed_service/core/keyed_service.h"

namespace unified_consent {
class UrlKeyedDataCollectionConsentHelper;
}

class PrefService;

class UrlKeyedDataCollectionConsentHelperIOS : public KeyedService {
 public:
  UrlKeyedDataCollectionConsentHelperIOS(PrefService* pref_service);
  ~UrlKeyedDataCollectionConsentHelperIOS() override;

  UrlKeyedDataCollectionConsentHelperIOS(
      const UrlKeyedDataCollectionConsentHelperIOS&) = delete;
  UrlKeyedDataCollectionConsentHelperIOS& operator=(
      const UrlKeyedDataCollectionConsentHelperIOS&) = delete;

  // Returns true if the user has consented for URL keyed anonymized data
  // collection.
  bool IsEnabled();

 private:
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      consent_helper_;
};

#endif  // IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_URL_KEYED_DATA_COLLECTION_CONSENT_HELPER_IOS_H_
