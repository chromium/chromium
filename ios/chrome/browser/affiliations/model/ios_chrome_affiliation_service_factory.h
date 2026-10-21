// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AFFILIATIONS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AFFILIATIONS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_

#include "components/affiliations/core/browser/affiliation_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class IOSChromeAffiliationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeAffiliationServiceFactory,
          affiliations::AffiliationService> {
 public:
  explicit IOSChromeAffiliationServiceFactory(PassKey key);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AFFILIATIONS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_
