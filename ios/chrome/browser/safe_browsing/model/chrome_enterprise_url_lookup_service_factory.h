// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_

#import "components/safe_browsing/core/browser/realtime/chrome_enterprise_url_lookup_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace safe_browsing {

// Singleton that owns ChromeEnterpriseRealTimeUrlLookupService objects, one for
// each active profile. It returns nullptr for Incognito profiles.
class ChromeEnterpriseRealTimeUrlLookupServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ChromeEnterpriseRealTimeUrlLookupServiceFactory,
          ChromeEnterpriseRealTimeUrlLookupService> {
 public:
  explicit ChromeEnterpriseRealTimeUrlLookupServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_
