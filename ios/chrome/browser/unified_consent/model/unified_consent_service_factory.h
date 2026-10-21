// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_UNIFIED_CONSENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_UNIFIED_CONSENT_SERVICE_FACTORY_H_

#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class UnifiedConsentServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          UnifiedConsentServiceFactory,
          unified_consent::UnifiedConsentService> {
 public:
  explicit UnifiedConsentServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_UNIFIED_CONSENT_SERVICE_FACTORY_H_
